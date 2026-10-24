#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "assimp/Exporter.hpp"
#include "assimp/material.h"
#include "assimp/scene.h"
#include "catch2/catch_test_macros.hpp"
#include "catch2/matchers/catch_matchers_floating_point.hpp"
#include "catch2/matchers/catch_matchers_string.hpp"
#include "huira/huira.hpp"

using namespace huira;
using namespace huira::units::literals;
using Catch::Matchers::ContainsSubstring;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

namespace fs = std::filesystem;

// Models are written for each test, as glTF (whose images are files beside the model, or embedded
// in it as data), OBJ, or assimp's own format (assbin, written from an aiScene made here, for what
// the other formats can't hold). Texture values are 8-bit fractions (k / 255), exact in a file.

namespace {
using Bytes = std::vector<unsigned char>;

/// A directory of this test's own for its files.
fs::path test_dir()
{
    static const fs::path dir = [] {
        const fs::path d = fs::temp_directory_path() / "huira_test_model_loader";
        fs::create_directories(d);
        return d;
    }();
    return dir;
}

fs::path write_file(const std::string& name, const std::string& contents)
{
    const fs::path path = test_dir() / name;
    std::ofstream(path, std::ios::binary) << contents;
    return path;
}

fs::path write_file(const std::string& name, const Bytes& contents)
{
    return write_file(name, std::string(contents.begin(), contents.end()));
}

Bytes read_file(const fs::path& path)
{
    std::ifstream in(path, std::ios::binary);
    return Bytes(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

std::string base64(const Bytes& bytes)
{
    static constexpr char DIGITS[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    for (std::size_t i = 0; i < bytes.size(); i += 3) {
        const std::size_t left = bytes.size() - i;
        std::uint32_t n = static_cast<std::uint32_t>(bytes[i]) << 16;
        if (left > 1) {
            n |= static_cast<std::uint32_t>(bytes[i + 1]) << 8;
        }
        if (left > 2) {
            n |= static_cast<std::uint32_t>(bytes[i + 2]);
        }
        out += DIGITS[(n >> 18) & 63];
        out += DIGITS[(n >> 12) & 63];
        out += left > 1 ? DIGITS[(n >> 6) & 63] : '=';
        out += left > 2 ? DIGITS[n & 63] : '=';
    }
    return out;
}

std::string data_uri(const std::string& mime_type, const Bytes& bytes)
{
    return "data:" + mime_type + ";base64," + base64(bytes);
}

/// An 8-bit value as the fraction it is stored as.
constexpr float b8(int value)
{
    return static_cast<float>(value) / 255.f;
}

/// A 1 x 1 PNG of one color (and alpha, when given), labelled with a color space.
Bytes png(int r,
          int g,
          int b,
          std::optional<int> a = std::nullopt,
          ColorSpaceHint color_space = ColorSpaceHint::sRGB)
{
    ImageBundle<RGB> bundle(Image<RGB>(1, 1, RGB{b8(r), b8(g), b8(b)}));
    if (a) {
        bundle.alpha = Image<float>(1, 1, b8(*a));
    }
    bundle.color_space = color_space;
    const fs::path path = test_dir() / "scratch.png";
    write_image_png(path, bundle);
    return read_file(path);
}

/// An 8 x 8 JPEG of one color.
Bytes jpeg(int r, int g, int b)
{
    const fs::path path = test_dir() / "scratch.jpg";
    write_image_jpeg(path, ImageBundle<RGB>(Image<RGB>(8, 8, RGB{b8(r), b8(g), b8(b)})), 100);
    return read_file(path);
}

void put_le(Bytes& out, std::uint32_t value, int bytes)
{
    for (int i = 0; i < bytes; ++i) {
        out.push_back(static_cast<unsigned char>((value >> (8 * i)) & 0xFF));
    }
}

/// A 1 x 1, 24-bit, uncompressed TGA.
Bytes tga(int r, int g, int b)
{
    Bytes out{0, 0, 2, 0, 0, 0, 0, 0}; // no ID, no color map, uncompressed true-color
    put_le(out, 0, 2);                 // x origin
    put_le(out, 0, 2);                 // y origin
    put_le(out, 1, 2);                 // width
    put_le(out, 1, 2);                 // height
    out.push_back(24);                 // bits per pixel
    out.push_back(0x20);               // top-left origin
    out.insert(out.end(),
               {static_cast<unsigned char>(b),
                static_cast<unsigned char>(g),
                static_cast<unsigned char>(r)});
    return out;
}

/// A 1 x 1, 24-bit BMP.
Bytes bmp(int r, int g, int b)
{
    Bytes out{'B', 'M'};
    put_le(out, 58, 4); // file size: 14 + 40 + one row of 4 bytes
    put_le(out, 0, 4);
    put_le(out, 54, 4); // pixel data offset
    put_le(out, 40, 4); // BITMAPINFOHEADER
    put_le(out, 1, 4);  // width
    put_le(out, 1, 4);  // height
    put_le(out, 1, 2);  // planes
    put_le(out, 24, 2); // bits per pixel
    put_le(out, 0, 4);  // no compression
    put_le(out, 4, 4);  // image size
    put_le(out, 2835, 4);
    put_le(out, 2835, 4);
    put_le(out, 0, 4);
    put_le(out, 0, 4);
    out.insert(out.end(),
               {static_cast<unsigned char>(b),
                static_cast<unsigned char>(g),
                static_cast<unsigned char>(r),
                0});
    return out;
}

/// A 1 x 1 Radiance HDR of (0.5, 0.25, 0.125): mantissas of 128, 64 and 32 under an exponent of
/// 0, which the reader decodes exactly.
Bytes hdr()
{
    const std::string header = "#?RADIANCE\nFORMAT=32-bit_rle_rgbe\n\n-Y 1 +X 1\n";
    Bytes out(header.begin(), header.end());
    out.insert(out.end(), {128, 64, 32, 128});
    return out;
}

/// The bytes of a triangle's buffer: (0, 0, 0), (1, 0, 0) and (0, 1, 0), with texture
/// coordinates (0, 0), (1, 0) and (0, 1), and indices 0, 1, 2.
std::string triangle_buffer_uri()
{
    const float positions[] = {0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 1.f, 0.f};
    const float uvs[] = {0.f, 0.f, 1.f, 0.f, 0.f, 1.f};
    const std::uint16_t indices[] = {0, 1, 2, 0};
    Bytes bytes(sizeof(positions) + sizeof(uvs) + sizeof(indices));
    std::memcpy(bytes.data(), positions, sizeof(positions));
    std::memcpy(bytes.data() + sizeof(positions), uvs, sizeof(uvs));
    std::memcpy(bytes.data() + sizeof(positions) + sizeof(uvs), indices, sizeof(indices));
    return data_uri("application/octet-stream", bytes);
}

/**
 * A glTF model of triangles, one mesh for each material: mesh i, named "Mesh<i>", has material i.
 * Each image is a texture of the same index. By default each mesh is placed by a node of its own,
 * named "Node<i>", at the model's origin.
 */
struct Gltf {
    std::vector<std::string> materials{"{}"};
    std::vector<std::string> images;
    std::optional<std::string> nodes;       // the "nodes" array's contents
    std::optional<std::string> scene_nodes; // the scene's root nodes
    bool meshes_have_materials = true;      // or none at all

    fs::path write(const std::string& name) const
    {
        std::ostringstream json;
        json << R"({"asset": {"version": "2.0"}, "scene": 0, "scenes": [{"nodes": [)";
        if (scene_nodes) {
            json << *scene_nodes;
        } else {
            for (std::size_t i = 0; i < materials.size(); ++i) {
                json << (i ? ", " : "") << i;
            }
        }
        json << R"(]}], "nodes": [)";
        if (nodes) {
            json << *nodes;
        } else {
            for (std::size_t i = 0; i < materials.size(); ++i) {
                json << (i ? ", " : "") << R"({"name": "Node)" << i << R"(", "mesh": )" << i << "}";
            }
        }
        json << R"(], "meshes": [)";
        for (std::size_t i = 0; i < materials.size(); ++i) {
            json << (i ? ", " : "") << R"({"name": "Mesh)" << i
                 << R"(", "primitives": [{"attributes": {"POSITION": 0, "TEXCOORD_0": 1}, )"
                 << R"("indices": 2)";
            if (meshes_have_materials) {
                json << R"(, "material": )" << i;
            }
            json << "}]}";
        }
        json << R"(], "materials": [)";
        for (std::size_t i = 0; i < materials.size(); ++i) {
            json << (i ? ", " : "") << materials[i];
        }
        json << "]";
        if (!images.empty()) {
            json << R"(, "textures": [)";
            for (std::size_t i = 0; i < images.size(); ++i) {
                json << (i ? ", " : "") << R"({"source": )" << i << "}";
            }
            json << R"(], "images": [)";
            for (std::size_t i = 0; i < images.size(); ++i) {
                json << (i ? ", " : "") << images[i];
            }
            json << "]";
        }
        json << R"(, "buffers": [{"byteLength": 68, "uri": ")" << triangle_buffer_uri()
             << R"("}], "bufferViews": [)"
             << R"({"buffer": 0, "byteOffset": 0, "byteLength": 36}, )"
             << R"({"buffer": 0, "byteOffset": 36, "byteLength": 24}, )"
             << R"({"buffer": 0, "byteOffset": 60, "byteLength": 6}], "accessors": [)"
             << R"({"bufferView": 0, "componentType": 5126, "count": 3, "type": "VEC3", )"
             << R"("min": [0, 0, 0], "max": [1, 1, 0]}, )"
             << R"({"bufferView": 1, "componentType": 5126, "count": 3, "type": "VEC2"}, )"
             << R"({"bufferView": 2, "componentType": 5123, "count": 3, "type": "SCALAR"}]})";
        return write_file(name, json.str());
    }
};

std::string image_file(const std::string& file)
{
    return R"({"uri": ")" + file + R"("})";
}

std::string image_data(const std::string& mime_type, const Bytes& bytes)
{
    return R"({"uri": ")" + data_uri(mime_type, bytes) + R"("})";
}

/// The material of a loaded mesh, by the mesh's name.
std::shared_ptr<Material<RGB>> material_of(const Scene<RGB>& scene, const std::string& mesh)
{
    return scene.get_primitive(mesh).get()->material;
}

/// A surface facing +z, at a texture coordinate, with a tangent frame for normal maps.
Interaction<RGB> flat_surface(Vec2<float> uv = Vec2<float>{0.5f, 0.5f})
{
    Interaction<RGB> isect;
    isect.normal_g = Vec3<float>{0.f, 0.f, 1.f};
    isect.normal_s = isect.normal_g;
    build_default_tangent_frame(isect.normal_s, isect.tangent, isect.bitangent);
    isect.uv = uv;
    return isect;
}

/// A loaded mesh's material's values, by the mesh's name.
ShadingParams<RGB> values_of(const Scene<RGB>& scene, const std::string& mesh)
{
    return material_of(scene, mesh)->evaluate(flat_surface()).params;
}

/// A loaded mesh's geometry, by the mesh's name.
std::shared_ptr<Mesh<RGB>> mesh_of(const Scene<RGB>& scene, const std::string& mesh)
{
    return std::dynamic_pointer_cast<Mesh<RGB>>(scene.get_primitive(mesh).get()->geometry);
}

/// Each bin of a matches b's, to an absolute tolerance.
void check_bins(const RGB& a, const RGB& b, float tolerance = 1e-6f)
{
    for (std::size_t i = 0; i < RGB::size(); ++i) {
        INFO("bin " << i);
        CHECK_THAT(a[i], WithinAbs(b[i], tolerance));
    }
}

RGB linear(int r, int g, int b)
{
    return RGB{srgb_to_linear(b8(r)), srgb_to_linear(b8(g)), srgb_to_linear(b8(b))};
}

/// A 1 x 1 uncompressed texture, of one texel, for an aiScene.
aiTexture* raw_texture(int r, int g, int b, int a)
{
    auto* texture = new aiTexture();
    texture->mWidth = 1;
    texture->mHeight = 1;
    texture->pcData = new aiTexel[1];
    texture->pcData[0].r = static_cast<unsigned char>(r);
    texture->pcData[0].g = static_cast<unsigned char>(g);
    texture->pcData[0].b = static_cast<unsigned char>(b);
    texture->pcData[0].a = static_cast<unsigned char>(a);
    std::memcpy(texture->achFormatHint, "rgba8888", sizeof("rgba8888"));
    return texture;
}

/**
 * Writes, in assimp's own format, a model of one triangle attached to its root node, whose
 * material's base color, normal and metallic-roughness textures are uncompressed and embedded
 * (*0, *1 and *2). Its normals and tangents are given, and those of one vertex are made NaN when
 * asked for. Its mesh is named "Embedded", which the format does not keep, and its material index
 * is 0, unless it is given another, which no material has.
 */
fs::path write_assbin(const std::string& name,
                      bool nan_normal,
                      bool nan_tangent,
                      unsigned int material_index = 0)
{
    auto scene = std::make_unique<aiScene>();
    scene->mRootNode = new aiNode("Root");
    scene->mRootNode->mNumMeshes = 1;
    scene->mRootNode->mMeshes = new unsigned int[1]{0};

    auto* mesh = new aiMesh();
    mesh->mName = aiString("Embedded");
    mesh->mPrimitiveTypes = aiPrimitiveType_TRIANGLE;
    mesh->mNumVertices = 3;
    mesh->mVertices = new aiVector3D[3]{{0.f, 0.f, 0.f}, {1.f, 0.f, 0.f}, {0.f, 1.f, 0.f}};
    mesh->mNormals = new aiVector3D[3]{{0.f, 0.f, 1.f}, {0.f, 0.f, 1.f}, {0.f, 0.f, 1.f}};
    mesh->mTangents = new aiVector3D[3]{{1.f, 0.f, 0.f}, {1.f, 0.f, 0.f}, {1.f, 0.f, 0.f}};
    mesh->mBitangents = new aiVector3D[3]{{0.f, 1.f, 0.f}, {0.f, 1.f, 0.f}, {0.f, 1.f, 0.f}};
    mesh->mTextureCoords[0] = new aiVector3D[3]{{0.f, 0.f, 0.f}, {1.f, 0.f, 0.f}, {0.f, 1.f, 0.f}};
    mesh->mNumUVComponents[0] = 2;
    const float nan = std::numeric_limits<float>::quiet_NaN();
    if (nan_normal) {
        mesh->mNormals[1] = aiVector3D(nan, nan, nan);
    }
    if (nan_tangent) {
        mesh->mTangents[2] = aiVector3D(nan, 0.f, 0.f);
    }
    mesh->mNumFaces = 1;
    mesh->mFaces = new aiFace[1];
    mesh->mFaces[0].mNumIndices = 3;
    mesh->mFaces[0].mIndices = new unsigned int[3]{0, 1, 2};
    mesh->mMaterialIndex = material_index;
    scene->mNumMeshes = 1;
    scene->mMeshes = new aiMesh*[1]{mesh};

    auto* material = new aiMaterial();
    const aiString material_name("RawTextures");
    material->AddProperty(&material_name, AI_MATKEY_NAME);
    const aiString base_color("*0");
    const aiString normals("*1");
    const aiString metallic_roughness("*2");
    material->AddProperty(&base_color, AI_MATKEY_TEXTURE(aiTextureType_BASE_COLOR, 0));
    material->AddProperty(&normals, AI_MATKEY_TEXTURE(aiTextureType_NORMALS, 0));
    material->AddProperty(&metallic_roughness, AI_MATKEY_TEXTURE(aiTextureType_METALNESS, 0));
    scene->mNumMaterials = 1;
    scene->mMaterials = new aiMaterial*[1]{material};

    scene->mNumTextures = 3;
    scene->mTextures = new aiTexture*[3]{raw_texture(204, 102, 51, 153),
                                         raw_texture(204, 128, 204, 255),
                                         raw_texture(0, 153, 51, 255)};

    const fs::path path = test_dir() / name;
    Assimp::Exporter exporter;
    REQUIRE(exporter.Export(scene.get(), "assbin", path.string()) == aiReturn_SUCCESS);
    return path;
}

/**
 * Loads a model of one mesh, written by write_assbin(), as it is (with no post-processing), and
 * returns the name of its primitive. The format keeps no mesh names, so the primitive has the
 * name a scene gives one without a name, "Primitive.<ID>", and the ID after that of a primitive
 * made just before it.
 */
std::string load_assbin(Scene<RGB>& scene, const fs::path& path)
{
    const auto before = scene.add_primitive(scene.add_ellipsoid(1_m, 1_m, 1_m));
    scene.load_model(path, "", 0);
    return "Primitive." + std::to_string(before.id() + 1);
}

/// The camera at the origin, and what a ray from it in a direction hits.
struct RayCaster {
    const Scene<RGB>& scene;
    InstanceHandle<RGB> camera;

