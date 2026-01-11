#include <type_traits>

#include "catch2/catch_test_macros.hpp"

#include "huira/math/constants.hpp"
#include "huira/units/units.hpp"

using namespace huira;
using namespace huira::literals;

// Helper for floating point comparisons
constexpr double EPSILON = 1e-10;

TEST_CASE("Explicit constructors prevent implicit conversions", "[constructors]") {
    SECTION("Cannot implicitly construct from numeric types") {
        // These should NOT compile (commented out to prevent compilation errors in tests)
        // Meter m = 5.0;  // Should fail
        // Degree d = 45;  // Should fail

        // This is the only way (explicit construction)
        Meter m{ 5.0 };
        Degree d{ 45 };

        REQUIRE(m.value() == 5.0);
        REQUIRE(d.value() == 45.0);
    }

    SECTION("User-defined literals work") {
        auto m = 5.0_m;
        auto d = 45.0_deg;

        REQUIRE(m.value() == 5.0);
        REQUIRE(d.value() == 45.0);
    }

    SECTION("Explicit keyword is present") {
        // This is a compile-time check via type traits
        // If constructor were implicit, this would fail
        REQUIRE(!std::is_convertible_v<double, Meter>);
        REQUIRE(!std::is_convertible_v<int, Degree>);
    }
}

TEST_CASE("Basic unit construction and value retrieval", "[construction]") {
    SECTION("Length units") {
        Meter m{ 100 };
        REQUIRE(m.value() == 100.0);
        REQUIRE(m.getSIValue() == 100.0);

        Kilometer km{ 1 };
        REQUIRE(km.value() == 1.0);
        REQUIRE(km.getSIValue() == 1000.0);

        Millimeter mm{ 1000 };
        REQUIRE(mm.value() == 1000.0);
        REQUIRE(mm.getSIValue() == 1.0);
    }

    SECTION("Angle units") {
        Radian rad{ 1.0 };
        REQUIRE(rad.value() == 1.0);
        REQUIRE(rad.getSIValue() == 1.0);

        Degree deg{ 180 };
        REQUIRE(deg.value() == 180.0);
        REQUIRE(std::abs(deg.getSIValue() - PI<double>()) < EPSILON);
    }

    SECTION("Time units") {
        Second s{ 60 };
        REQUIRE(s.value() == 60.0);
        REQUIRE(s.getSIValue() == 60.0);

        Minute min{ 1 };
        REQUIRE(min.value() == 1.0);
        REQUIRE(min.getSIValue() == 60.0);

        Hour h{ 1 };
        REQUIRE(h.value() == 1.0);
        REQUIRE(h.getSIValue() == 3600.0);
    }
}

TEST_CASE("Unit conversions with as<>()", "[conversion]") {
    SECTION("Length conversions") {
        Meter m{ 1000 };
        auto km = m.as<Kilometer>();

        REQUIRE(km.value() == 1.0);
        REQUIRE(km.getSIValue() == 1000.0);

        auto mm = m.as<Millimeter>();
        REQUIRE(mm.value() == 1000000.0);
    }

    SECTION("Angle conversions") {
        Degree deg{ 180 };
        auto rad = deg.as<Radian>();

        REQUIRE(std::abs(rad.value() - PI<double>()) < EPSILON);

        Radian r{ PI<double>() / 2 };
        auto d = r.as<Degree>();
        REQUIRE(std::abs(d.value() - 90.0) < EPSILON);
    }

    SECTION("Temperature conversions") {
        Celsius c{ 0 };
        auto k = c.as<Kelvin>();

        REQUIRE(std::abs(k.value() - 273.15) < EPSILON);

        Celsius room{ 20 };
        auto f = room.as<Fahrenheit>();
        REQUIRE(std::abs(f.value() - 68.0) < EPSILON);
    }

    SECTION("Round-trip conversions preserve value") {
        Meter original{ 1234.56 };
        auto km = original.as<Kilometer>();
        auto back = km.as<Meter>();

        REQUIRE(std::abs(back.value() - original.value()) < EPSILON);
    }
}

TEST_CASE("Same-scale arithmetic", "[arithmetic][same-scale]") {
    SECTION("Addition") {
        Meter a{ 100 };
        Meter b{ 50 };
        auto c = a + b;

        REQUIRE(c.value() == 150.0);
        REQUIRE(c.getSIValue() == 150.0);
    }

    SECTION("Subtraction") {
        Degree a{ 90 };
        Degree b{ 45 };
        auto c = a - b;

        REQUIRE(c.value() == 45.0);
    }

    SECTION("Compound assignment") {
        Meter m{ 100 };
        m += Meter{ 50 };
        REQUIRE(m.value() == 150.0);

        m -= Meter{ 25 };
        REQUIRE(m.value() == 125.0);
    }
}

