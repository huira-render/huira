#include <cmath>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <optional>
#include <random>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include "catch2/catch_test_macros.hpp"
#include "catch2/matchers/catch_matchers_string.hpp"
#include "huira/huira.hpp"
#include "tle_test_support.hpp"

using namespace huira;
using namespace huira::test;
using namespace huira::units::literals;
using Catch::Matchers::ContainsSubstring;

namespace {

/// A satellite made from the ISS's element set, with another catalog number, and, if given,
/// another mean motion (rev/day, as columns 53-63 of line 2 write it) and mean anomaly (degrees,
/// columns 44-51).
SatRec iss_like(int catalog_number,
                std::string_view mean_motion = "15.72125391",
                std::string_view mean_anomaly = "325.0288")
{
    std::ostringstream number;
    number << std::setw(5) << std::setfill('0') << catalog_number;
    const std::string line1 = edit(ISS_LINE1, 3, number.str());
    const std::string line2 =
        edit(edit(edit(ISS_LINE2, 3, number.str()), 44, mean_anomaly), 53, mean_motion);
    return SatRec::from_tle(line1, line2);
}

/// Vallado's decaying satellite without its drag, which then stays in orbit: another satellite,
/// for its epoch.
SatRec staying_up()
{
    const std::string line1 =
        edit(edit(edit(DECAYING_LINE1, 3, "99001"), 34, " .00000000"), 54, " 00000-0");
    return SatRec::from_tle(line1, edit(DECAYING_LINE2, 3, "99001"));
}

/// Where a satellite is in the TEME frame at a time, in meters.
Vec3<double> teme_position(const SatRec& satrec, const Time& time)
{
    const std::optional<PositionState> state = satrec.evaluate(time);
    REQUIRE(state.has_value());
    return state->position;
}

/// A camera in the scene's TEME frame at a place, looking at a point (both in meters, in the TEME
/// frame): 512 by 512 pixels of 10 um behind a 1 m focal length, 1e-5 rad to a pixel.
InstanceHandle<RGB>
camera_at(Scene<RGB>& scene, const Vec3<double>& position, const Vec3<double>& target)
{
    CameraModelHandle<RGB> model = scene.new_camera_model("Camera");
    model.configure_sensor_from_pitch({512, 512}, 10_um);
    model.set_focal_length(1000_mm);
    model.enable_diffraction(false);
    model.enable_depth_of_field(false);

    InstanceHandle<RGB> camera = scene.teme_frame().new_instance(model);
    camera.set_position(
        units::Meter(position.x), units::Meter(position.y), units::Meter(position.z));
    const Vec3<double> z = glm::normalize(target - position);
    const Vec3<double> x = glm::normalize(glm::cross(Vec3<double>{0.0, 0.0, 1.0}, z));
    camera.set_rotation(Rotation<double>::from_basis_vectors(x, glm::cross(z, x), z));
    return camera;
}

/// The satellites of constellations a view collected, by catalog number.
const UnresolvedInstance<RGB>* satellite_in(const SceneView<RGB>& view, int catalog_number)
{
    for (const UnresolvedInstance<RGB>& instance : view.unresolved_instances()) {
        if (instance.constellation_id != NO_OBJECT_ID &&
            instance.catalog_number == catalog_number) {
            return &instance;
        }
    }
    return nullptr;
}

/// A file of text, deleted when it goes.
class TemporaryFile {
  public:
    explicit TemporaryFile(const std::string& text)
    {
        std::random_device random;
        path_ = std::filesystem::temp_directory_path() /
                ("huira_test_constellation_" + std::to_string(random()) + ".tle");
        std::ofstream(path_) << text;
    }
    ~TemporaryFile()
    {
        std::error_code ignored;
        std::filesystem::remove(path_, ignored);
    }
    TemporaryFile(const TemporaryFile&) = delete;
    TemporaryFile& operator=(const TemporaryFile&) = delete;

    const std::filesystem::path& path() const { return path_; }

  private:
    std::filesystem::path path_;
};

} // namespace