    RayCaster(const Scene<RGB>& scene_, InstanceHandle<RGB> camera_)
        : scene(scene_), camera(std::move(camera_))
    {
    }
    RayCaster(const RayCaster&) = delete;
    RayCaster(RayCaster&&) = delete;
    RayCaster& operator=(const RayCaster&) = delete;
    RayCaster& operator=(RayCaster&&) = delete;

    HitRecord cast(const Vec3<float>& toward) const
    {
        const SceneView<RGB> view(scene,
                                  Interval{Time::from_et(0.0), Time::from_et(1.0)},
                                  camera,
                                  ObservationMode::TRUE_STATE);
        return view.intersect(Ray<RGB>(Vec3<float>{0.f}, glm::normalize(toward)));
    }
};
} // namespace

TEST_CASE("A model that is missing or can't be read is refused", "[assets][models]")
{
    Scene<RGB> scene;
    CHECK_THROWS(scene.load_model(test_dir() / "no_such_model.gltf"));
    CHECK_THROWS(scene.load_model(write_file("not_a_model.gltf", "this is not a model")));
    CHECK_THROWS(scene.load_model(write_file("not_a_model.obj", std::string("\0\1\2", 3))));
}

TEST_CASE("A model is named for its file, unless it is given a name", "[assets][models]")
{
    const fs::path path = Gltf{}.write("named_model.gltf");
    Scene<RGB> scene;
    auto unnamed = scene.load_model(path);
    CHECK(unnamed.name() == "named_model");
    CHECK(unnamed.type() == "Model");
    CHECK(unnamed.get()->source_path() == path);

    Scene<RGB> other;
    auto named = other.load_model(path, "Triangle");
    CHECK(named.name() == "Triangle");
    CHECK(other.get_model("Triangle").id() == named.id());
}