TEST_CASE("Mixed-scale arithmetic preserves left-hand scale", "[arithmetic][mixed-scale]") {
    SECTION("Addition preserves LHS scale") {
        Degree a{ 45 };
        Degree b{ 45 };
        auto c = a + b;

        // Result should be in Degrees (LHS), not Radians
        REQUIRE(c.value() == 90.0);
        REQUIRE(std::abs(c.getSIValue() - PI<double>() / 2) < EPSILON);
    }

    SECTION("Subtraction preserves LHS scale") {
        Kilometer km{ 1 };
        Meter m{ 500 };
        auto result = km - m;

        // Result should be in Kilometers (LHS)
        REQUIRE(std::abs(result.value() - 0.5) < EPSILON);
    }

    SECTION("Different scales mixed") {
        Meter m{ 1000 };
        Kilometer km{ 1 };
        auto sum = m + km;

        // Should be in Meters (LHS)
        REQUIRE(sum.value() == 2000.0);

        auto sum2 = km + m;
        // Should be in Kilometers (LHS)
        REQUIRE(std::abs(sum2.value() - 2.0) < EPSILON);
    }
}

TEST_CASE("Scalar multiplication and division", "[arithmetic][scalar]") {
    SECTION("Multiplication") {
        Meter m{ 10 };
        auto result = m * 5.0;
        REQUIRE(result.value() == 50.0);

        auto result2 = 5.0 * m;
        REQUIRE(result2.value() == 50.0);
    }

    SECTION("Division") {
        Meter m{ 100 };
        auto result = m / 4.0;
        REQUIRE(result.value() == 25.0);
    }

    SECTION("Compound assignment") {
        Watt w{ 100 };
        w *= 2.0;
        REQUIRE(w.value() == 200.0);

        w /= 4.0;
        REQUIRE(w.value() == 50.0);
    }
}

TEST_CASE("Quantity multiplication creates composite units", "[arithmetic][multiplication]") {
    SECTION("Length * Length = Area") {
        Meter l{ 5 };
        Meter w{ 3 };
        auto area = l * w;

        REQUIRE(area.value() == 15.0);
        REQUIRE(area.getSIValue() == 15.0);
    }

    SECTION("Distance / Time = Speed") {
        Meter dist{ 100 };
        Second time{ 10 };
        auto speed = dist / time;

        REQUIRE(speed.value() == 10.0);  // 10 m/s
    }

    SECTION("Force * Length = Energy") {
        Newton force{ 50 };
        Meter distance{ 2 };
        auto energy = force * distance;

        // Result should be in Joules (N * m)
        REQUIRE(energy.getSIValue() == 100.0);
    }

    SECTION("Power * Time = Energy") {
        Watt power{ 100 };
        Second time{ 10 };
        auto energy = power * time;

        REQUIRE(energy.getSIValue() == 1000.0);  // Joules
    }

    SECTION("Mixed scale multiplication") {
        Kilometer km{ 5 };
        Hour h{ 2 };
        auto speed = km / h;

        // Should be 5km / 2h in the composed scale
        REQUIRE(std::abs(speed.value() - 2.5) < EPSILON);

        // SI value should be m/s
        REQUIRE(std::abs(speed.getSIValue() - (5000.0 / 7200.0)) < EPSILON);
    }
}

TEST_CASE("Dimensionless quantities", "[dimensionless]") {
    SECTION("Same dimension division creates dimensionless") {
        Meter a{ 100 };
        Meter b{ 50 };
        auto ratio = a / b;

        // Should be able to implicitly convert to double
        double value = ratio;
        REQUIRE(value == 2.0);
    }

    SECTION("Dimensionless can still be used as Quantity") {
        auto ratio = Meter{ 100 } / Meter{ 50 };

        REQUIRE(ratio.value() == 2.0);
        REQUIRE(ratio.getSIValue() == 2.0);
    }

    SECTION("Dimensionless arithmetic") {
        auto r1 = Meter{ 100 } / Meter{ 50 };
        auto r2 = Meter{ 200 } / Meter{ 100 };

        auto sum = r1 + r2;
        double value = sum;
        REQUIRE(value == 4.0);
    }
}