TEST_CASE("A constellation holds one element set for each satellite", "[scene][constellation]")
{
    Scene<RGB> scene;
    ConstellationHandle<RGB> constellation =
        scene.new_constellation(scene.new_unresolved_object_from_magnitude(5.0), "Active");
    CHECK(constellation.size() == 0);
    CHECK(scene.get_constellation("Active").id() == constellation.id());

    ConstellationReport report = constellation.add({iss(), iss_like(90001)});
    CHECK(report.added == 2);
    CHECK(report.replaced == 0);
    CHECK(report.skipped.empty());
    CHECK(constellation.size() == 2);
    CHECK(constellation.contains(25544));
    CHECK(constellation.contains(90001));
    CHECK_FALSE(constellation.contains(90002));
    CHECK(constellation.element_sets().size() == 2);

    // A newer element set replaces the one it holds
    report = constellation.add({iss_a_day_later()});
    CHECK(report.added == 0);
    CHECK(report.replaced == 1);
    CHECK(constellation.size() == 2);
    CHECK(constellation.element_set(25544).epoch() == iss_a_day_later().epoch());
    CHECK_THROWS_WITH(constellation.element_set(90002),
                      ContainsSubstring("doesn't hold satellite 90002"));

    CHECK(constellation.remove(25544));
    CHECK_FALSE(constellation.remove(25544));
    CHECK(constellation.size() == 1);
    CHECK(constellation.element_set(90001).catalog_number() == 90001);
    constellation.clear();
    CHECK(constellation.size() == 0);
}

TEST_CASE("A satellite is placed once, by a node or as one of a constellation's",
          "[scene][constellation]")
{
    Scene<RGB> scene;
    const UnresolvedObjectHandle<RGB> object = scene.new_unresolved_object_from_magnitude(5.0);
    ConstellationHandle<RGB> first = scene.new_constellation(object, "First");
    ConstellationHandle<RGB> second = scene.new_constellation(object, "Second");
    const FrameHandle<RGB> node = scene.teme_frame().new_subframe();

    first.add({iss()});

    // Another constellation leaves it out, and says what places it
    ConstellationReport report = second.add({iss_like(90001), iss()});
    CHECK(report.added == 1);
    REQUIRE(report.skipped.size() == 1);
    CHECK(report.skipped[0].reason == SkipReason::CLAIMED);
    CHECK(report.skipped[0].catalog_number == 25544);
    CHECK(report.skipped[0].entry == 1); // Its place in the list
    CHECK(report.skipped[0].line == 0);
    CHECK_THAT(report.skipped[0].message, ContainsSubstring(first.get()->get_info()));

    // A node can't be placed by it
    CHECK_THROWS_WITH(node.set_tle_position(iss()),
                      ContainsSubstring("cannot be placed by satellite 25544's TLE: it is in " +
                                        first.get()->get_info()));
    CHECK(node.get()->get_position_mode() == TransformMode::MANUAL_TRANSFORM);

    SECTION("Until it is removed")
    {
        first.remove(25544);
        CHECK_NOTHROW(node.set_tle_position(iss()));
    }

    SECTION("Until its constellation is cleared")
    {
        first.clear();
        CHECK(second.add({iss()}).added == 1);
    }

    SECTION("Until its constellation is deleted")
    {
        scene.delete_constellation(first);
        CHECK_FALSE(first.valid());
        CHECK_NOTHROW(node.set_tle_position(iss()));
    }

    SECTION("A node's satellite is left out of a constellation")
    {
        first.remove(25544);
        node.set_tle_position(iss());
        report = first.add({iss()});
        CHECK(report.added == 0);
        REQUIRE(report.skipped.size() == 1);
        CHECK_THAT(report.skipped[0].message,
                   ContainsSubstring("its satellite is placed by " + node.get()->get_info()));

        // Until the node lets it go
        scene.teme_frame().delete_subframe(node);
        CHECK(first.add({iss()}).added == 1);
    }
}

TEST_CASE("A constellation loads catalogs, and reports what it left out", "[scene][constellation]")
{
    Scene<RGB> scene;
    ConstellationHandle<RGB> constellation =
        scene.new_constellation(scene.new_unresolved_object_from_magnitude(5.0));
    scene.teme_frame().new_subframe().set_tle_position(iss_like(90002));

    // The ISS, an entry with a wrong checksum, and a satellite a node places
    const SatRec claimed = iss_like(90002);
    const std::string text =
        "ISS (ZARYA)\n" + std::string(ISS_LINE1) + "\n" + std::string(ISS_LINE2) + "\n" +
        edit(ISS_LINE1, 3, "90001").substr(0, 68) + "0\n" + edit(ISS_LINE2, 3, "90001") + "\n" +
        edit(ISS_LINE1, 3, "90002") + "\n" + edit(ISS_LINE2, 3, "90002") + "\n";
    const TemporaryFile file(text);

    ConstellationReport report = constellation.load(file.path());
    CHECK(report.added == 1);
    CHECK(constellation.contains(25544));
    REQUIRE(report.skipped.size() == 2);
    CHECK(report.skipped[0].reason == SkipReason::INVALID);
    CHECK(report.skipped[0].line == 4);
    CHECK(report.skipped[1].reason == SkipReason::CLAIMED);
    CHECK(report.skipped[1].catalog_number == 90002);
    CHECK(report.skipped[1].entry == 2); // Where it is in the catalog
    CHECK(report.skipped[1].line == 6);

    // Loaded again, as a newer catalog would be, the satellite it holds is brought up to date, and
    // the options apply
    report = constellation.load(file.path(), ElementSetFormat::TLE, {.exclude = {90001}});
    CHECK(report.added == 0);
    CHECK(report.replaced == 1);
    REQUIRE(report.skipped.size() == 2);
    CHECK(report.skipped[0].reason == SkipReason::EXCLUDED);
    CHECK(report.skipped[1].reason == SkipReason::CLAIMED);
}