TEST_CASE("A model's mesh keeps its vertices, texture coordinates and faces", "[assets][models]")
{
    // OBJ's texture coordinates count up from the bottom of an image, and huira's down from the
    // top: v becomes 1 - v.
    const fs::path path = write_file("uv_triangle.obj",
                                     "o Tri\n"
                                     "v 0 0 0\nv 2 0 0\nv 0 3 0\n"
                                     "vt 0.25 0.75\nvt 1 0\nvt 0 1\n"
                                     "f 1/1 2/2 3/3\n");
    Scene<RGB> scene;
    scene.load_model(path);
    auto mesh = mesh_of(scene, "Tri");
    REQUIRE(mesh != nullptr);
    REQUIRE(mesh->vertex_count() == 3);
    CHECK(mesh->index_buffer() == IndexBuffer{0, 1, 2});
    const auto& vertices = mesh->vertex_buffer();
    CHECK(vertices[1].position == Vec3<float>{2.f, 0.f, 0.f});
    CHECK(vertices[2].position == Vec3<float>{0.f, 3.f, 0.f});
    CHECK(vertices[0].uv == Vec2<float>{0.25f, 0.25f});
    CHECK(vertices[1].uv == Vec2<float>{1.f, 1.f});
    // Normals are made for it, facing as the triangle winds, and tangents from its texture
    // coordinates:
    for (const auto& vertex : vertices) {
        CHECK(vertex.normal == Vec3<float>{0.f, 0.f, 1.f});
        CHECK(vertex.albedo == RGB{1.f}); // no vertex colors
    }
    CHECK(mesh->has_tangents());

    // Without texture coordinates, a mesh has none, and no tangents:
    Scene<RGB> plain;
    plain.load_model(
        write_file("plain_triangle.obj", "o Tri\nv 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n"));
    CHECK(mesh_of(plain, "Tri")->vertex_buffer()[1].uv == Vec2<float>{0.f, 0.f});
    CHECK_FALSE(mesh_of(plain, "Tri")->has_tangents());
}