TEST_CASE("Comparison operators", "[comparison]") {
    SECTION("Same scale comparisons") {
        Meter a{ 100 };
        Meter b{ 50 };
        Meter c{ 100 };

        REQUIRE(a > b);
        REQUIRE(b < a);
        REQUIRE(a >= c);
        REQUIRE(a <= c);
        REQUIRE(a == c);
        REQUIRE(a != b);
    }

    SECTION("Different scale comparisons") {
        Meter m{ 1000 };
        Kilometer km{ 1 };

        REQUIRE(m == km);  // Same SI value
        REQUIRE(m >= km);
        REQUIRE(m <= km);

        Meter m2{ 1001 };
        REQUIRE(m2 > km);
        REQUIRE(km < m2);
    }

    SECTION("Angle comparisons") {
        Degree d{ 180 };
        Radian r{ PI<double>() };

        REQUIRE(d == r);  // Both are pi radians

        Degree d2{ 90 };
        REQUIRE(d > d2);
        REQUIRE(r > d2);
    }
}

TEST_CASE("User-defined literals", "[literals]") {
    SECTION("Length literals") {
        auto km = 5.5_km;
        auto m = 100.0_m;
        auto cm = 50.0_cm;
        auto mm = 25.0_mm;

        REQUIRE(km.value() == 5.5);
        REQUIRE(m.value() == 100.0);
        REQUIRE(cm.value() == 50.0);
        REQUIRE(mm.value() == 25.0);
    }

    SECTION("Time literals") {
        auto h = 2.0_h;
        auto min = 30.0_min;
        auto s = 45.0_s;
        auto ms = 500.0_ms;

        REQUIRE(h.getSIValue() == 7200.0);
        REQUIRE(min.getSIValue() == 1800.0);
        REQUIRE(s.getSIValue() == 45.0);
        REQUIRE(ms.getSIValue() == 0.5);
    }

    SECTION("Angle literals") {
        auto deg = 45.0_deg;
        auto rad = 1.57_rad;

        REQUIRE(deg.value() == 45.0);
        REQUIRE(std::abs(rad.value() - 1.57) < EPSILON);
    }

    SECTION("Power literals") {
        auto w = 100.0_W;
        auto kw = 1.5_kW;
        auto mw = 0.001_MW;

        REQUIRE(w.getSIValue() == 100.0);
        REQUIRE(kw.getSIValue() == 1500.0);
        REQUIRE(mw.getSIValue() == 1000.0);
    }

    SECTION("Literals in expressions") {
        auto distance = 100.0_m;
        auto time = 10.0_s;
        auto speed = distance / time;

        REQUIRE(speed.getSIValue() == 10.0);  // 10 m/s
    }
}

TEST_CASE("Temperature special conversions", "[temperature]") {
    SECTION("Celsius to Kelvin") {
        Celsius c{ 0 };
        auto k = c.as<Kelvin>();
        REQUIRE(std::abs(k.value() - 273.15) < EPSILON);

        Celsius room{ 20 };
        auto k2 = room.as<Kelvin>();
        REQUIRE(std::abs(k2.value() - 293.15) < EPSILON);
    }

    SECTION("Kelvin to Celsius") {
        Kelvin k{ 273.15 };
        auto c = k.as<Celsius>();
        REQUIRE(std::abs(c.value() - 0.0) < EPSILON);
    }

    SECTION("Celsius to Fahrenheit") {
        Celsius c{ 0 };
        auto f = c.as<Fahrenheit>();
        REQUIRE(std::abs(f.value() - 32.0) < EPSILON);

        Celsius c2{ 100 };
        auto f2 = c2.as<Fahrenheit>();
        REQUIRE(std::abs(f2.value() - 212.0) < EPSILON);
    }

    SECTION("Fahrenheit to Celsius") {
        Fahrenheit f{ 32 };
        auto c = f.as<Celsius>();
        REQUIRE(std::abs(c.value() - 0.0) < EPSILON);

        Fahrenheit f2{ 212 };
        auto c2 = f2.as<Celsius>();
        REQUIRE(std::abs(c2.value() - 100.0) < EPSILON);
    }
}

TEST_CASE("Complex unit compositions", "[composite]") {
    SECTION("Radiance: W / (m^2  sr)") {
        Watt power{ 60 };
        Meter side{ 2 };
        Steradian solid_angle{ 0.1 };

        auto area = side * side;
        auto radiance = power / (area * solid_angle);

        // 60W / (4m^2 * 0.1sr) = 150 W/(m^2 sr)
        REQUIRE(std::abs(radiance.getSIValue() - 150.0) < EPSILON);
    }

    SECTION("Irradiance: W / m^2") {
        Watt power{ 1000 };
        Meter side{ 10 };

        auto area = side * side;
        auto irradiance = power / area;

        // 1000W / 100m^2 = 10 W/m^2
        REQUIRE(std::abs(irradiance.getSIValue() - 10.0) < EPSILON);
    }

    SECTION("Kinetic energy: 1/2 m v^2") {
        Kilogram mass{ 10 };
        Meter dist{ 100 };
        Second time{ 10 };

        auto velocity = dist / time;  // 10 m/s
        auto velocity_squared = velocity * velocity;  // 100 m^2/s^2
        auto energy = 0.5 * mass * velocity_squared;

        // 0.5 * 10kg * 100 m^2/s^2 = 500 J
        REQUIRE(std::abs(energy.getSIValue() - 500.0) < EPSILON);
    }

    SECTION("Angular velocity") {
        Degree angle{ 360 };
        Second time{ 60 };

        // Composites for tags get converted to SI:
        auto angular_velocity = angle / time;

        // Should be in radians:
        REQUIRE(std::abs(angular_velocity.value() - (2 * PI<double>() / 60.0)) < EPSILON);

        // In radians: 2pi / 60s
        REQUIRE(std::abs(angular_velocity.getSIValue() - (2 * PI<double>() / 60.0)) < EPSILON);
    }
}