TEST_CASE("A constellation's satellites are seen where nodes placed by their TLEs are",
          "[scene][constellation]")
{
    TestEarthEphemeris::load();
    const SatRec satrec = iss();
    const Time middle = satrec.epoch() + units::Second(600.0);
    const Vec3<double> target = teme_position(satrec, middle);
    const Vec3<double> camera_position = target + glm::normalize(Vec3<double>{1.0, 2.0, 3.0}) * 1e6;

    // The ISS seen from 1000 km, crossing the frame at about 7.6 mrad/s: some 40 pixels in 50 ms
    const Interval exposure = Interval::from_centered(middle, 50_ms);

    for (const ObservationMode mode : {ObservationMode::TRUE_STATE,
                                       ObservationMode::GEOMETRIC_STATE,
                                       ObservationMode::ABERRATED_STATE}) {
        for (const std::optional<std::size_t> samples :
             {std::optional<std::size_t>{}, std::optional<std::size_t>{5}}) {
            INFO("Mode " << static_cast<int>(mode) << ", samples "
                         << (samples ? std::to_string(*samples) : "chosen"));

            Scene<RGB> by_node;
            const InstanceHandle<RGB> node = by_node.teme_frame().new_instance(
                by_node.new_unresolved_object_from_magnitude(5.0));
            node.set_tle_position(satrec);
            const SceneView<RGB> node_view(
                by_node, exposure, camera_at(by_node, camera_position, target), mode, samples);

            Scene<RGB> by_constellation;
            by_constellation
                .new_constellation(by_constellation.new_unresolved_object_from_magnitude(5.0))
                .add({satrec});
            const SceneView<RGB> constellation_view(
                by_constellation,
                exposure,
                camera_at(by_constellation, camera_position, target),
                mode,
                samples);

            REQUIRE(node_view.unresolved_instances().size() == 1);
            const UnresolvedInstance<RGB>& expected = node_view.unresolved_instances().front();
            const UnresolvedInstance<RGB>* seen = satellite_in(constellation_view, 25544);
            REQUIRE(seen != nullptr);
            CHECK(seen->instance_id == NO_OBJECT_ID);
            CHECK(seen->unresolved_object != nullptr);

            // The same samples, at the same times, and each satellite where its node is, to the
            // precision of a position in single precision 1000 km away (0.06 m)
            REQUIRE(seen->transforms.size() == expected.transforms.size());
            CHECK(seen->transforms.size() == (samples ? *samples : seen->transforms.size()));
            CHECK(seen->transforms.size() > 1);
            for (std::size_t i = 0; i < expected.transforms.size(); ++i) {
                CHECK(seen->times[i] == expected.times[i]);
                CHECK(glm::length(Vec3<double>{seen->transforms[i].position} -
                                  Vec3<double>{expected.transforms[i].position}) < 0.2);
            }
            CHECK(seen->irradiances == expected.irradiances);
        }
    }

    // Light time moves it back along its path relative to the barycenter, by its velocity, some
    // 37 km/s with the Earth's, over the 3.3 ms its light takes: about 120 m. Aberration turns it
    // by the camera's velocity, the Earth's, about 30 km/s, over the speed of light: 1e-4 rad,
    // some 100 m at 1000 km, nearly undoing the light time's share of the Earth's motion.
    const auto seen_at = [&](ObservationMode mode) {
        Scene<RGB> scene;
        scene.new_constellation(scene.new_unresolved_object_from_magnitude(5.0)).add({satrec});
        const SceneView<RGB> view(
            scene, exposure, camera_at(scene, camera_position, target), mode, std::size_t{1});
        return Vec3<double>{satellite_in(view, 25544)->transforms.front().position};
    };
    const double light_time = 1e6 / SPEED_OF_LIGHT<double>();
    const double speed = glm::length(expected_state(satrec, middle).second);
    const double light_time_shift = glm::length(seen_at(ObservationMode::GEOMETRIC_STATE) -
                                                seen_at(ObservationMode::TRUE_STATE));
    const double aberration_shift = glm::length(seen_at(ObservationMode::ABERRATED_STATE) -
                                                seen_at(ObservationMode::GEOMETRIC_STATE));
    CHECK(std::abs(light_time_shift - speed * light_time) < 0.01 * speed * light_time);
    CHECK(aberration_shift > 50.0);
    CHECK(aberration_shift < 150.0);
}