TEST_CASE("A model's faces that are not triangles are left out of its mesh", "[assets][models]")
{
    // Loaded with no post-processing, nothing is triangulated or made: the quad and the line are
    // dropped, and the vertices, which have no normals, face up (+y).
    const fs::path path = write_file("mixed_faces.obj",
                                     "o Mixed\n"
                                     "v 0 0 0\nv 1 0 0\nv 0 1 0\nv 1 1 0\nv 2 0 0\nv 3 0 0\n"
                                     "f 1 2 3\nf 2 4 3 5\nl 5 6\n");
    Scene<RGB> scene;
    scene.load_model(path, "", 0);
    auto mesh = mesh_of(scene, "Mixed");
    REQUIRE(mesh != nullptr);
    CHECK(mesh->triangle_count() == 1);
    for (const auto& vertex : mesh->vertex_buffer()) {
        CHECK(vertex.normal == Vec3<float>{0.f, 1.f, 0.f});
    }
}

TEST_CASE("A model's vertex colors are its vertices' albedos, in the scene's spectrum",
          "[assets][models]")
{
    const fs::path path = write_file("colored_triangle.obj",
                                     "o Tri\n"
                                     "v 0 0 0 1 0 0\nv 1 0 0 0 0.5 0\nv 0 1 0 0 0 0.25\n"
                                     "f 1 2 3\n");
    Scene<RGB> scene;
    scene.load_model(path);
    const auto& vertices = mesh_of(scene, "Tri")->vertex_buffer();
    CHECK(vertices[0].albedo == RGB{1.f, 0.f, 0.f});
    CHECK(vertices[1].albedo == RGB{0.f, 0.5f, 0.f});
    CHECK(vertices[2].albedo == RGB{0.f, 0.f, 0.25f});

    // Through the conversion given to the loader:
    Scene<RGB> converted;
    ModelLoader<RGB>::load(
        converted, path, "", ModelLoader<RGB>::DEFAULT_POST_PROCESS_FLAGS, [](RGB rgb) {
            return RGB{rgb[2], rgb[1], rgb[0]};
        });
    const auto& swapped = mesh_of(converted, "Tri")->vertex_buffer();
    CHECK(swapped[0].albedo == RGB{0.f, 0.f, 1.f});
    CHECK(swapped[2].albedo == RGB{0.25f, 0.f, 0.f});

    // A color beyond 1 is not a reflectance, which the mesh requires, so it is clamped, with a
    // warning, as an MTL file's diffuse color is:
    const fs::path bright = write_file("bright_triangle.obj",
                                       "o Bright\n"
                                       "v 0 0 0 1.5 0.5 0\nv 1 0 0 0 0 0\nv 0 1 0 0 0 0\n"
                                       "f 1 2 3\n");
    Scene<RGB> clamped;
    REQUIRE_NOTHROW(clamped.load_model(bright));
    CHECK(mesh_of(clamped, "Bright")->vertex_buffer()[0].albedo == RGB{1.f, 0.5f, 0.f});
}

TEST_CASE("A model's normals and tangents that are not finite are replaced or dropped",
          "[assets][models]")
{
    // Degenerate triangles can be given normals and tangents that are NaN, which a mesh refuses.
    // Such a normal is replaced with +y, and such tangents are dropped (for all of the mesh).
    Scene<RGB> good;
    const std::string good_mesh = load_assbin(good, write_assbin("finite.assbin", false, false));
    CHECK(mesh_of(good, good_mesh)->vertex_buffer()[1].normal == Vec3<float>{0.f, 0.f, 1.f});
    CHECK(mesh_of(good, good_mesh)->has_tangents());

    Scene<RGB> bad_normal;
    const std::string bad_normal_mesh =
        load_assbin(bad_normal, write_assbin("nan_normal.assbin", true, false));
    const auto& vertices = mesh_of(bad_normal, bad_normal_mesh)->vertex_buffer();
    CHECK(vertices[0].normal == Vec3<float>{0.f, 0.f, 1.f});
    CHECK(vertices[1].normal == Vec3<float>{0.f, 1.f, 0.f});
    CHECK(mesh_of(bad_normal, bad_normal_mesh)->has_tangents());

    Scene<RGB> bad_tangent;
    const std::string bad_tangent_mesh =
        load_assbin(bad_tangent, write_assbin("nan_tangent.assbin", false, true));
    CHECK_FALSE(mesh_of(bad_tangent, bad_tangent_mesh)->has_tangents());
}