TEST_CASE("Edge cases and special values", "[edge-cases]") {
    SECTION("Zero values") {
        Meter m{ 0 };
        REQUIRE(m.value() == 0.0);
        REQUIRE(m.getSIValue() == 0.0);

        auto zero_sum = m + Meter{ 0 };
        REQUIRE(zero_sum.value() == 0.0);
    }

    SECTION("Negative values") {
        Celsius c{ -40 };
        REQUIRE(c.value() == -40.0);

        auto f = c.as<Fahrenheit>();
        REQUIRE(std::abs(f.value() - (-40.0)) < EPSILON);  // -40deg C = -40deg F
    }

    SECTION("Very large values") {
        Kilometer km{ 1000000 };  // 1 million km
        auto m = km.as<Meter>();
        REQUIRE(m.getSIValue() == 1e9);
    }

    SECTION("Very small values") {
        Nanometer nm{ 1 };
        REQUIRE(nm.getSIValue() == 1e-9);

        auto m = nm.as<Meter>();
        REQUIRE(std::abs(m.value() - 1e-9) < 1e-15);
    }
}

TEST_CASE("Type safety", "[type-safety]") {
    SECTION("Cannot add different dimensions") {
        // These should NOT compile (compile-time check)
        // Meter m{10};
        // Second s{5};
        // auto invalid = m + s;  // Should not compile

        // Just verify we can't accidentally mix dimensions
        REQUIRE(true);  // Placeholder since we can't test non-compilation
    }

    SECTION("Multiplication produces correct dimension") {
        Meter m{ 5 };
        Second s{ 2 };
        auto result = m / s;

        // Result should be Speed (m/s), not Meter or Second
        // This is verified by the type system, but we can check the value
        REQUIRE(result.getSIValue() == 2.5);
    }
}

TEST_CASE("Real-world rendering scenarios", "[rendering]") {
    SECTION("FOV conversions") {
        // User specifies FOV in degrees
        Degree user_fov{ 60 };

        // Renderer needs radians internally
        double fov_radians = user_fov.getSIValue();
        REQUIRE(std::abs(fov_radians - (PI<double>() / 3.0)) < EPSILON);

        // Can also convert explicitly
        auto fov_rad = user_fov.as<Radian>();
        REQUIRE(std::abs(fov_rad.value() - (PI<double>() / 3.0)) < EPSILON);
    }

    SECTION("Light intensity calculations") {
        // Point light with power in watts
        Watt light_power{ 100 };

        // Distance from light
        Meter distance{ 5 };

        // Irradiance at distance (assuming spherical propagation)
        auto sphere_area = 4.0 * PI<double>() * (distance * distance);
        auto irradiance = light_power / sphere_area;

        // Should be approximately 0.318 W/m^2
        REQUIRE(std::abs(irradiance.getSIValue() - 0.318) < 0.001);
    }
}

TEST_CASE("Copy and assignment semantics", "[semantics]") {
    SECTION("Copy construction") {
        Meter original{ 100 };
        Meter copy{ original };

        REQUIRE(copy.value() == original.value());
        REQUIRE(copy.getSIValue() == original.getSIValue());
    }

    SECTION("Copy assignment") {
        Meter a{ 100 };
        Meter b{ 50 };

        b = a;
        REQUIRE(b.value() == 100.0);
    }

    SECTION("Cross-scale copy construction") {
        Kilometer km{ 1 };
        Meter m{ km };  // Should convert

        REQUIRE(m.getSIValue() == 1000.0);
    }
}

TEST_CASE("Default construction", "[construction]") {
    SECTION("Default constructed quantities are zero") {
        Meter m;
        REQUIRE(m.value() == 0.0);

        Degree d;
        REQUIRE(d.value() == 0.0);

        Watt w;
        REQUIRE(w.value() == 0.0);
    }
}