TEST_CASE("A constellation's satellites each have the temporal samples their motion needs",
          "[scene][constellation]")
{
    TestEarthEphemeris::load();

    // From the Earth's center, for 10 ms, the ISS crosses some 1.1 pixels, and a satellite in the
    // geostationary belt, in the same direction, 0.07: less than the tolerance, 0.1 pixels
    const SatRec low = iss();
    const SatRec high = iss_like(90003, " 1.00270000");
    const Time middle = low.epoch() + units::Second(1.0);

    Scene<RGB> scene;
    scene.new_constellation(scene.new_unresolved_object_from_magnitude(5.0)).add({low, high});
    const InstanceHandle<RGB> camera =
        camera_at(scene, Vec3<double>{0.0}, teme_position(low, middle));
    const SceneView<RGB> view(scene, Interval::from_centered(middle, 10_ms), camera);

    const UnresolvedInstance<RGB>* moving = satellite_in(view, 25544);
    const UnresolvedInstance<RGB>* still = satellite_in(view, 90003);
    REQUIRE(moving != nullptr);
    REQUIRE(still != nullptr);
    CHECK(moving->transforms.size() > 1);
    CHECK(still->transforms.size() == 1);
    CHECK(still->times.front() == middle);
    CHECK(view.max_temporal_samples() == moving->transforms.size());
}

TEST_CASE("A satellite SGP4 can't place during the exposure is left out of the view",
          "[scene][constellation]")
{
    TestEarthEphemeris::load();
    Scene<RGB> scene;
    ConstellationHandle<RGB> constellation =
        scene.new_constellation(scene.new_unresolved_object_from_magnitude(5.0), "Low");
    constellation.add({decaying(), staying_up()});

    // Decayed, eight hours from its epoch; the other, without its drag, stays up
    const Time later = decaying().epoch() + units::Second(8.0 * 3600.0);
    const InstanceHandle<RGB> camera =
        camera_at(scene, Vec3<double>{0.0}, teme_position(staying_up(), later));
    const SceneView<RGB> view(scene, Interval::from_centered(later, 1_ms), camera);

    CHECK(satellite_in(view, 99001) != nullptr);
    CHECK(satellite_in(view, 29141) == nullptr);
    REQUIRE(view.skipped_satellites().size() == 1);
    const SkippedSatellite& skipped = view.skipped_satellites().front();
    CHECK(skipped.constellation_id == constellation.id());
    CHECK(skipped.catalog_number == 29141);
    CHECK_THAT(skipped.message,
               ContainsSubstring("SGP4 can't place it at") &&
                   ContainsSubstring("0.3 days from its element set's epoch") &&
                   ContainsSubstring("SGP4 error 6"));

    // Earlier, both are placed
    const SceneView<RGB> earlier(
        scene, Interval::from_centered(decaying().epoch() + units::Second(600.0), 1_ms), camera);
    CHECK(earlier.skipped_satellites().empty());
}

TEST_CASE("A view leaves out satellites behind the camera, and hidden constellations'",
          "[scene][constellation]")
{
    TestEarthEphemeris::load();
    const SatRec satrec = iss();
    const Time middle = satrec.epoch() + units::Second(600.0);
    const Vec3<double> target = teme_position(satrec, middle);
    const Interval exposure = Interval::from_centered(middle, 1_ms);

    Scene<RGB> scene;
    const UnresolvedObjectHandle<RGB> object = scene.new_unresolved_object_from_magnitude(5.0);
    ConstellationHandle<RGB> constellation = scene.new_constellation(object);
    constellation.add({satrec});

    // Looking away from it
    const SceneView<RGB> away(scene, exposure, camera_at(scene, Vec3<double>{0.0}, -target));
    CHECK(away.unresolved_instances().empty());
    CHECK(away.skipped_satellites().empty());

    // At it, but hidden
    const InstanceHandle<RGB> camera = camera_at(scene, Vec3<double>{0.0}, target);
    CHECK(constellation.is_visible());
    constellation.set_visible(false);
    CHECK_FALSE(constellation.is_visible());
    const SceneView<RGB> hidden(scene, exposure, camera);
    CHECK(hidden.unresolved_instances().empty());

    constellation.set_visible(true);
    const SceneView<RGB> shown(scene, exposure, camera);
    CHECK(shown.unresolved_instances().size() == 1);
}