TEST_CASE("A model's nodes place its meshes", "[assets][models]")
{
    // The triangle (0, 0, 0), (1, 0, 0), (0, 1, 0) is scaled by 2 by its node, turned 90 degrees
    // about z by its parent, and moved 10 m along z by its grandparent: it is (0, 0, 10),
    // (0, 2, 10), (-2, 0, 10).
    Gltf gltf;
    gltf.nodes = R"({"name": "Grandparent", "translation": [0, 0, 10], "children": [1]},
                    {"name": "Parent", "rotation": [0, 0, 0.70710678, 0.70710678],
                     "children": [2]},
                    {"name": "Child", "mesh": 0, "scale": [2, 2, 2]})";
    gltf.scene_nodes = "0";
    Scene<RGB> scene;
    auto model = scene.load_model(gltf.write("nested_nodes.gltf"));
    scene.root.new_instance(model);
    const RayCaster caster{scene, scene.root.new_instance(scene.new_camera_model())};

    // (Each point is on it only if it is scaled.)
    for (const Vec3<float>& point :
         {Vec3<float>{-1.5f, 0.25f, 10.f}, Vec3<float>{-0.25f, 1.5f, 10.f}}) {
        const HitRecord hit = caster.cast(point);
        REQUIRE(hit.hit());
        CHECK_THAT(hit.t, WithinRel(glm::length(point), 1e-5f));
    }
    // Where it would be if it were not turned:
    CHECK_FALSE(caster.cast(Vec3<float>{0.25f, 0.25f, 10.f}).hit());

    // And a model of several nodes is placed by each:
    Gltf two;
    two.materials = {"{}", "{}"};
    two.nodes = R"({"name": "Left", "mesh": 0, "translation": [-5, 0, 10]},
                   {"name": "Right", "mesh": 1, "translation": [5, 0, 10]})";
    Scene<RGB> pair;
    pair.root.new_instance(pair.load_model(two.write("two_nodes.gltf")));
    const RayCaster pair_caster{pair, pair.root.new_instance(pair.new_camera_model())};
    CHECK(pair_caster.cast(Vec3<float>{-4.75f, 0.25f, 10.f}).hit());
    CHECK(pair_caster.cast(Vec3<float>{5.25f, 0.25f, 10.f}).hit());
    CHECK_FALSE(pair_caster.cast(Vec3<float>{0.25f, 0.25f, 10.f}).hit());
}

TEST_CASE("A model's node that mirrors its mesh is loaded, mirrored", "[assets][models]")
{
    // Before, the loader took a node's matrix, less its scale, to be a rotation, and one that
    // mirrors isn't (its determinant is -1), so the model could not be loaded. glTF allows a
    // negative scale, and nodes take one.
    //
    // Scaled by -2 along x and turned 90 degrees about z, 10 m along z, the triangle is
    // (0, 0, 10), (0, -2, 10), (-1, 0, 10); not mirrored, it would be (0, 2, 10) and not
    // (0, -2, 10).
    Gltf gltf;
    gltf.nodes = R"({"name": "Mirror", "mesh": 0, "translation": [0, 0, 10],
                     "scale": [-2, 1, 1], "rotation": [0, 0, 0.70710678, 0.70710678]})";
    Scene<RGB> scene;
    ModelHandle<RGB> model = scene.load_model(gltf.write("mirrored_node.gltf"));
    scene.root.new_instance(model);
    const RayCaster caster{scene, scene.root.new_instance(scene.new_camera_model())};

    const HitRecord hit = caster.cast(Vec3<float>{-0.25f, -0.5f, 10.f});
    REQUIRE(hit.hit());
    CHECK_THAT(hit.t, WithinRel(glm::length(Vec3<float>{-0.25f, -0.5f, 10.f}), 1e-5f));
    CHECK_FALSE(caster.cast(Vec3<float>{-0.25f, 0.5f, 10.f}).hit());
}

namespace {
/// A Collada model of one triangle, (0, 0, 0), (1, 0, 0), (0, 1, 0), placed by one node of the
/// given name and matrix (its 16 numbers, row by row, as Collada writes them).
fs::path write_collada(const std::string& file, const std::string& node, const std::string& matrix)
{
    std::ostringstream dae;
    dae << R"(<?xml version="1.0" encoding="utf-8"?>
<COLLADA xmlns="http://www.collada.org/2005/11/COLLADASchema" version="1.4.1">
  <asset><unit name="meter" meter="1"/><up_axis>Y_UP</up_axis></asset>
  <library_geometries><geometry id="Tri" name="Tri"><mesh>
    <source id="Tri-positions">
      <float_array id="Tri-positions-array" count="9">0 0 0 1 0 0 0 1 0</float_array>
      <technique_common><accessor source="#Tri-positions-array" count="3" stride="3">
        <param name="X" type="float"/>
        <param name="Y" type="float"/>
        <param name="Z" type="float"/>
      </accessor></technique_common>
    </source>
    <vertices id="Tri-vertices"><input semantic="POSITION" source="#Tri-positions"/></vertices>
    <triangles count="1">
      <input semantic="VERTEX" source="#Tri-vertices" offset="0"/><p>0 1 2</p>
    </triangles>
  </mesh></geometry></library_geometries>
  <library_visual_scenes><visual_scene id="Scene" name="Scene">
)";
    dae << "    <node id=\"" << node << "\" name=\"" << node << "\">\n"
        << "      <matrix>" << matrix << "</matrix>\n"
        << R"(      <instance_geometry url="#Tri"/>
    </node>
  </visual_scene></library_visual_scenes>
  <scene><instance_visual_scene url="#Scene"/></scene>
</COLLADA>
)";
    return write_file(file, dae.str());
}
} // namespace

TEST_CASE("A model's node matrix written to 3 decimals is loaded, and one that isn't a rotation "
          "is refused, naming its node",
          "[assets][models]")
{
    // A node's matrix, less its scale, was refused unless its columns were within 1e-3 of right
    // angles, which rounding to 3 decimals often leaves them beyond, so models that had loaded no
    // longer did. The error didn't say which file or node.
    //
    // This one turns by Euler angles of 60, 35 and 120 degrees (extrinsic, XYZ), and moves 10 m
    // along z. Written to 3 decimals, its first and last columns are 1.4e-3 from right angles.
    const fs::path path = write_collada("rounded_node.dae",
                                        "Rounded",
                                        "-0.41 -0.681 0.607 0  0.709 0.18 0.681 0  "
                                        "-0.574 0.709 0.41 10  0 0 0 1");
    Scene<RGB> scene;
    scene.root.new_instance(scene.load_model(path));
    const RayCaster caster{scene, scene.root.new_instance(scene.new_camera_model())};

    const Rotation<double> exact = Rotation<double>::extrinsic_euler_angles(
        units::Degree(60), units::Degree(35), units::Degree(120), "XYZ");
    const Vec3<double> centroid =
        exact * Vec3<double>{0.25, 0.25, 0.0} + Vec3<double>{0.0, 0.0, 10.0};
    const HitRecord hit = caster.cast(Vec3<float>(centroid));
    REQUIRE(hit.hit());
    CHECK_THAT(hit.t, WithinRel(static_cast<float>(glm::length(centroid)), 1e-3f));

    // One that shears its mesh is not a rotation and a scale, and the error says where it is:
    const fs::path sheared =
        write_collada("sheared_node.dae", "Sheared", "1 0.2 0 0  0 1 0 0  0 0 1 10  0 0 0 1");
    Scene<RGB> refused;
    CHECK_THROWS_WITH(refused.load_model(sheared),
                      ContainsSubstring("Sheared") && ContainsSubstring("sheared_node.dae"));
}