TEST_CASE("A constellation's satellites are seen as its unresolved object",
          "[scene][constellation]")
{
    Scene<RGB> scene;
    const UnresolvedObjectHandle<RGB> faint = scene.new_unresolved_object_from_magnitude(10.0);
    const UnresolvedObjectHandle<RGB> bright = scene.new_unresolved_object_from_magnitude(5.0);
    ConstellationHandle<RGB> constellation = scene.new_constellation(faint);
    CHECK(constellation.get_unresolved_object().id() == faint.id());
    constellation.set_unresolved_object(bright);
    CHECK(constellation.get_unresolved_object().id() == bright.id());

    // Deleted with it
    constellation.add({iss()});
    scene.delete_unresolved_object(bright);
    CHECK_FALSE(constellation.valid());
    CHECK_NOTHROW(scene.teme_frame().new_subframe().set_tle_position(iss()));
}

TEST_CASE("A constellation's satellites are drawn as nodes placed by their TLEs are",
          "[scene][constellation]")
{
    TestEarthEphemeris::load();
    const SatRec satrec = iss();
    const Time middle = satrec.epoch() + units::Second(600.0);
    const Vec3<double> target = teme_position(satrec, middle);
    const Vec3<double> camera_position = target + glm::normalize(Vec3<double>{1.0, 2.0, 3.0}) * 1e6;
    const Interval exposure = Interval::from_centered(middle, 5_ms);

    // Where its light lands, and how much of it, in a scene that places it by a node or as one of a
    // constellation's (with another satellite, out of view across the Earth)
    struct Drawn {
        Vec2<double> centroid{0.0};
        double power = 0.0;
    };
    const auto drawn = [&](bool by_constellation) {
        Scene<RGB> scene;
        const UnresolvedObjectHandle<RGB> object =
            scene.new_unresolved_object(units::WattsPerMeterSquared(1e-10));
        if (by_constellation) {
            scene.new_constellation(object).add(
                {satrec, iss_like(90001, "15.72125391", "145.0288")});
        } else {
            scene.teme_frame().new_instance(object).set_tle_position(satrec);
        }
        const InstanceHandle<RGB> camera = camera_at(scene, camera_position, target);
        SceneView<RGB> view(scene, exposure, camera);
        // The other is in front of the camera, but far out of the frame
        CHECK(view.unresolved_instances().size() == (by_constellation ? 2 : 1));

        FrameBuffer<RGB> frame_buffer = scene.get_camera_model("Camera").make_frame_buffer();
        frame_buffer.enable_received_power();
        Renderer<RGB> renderer;
        renderer.render(view, frame_buffer);
        const Image<RGB>& image = frame_buffer.received_power();

        Drawn result;
        Vec2<double> moment{0.0};
        for (int y = 0; y < image.height(); ++y) {
            for (int x = 0; x < image.width(); ++x) {
                const double power = image(x, y).total();
                result.power += power;
                moment += power * Vec2<double>{x, y};
            }
        }
        REQUIRE(result.power > 0.0);
        result.centroid = moment / result.power;
        return result;
    };

    // It crosses some 4 pixels in 5 ms, near the frame's middle: 10 pixels from it, as the
    // camera's velocity, the Earth's, aberrates it by 1e-4 rad
    const Drawn by_node = drawn(false);
    const Drawn by_constellation = drawn(true);
    CHECK(std::abs(by_node.centroid.x - 255.5) < 20.0);
    CHECK(std::abs(by_node.centroid.y - 255.5) < 20.0);
    CHECK(std::abs(by_constellation.centroid.x - by_node.centroid.x) < 1e-3);
    CHECK(std::abs(by_constellation.centroid.y - by_node.centroid.y) < 1e-3);
    CHECK(std::abs(by_constellation.power - by_node.power) < 1e-6 * by_node.power);
}