TEST_CASE("A model's materials take their glTF values", "[assets][models]")
{
    Gltf gltf;
    gltf.materials = {
        R"({"name": "Values", "pbrMetallicRoughness": {"baseColorFactor": [0.2, 0.4, 0.6, 0.8],
            "metallicFactor": 0.25, "roughnessFactor": 0.75}, "emissiveFactor": [0.5, 0.25, 1]})",
        R"({"name": "Defaults"})"};
    Scene<RGB> scene;
    scene.load_model(gltf.write("material_values.gltf"));

    const ShadingParams<RGB> p = values_of(scene, "Mesh0");
    check_bins(p.albedo, RGB{0.2f, 0.4f, 0.6f});
    CHECK_THAT(p.opacity, WithinAbs(0.8f, 1e-6f));
    CHECK_THAT(p.metallic, WithinAbs(0.25f, 1e-6f));
    CHECK_THAT(p.roughness, WithinAbs(0.75f, 1e-6f));
    check_bins(p.emission, RGB{0.5f, 0.25f, 1.f});
    CHECK(material_of(scene, "Mesh0")->name() == "Values");

    // glTF's defaults: white, opaque, metallic, rough, and dark.
    const ShadingParams<RGB> d = values_of(scene, "Mesh1");
    check_bins(d.albedo, RGB{1.f});
    CHECK(d.opacity == 1.f);
    CHECK(d.metallic == 1.f);
    CHECK(d.roughness == 1.f);
    check_bins(d.emission, RGB{0.f});
}

TEST_CASE("A model that fails to load partway leaves nothing in the scene", "[assets][models]")
{
    // Its model, materials, meshes and named nodes were added as it went, and stayed when a later
    // part threw: the half-built model was found by its name, and a good load after it was given
    // names of its own (half_built_1, First_1). Here the second node's matrix has no inverse,
    // which is refused after everything but that node was added.
    Gltf gltf;
    gltf.materials = {R"({"name": "First"})", R"({"name": "Second"})"};
    gltf.nodes =
        R"({"name": "Node0", "mesh": 0}, {"name": "Node1", "mesh": 1, "scale": [0, 1, 1]})";
    Scene<RGB> scene;
    CHECK_THROWS(scene.load_model(gltf.write("half_built.gltf")));
    CHECK_THROWS(scene.get_model("half_built"));
    CHECK_THROWS(scene.get_geometry("Mesh0"));
    CHECK_THROWS(scene.get_primitive("Mesh1"));

    // The same model, made valid, loads with its own names:
    gltf.nodes.reset();
    const ModelHandle<RGB> model = scene.load_model(gltf.write("half_built.gltf"));
    CHECK(model.get()->name() == "half_built");
    CHECK(scene.get_geometry("Mesh0").get()->name() == "Mesh0");
    CHECK(material_of(scene, "Mesh0")->name() == "First");
    CHECK(material_of(scene, "Mesh1")->name() == "Second");
}

TEST_CASE("A model's mesh without a material is given a default one", "[assets][models]")
{
    // A mesh with no material in a valid model is given its format's default material by ASSIMP:
    // glTF's is white, opaque, metallic and rough.
    Gltf gltf;
    gltf.meshes_have_materials = false;
    Scene<RGB> scene;
    scene.load_model(gltf.write("no_material.gltf"));
    const ShadingParams<RGB> p = values_of(scene, "Mesh0");
    check_bins(p.albedo, RGB{1.f});
    CHECK(p.opacity == 1.f);
    CHECK(p.metallic == 1.f);
    CHECK(p.roughness == 1.f);

    // A mesh whose material index is not one of the model's materials (which ASSIMP doesn't
    // check) is given the scene's. Before, the loader warned that it would be, but gave it an
    // invalid material, and the model could not be loaded.
    Scene<RGB> bad;
    const std::string mesh = load_assbin(bad, write_assbin("bad_material.assbin", false, false, 5));
    const auto default_material =
        bad.add_primitive(bad.add_ellipsoid(1_m, 1_m, 1_m)).get()->material;
    CHECK(material_of(bad, mesh) == default_material);
}

TEST_CASE("An OBJ model's materials take their diffuse colors and textures as albedos",
          "[assets][models]")
{
    // MTL has no base color, but a diffuse color (Kd) and texture (map_Kd), which ASSIMP gives as
    // such. Before, they were left out, and every OBJ material was white. The texture is multiplied
    // by the color, and made linear from sRGB; a color beyond 1 is clamped to it.
    write_file("diffuse.png", png(204, 102, 51));
    write_file("diffuse.mtl",
               "newmtl Colored\nKd 0.8 0.4 0.2\n"
               "newmtl Textured\nKd 0.5 1 1\nmap_Kd diffuse.png\n"
               "newmtl TooBright\nKd 1.5 0.5 0.25\n");
    const fs::path path = write_file("diffuse.obj",
                                     "mtllib diffuse.mtl\n"
                                     "v 0 0 0\nv 1 0 0\nv 0 1 0\nvt 0 0\nvt 1 0\nvt 0 1\n"
                                     "o Colored\nusemtl Colored\nf 1/1 2/2 3/3\n"
                                     "o Textured\nusemtl Textured\nf 1/1 2/2 3/3\n"
                                     "o TooBright\nusemtl TooBright\nf 1/1 2/2 3/3\n");
    Scene<RGB> scene;
    scene.load_model(path);
    check_bins(values_of(scene, "Colored").albedo, RGB{0.8f, 0.4f, 0.2f});
    check_bins(
        values_of(scene, "Textured").albedo, linear(204, 102, 51) * RGB{0.5f, 1.f, 1.f}, 1e-5f);
    check_bins(values_of(scene, "TooBright").albedo, RGB{1.f, 0.5f, 0.25f});

    // An OBJ model with no materials is given ASSIMP's default, whose diffuse color is gray, but
    // stays white, as a glTF model with no materials is:
    Scene<RGB> plain;
    plain.load_model(write_file("no_mtl.obj", "o Plain\nv 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n"));
    check_bins(values_of(plain, "Plain").albedo, RGB{1.f});
}

TEST_CASE("A model's textures are multiplied by their factors", "[assets][models]")
{
    // Color is stored as sRGB, and made linear; the metallic-roughness texture holds roughness in
    // its green channel and metalness in its blue one, as data, not colors.
    write_file("base_color.png", png(204, 102, 51, 153));
    write_file("metallic_roughness.png", png(0, 153, 51));
    write_file("normals.png", png(204, 128, 204)); // tilted toward the tangent
    Gltf gltf;
    gltf.images = {image_file("base_color.png"),
                   image_file("metallic_roughness.png"),
                   image_file("normals.png")};
    gltf.materials = {R"({"pbrMetallicRoughness": {"baseColorFactor": [0.5, 1, 1, 0.5],
                           "baseColorTexture": {"index": 0}, "metallicFactor": 1,
                           "roughnessFactor": 0.5, "metallicRoughnessTexture": {"index": 1}},
                          "normalTexture": {"index": 2}})"};
    Scene<RGB> scene;
    scene.load_model(gltf.write("textured.gltf"));

    const auto material = material_of(scene, "Mesh0");
    const MaterialEval<RGB> eval = material->evaluate(flat_surface());
    check_bins(eval.params.albedo, linear(204, 102, 51) * RGB{0.5f, 1.f, 1.f}, 1e-5f);
    CHECK_THAT(eval.params.opacity, WithinAbs(b8(153) * 0.5f, 1e-6f));
    CHECK_THAT(eval.params.roughness, WithinAbs(b8(153) * 0.5f, 1e-6f));
    CHECK_THAT(eval.params.metallic, WithinAbs(b8(51), 1e-6f));
    // The normal map turns the shading normal away from the surface's:
    CHECK(eval.isect.normal_s.z < 0.9f);
}

TEST_CASE("A glTF model's emissive texture is multiplied by its factor", "[assets][models]")
{
    // Before, the loader looked for the texture as ASSIMP's EMISSION_COLOR, but ASSIMP gives
    // glTF's as EMISSIVE: it was never loaded, and the factor alone was the emission, all over
    // the surface. It is a color, stored as sRGB, and made linear.
    write_file("emissive.png", png(204, 102, 51));
    Gltf gltf;
    gltf.images = {image_file("emissive.png")};
    gltf.materials = {R"({"emissiveTexture": {"index": 0}, "emissiveFactor": [1, 0.5, 1]})"};
    Scene<RGB> scene;
    scene.load_model(gltf.write("emissive.gltf"));

    check_bins(
        values_of(scene, "Mesh0").emission, linear(204, 102, 51) * RGB{1.f, 0.5f, 1.f}, 1e-5f);
    check_bins(material_of(scene, "Mesh0")->emissive_factor(), RGB{1.f, 0.5f, 1.f});
}

TEST_CASE("Textures in each image format the loader reads are loaded", "[assets][models]")
{
    // sRGB images (PNG, JPEG, TGA and BMP, unless a PNG says otherwise) are made linear; HDR
    // images are linear already.
    write_file("format.png", png(204, 102, 51));
    write_file("format_linear.png", png(204, 102, 51, std::nullopt, ColorSpaceHint::Linear));
    write_file("format_upper.PNG", png(204, 102, 51));
    write_file("format.jpg", jpeg(204, 102, 51));
    write_file("format.tga", tga(204, 102, 51));
    write_file("format.bmp", bmp(204, 102, 51));
    write_file("format.hdr", hdr());
    const std::vector<std::string> files = {"format.png",
                                            "format_linear.png",
                                            "format_upper.PNG",
                                            "format.jpg",
                                            "format.tga",
                                            "format.bmp",
                                            "format.hdr"};
    Gltf gltf;
    gltf.materials.clear();
    for (std::size_t i = 0; i < files.size(); ++i) {
        gltf.images.push_back(image_file(files[i]));
        gltf.materials.push_back(R"({"pbrMetallicRoughness": {"baseColorTexture": {"index": )" +
                                 std::to_string(i) + "}}}");
    }
    Scene<RGB> scene;
    scene.load_model(gltf.write("formats.gltf"));

    const RGB srgb = linear(204, 102, 51);
    check_bins(values_of(scene, "Mesh0").albedo, srgb, 1e-5f);
    check_bins(values_of(scene, "Mesh1").albedo, RGB{b8(204), b8(102), b8(51)}, 1e-5f);
    check_bins(values_of(scene, "Mesh2").albedo, srgb, 1e-5f);
    check_bins(values_of(scene, "Mesh3").albedo, srgb, 0.02f); // JPEG is lossy
    check_bins(values_of(scene, "Mesh4").albedo, srgb, 1e-5f);
    check_bins(values_of(scene, "Mesh5").albedo, srgb, 1e-5f);
    check_bins(values_of(scene, "Mesh6").albedo, RGB{0.5f, 0.25f, 0.125f}, 1e-6f);

    // And embedded in the model (as metallic-roughness textures, data whose values are kept):
    Gltf embedded;
    embedded.images = {image_data("image/tga", tga(0, 153, 51)),
                       image_data("image/bmp", bmp(0, 153, 51)),
                       image_data("image/hdr", hdr())};
    embedded.materials.clear();
    for (std::size_t i = 0; i < embedded.images.size(); ++i) {
        embedded.materials.push_back(
            R"({"pbrMetallicRoughness": {"metallicRoughnessTexture": {"index": )" +
            std::to_string(i) + "}}}");
    }
    Scene<RGB> embedded_scene;
    embedded_scene.load_model(embedded.write("embedded_formats.gltf"));
    for (const std::string mesh : {"Mesh0", "Mesh1"}) {
        INFO(mesh);
        CHECK_THAT(values_of(embedded_scene, mesh).roughness, WithinAbs(b8(153), 1e-6f));
        CHECK_THAT(values_of(embedded_scene, mesh).metallic, WithinAbs(b8(51), 1e-6f));
    }
    CHECK(values_of(embedded_scene, "Mesh2").roughness == 0.25f);
    CHECK(values_of(embedded_scene, "Mesh2").metallic == 0.125f);
}

TEST_CASE("Textures embedded in a model are loaded as those in files are", "[assets][models]")
{
    // Before, only textures in files were made linear: an embedded color texture (as in every
    // .glb) was taken as linear, and came out brighter, 0.8 rather than 0.6 for 204 / 255.

    // As data in a glTF file (as in a .glb), the color and data textures of the last test:
    Gltf gltf;
    gltf.images = {image_data("image/png", png(204, 102, 51, 153)),
                   image_data("image/png", png(0, 153, 51)),
                   image_data("image/jpeg", jpeg(204, 128, 204))};
    gltf.materials = {R"({"pbrMetallicRoughness": {"baseColorTexture": {"index": 0},
                           "metallicFactor": 1, "roughnessFactor": 0.5,
                           "metallicRoughnessTexture": {"index": 1}},
                          "normalTexture": {"index": 2}})"};
    Scene<RGB> scene;
    scene.load_model(gltf.write("embedded.gltf"));

    const auto material = material_of(scene, "Mesh0");
    const MaterialEval<RGB> eval = material->evaluate(flat_surface());
    check_bins(eval.params.albedo, linear(204, 102, 51), 1e-5f);
    CHECK_THAT(eval.params.opacity, WithinAbs(b8(153), 1e-6f));
    CHECK_THAT(eval.params.roughness, WithinAbs(b8(153) * 0.5f, 1e-6f));
    CHECK_THAT(eval.params.metallic, WithinAbs(b8(51), 1e-6f));
    CHECK(eval.isect.normal_s.z < 0.9f);

    // And uncompressed, as some formats embed them:
    Scene<RGB> raw;
    const std::string raw_mesh =
        load_assbin(raw, write_assbin("raw_textures.assbin", false, false));
    const MaterialEval<RGB> raw_eval = material_of(raw, raw_mesh)->evaluate(flat_surface());
    check_bins(raw_eval.params.albedo, linear(204, 102, 51), 1e-5f);
    CHECK_THAT(raw_eval.params.opacity, WithinAbs(b8(153), 1e-6f));
    CHECK_THAT(raw_eval.params.roughness, WithinAbs(b8(153), 1e-6f));
    CHECK_THAT(raw_eval.params.metallic, WithinAbs(b8(51), 1e-6f));
    CHECK(raw_eval.isect.normal_s.z < 0.9f);
}

TEST_CASE("A texture used by several materials is loaded once", "[assets][models]")
{
    write_file("shared_color.png", png(204, 102, 51, 153));
    write_file("shared_metallic_roughness.png", png(0, 153, 51));
    Gltf gltf;
    gltf.images = {image_file("shared_color.png"), image_file("shared_metallic_roughness.png")};
    const std::string material = R"({"pbrMetallicRoughness": {"baseColorTexture": {"index": 0},
                                     "metallicRoughnessTexture": {"index": 1}}})";
    gltf.materials = {material, material};
    Scene<RGB> scene;
    scene.load_model(gltf.write("shared_textures.gltf"));

    const auto first = material_of(scene, "Mesh0");
    const auto second = material_of(scene, "Mesh1");
    REQUIRE(first != second);
    CHECK(first->albedo_image_ == second->albedo_image_);
    CHECK(first->alpha_image_ == second->alpha_image_);
    CHECK(first->roughness_image_ == second->roughness_image_);
    CHECK(first->metallic_image_ == second->metallic_image_);
    CHECK(first->roughness_image_ != first->metallic_image_);
}

TEST_CASE("A model's textures that can't be read are left out, and the model loads",
          "[assets][models]")
{
    // Each material's base color factor is then its albedo.
    write_file("not_an_image.png", "this is not an image");
    write_file("unsupported.gif", "GIF89a");
    Gltf gltf;
    gltf.images = {image_file("missing.png"),
                   image_file("not_an_image.png"),
                   image_file("unsupported.gif"),
                   image_data("image/png", Bytes{'n', 'o', 't', ' ', 'P', 'N', 'G'}),
                   image_data("image/ktx2", Bytes{'K', 'T', 'X'})};
    gltf.materials.clear();
    for (std::size_t i = 0; i < gltf.images.size(); ++i) {
        gltf.materials.push_back(
            R"({"pbrMetallicRoughness": {"baseColorFactor": [0.5, 0.5, 0.5, 1],
                "baseColorTexture": {"index": )" +
            std::to_string(i) + "}}}");
    }
    Scene<RGB> scene;
    REQUIRE_NOTHROW(scene.load_model(gltf.write("unreadable_textures.gltf")));
    for (std::size_t i = 0; i < gltf.images.size(); ++i) {
        INFO("image " << i);
        const ShadingParams<RGB> p = values_of(scene, "Mesh" + std::to_string(i));
        check_bins(p.albedo, RGB{0.5f});
        CHECK(p.opacity == 1.f);
    }
}

TEST_CASE("A model prints its graph, and finds its materials", "[assets][models]")
{
    Gltf gltf;
    gltf.materials = {R"({"name": "First"})", R"({"name": "Second"})"};
    const fs::path path = gltf.write("graph.gltf");
    Scene<RGB> scene;
    auto model = scene.load_model(path);

    std::ostringstream printed;
    std::streambuf* const cout_buffer = std::cout.rdbuf(printed.rdbuf());
    model.print_graph();
    std::cout.rdbuf(cout_buffer);
    CHECK_THAT(printed.str(), ContainsSubstring("Source: " + path.string()));
    CHECK_THAT(printed.str(), ContainsSubstring("Model Graph:"));
    CHECK_THAT(printed.str(), ContainsSubstring("Node0"));
    CHECK_THAT(printed.str(), ContainsSubstring("Node1"));

    const auto second = material_of(scene, "Mesh1");
    CHECK(model.get_material_by_id(second->id()).id() == second->id());
    CHECK_THROWS(model.get_material_by_id(scene.new_material(scene.new_bsdf_lambertian()).id()));

    // A model with no graph (as it is before it is loaded) has nothing to find:
    Model<RGB> empty;
    std::ostringstream printed_empty;
    std::cout.rdbuf(printed_empty.rdbuf());
    empty.print_graph();
    std::cout.rdbuf(cout_buffer);
    CHECK_THAT(printed_empty.str(), ContainsSubstring("(empty)"));
    CHECK_THROWS(empty.get_material_by_id(second->id()));
    CHECK_THROWS(empty.set_all_bsdfs(scene.new_bsdf_null()));
}

TEST_CASE("A model's BSDFs can all be replaced at once", "[assets][models]")
{
    Gltf gltf;
    gltf.materials = {"{}", "{}"};
    Scene<RGB> scene;
    auto model = scene.load_model(gltf.write("bsdfs.gltf"));

    const Vec3<float> up{0.f, 0.f, 1.f};
    const auto reflected = [&](const std::string& mesh) {
        const auto material = material_of(scene, mesh);
        return material->bsdf_eval(up, up, material->evaluate(flat_surface())).total();
    };
    REQUIRE(reflected("Mesh0") > 0.f); // Cook-Torrance, as loaded
    REQUIRE(reflected("Mesh1") > 0.f);

    model.set_all_bsdfs(scene.new_bsdf_null());
    CHECK(reflected("Mesh0") == 0.f);
    CHECK(reflected("Mesh1") == 0.f);
}
