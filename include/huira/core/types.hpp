#pragma once

#include <string>
#include <sstream>
#include <vector>
#include <cstdint>

#define GLM_ENABLE_EXPERIMENTAL
#include "glm/glm.hpp"
#include "glm/gtx/io.hpp"
#include <glm/gtx/string_cast.hpp>

#include "huira/core/concepts/numeric_concepts.hpp"
#include "huira/core/concepts/spectral_concepts.hpp"

namespace huira {
    template <int N, IsFloatingPoint T>
    using Vec = glm::vec<N, T, glm::highp>;

    template <IsFloatingPoint T>
    using Vec2 = Vec<2, T>;

    template <IsFloatingPoint T>
    using Vec3 = Vec<3, T>;

    template <IsFloatingPoint T>
    using Vec4 = Vec<4, T>;

    template <int N, IsFloatingPoint T>
    std::string vec_to_string(const glm::vec<N, T, glm::highp>& v) {
        return glm::to_string(v);
    }


    template <int Rows, int Cols, IsFloatingPoint T>
    using Mat = glm::mat<Cols, Rows, T, glm::highp>;

    template <IsFloatingPoint T>
    using Mat2 = Mat<2, 2, T>;

    template <IsFloatingPoint T>
    using Mat3 = Mat<3, 3, T>;

    template <IsFloatingPoint T>
    using Mat4 = Mat<4, 4, T>;

    template <int Rows, int Cols, IsFloatingPoint T>
    std::string mat_to_string(const glm::mat<Cols, Rows, T, glm::highp>& m) {
        return glm::to_string(m);
    }


    // Quaternion aliases
    template<IsFloatingPoint T>
    using Quaternion = glm::qua<T, glm::highp>;  // GLM/Hamilton: (w, x, y, z)

    template <IsFloatingPoint T>
    using ShusterQuaternion = Vec4<T>;  // Shuster: (x, y, z, w) - common in aerospace

    template <IsFloatingPoint T>
    ShusterQuaternion<T> to_shuster(const Quaternion<T>& q) {
        return ShusterQuaternion<T>(q.x, q.y, q.z, q.w);
    }

    template <IsFloatingPoint T>
    Quaternion<T> to_hamilton(const ShusterQuaternion<T>& q) {
        return Quaternion<T>(q.w, q.x, q.y, q.z);
    }


    // Helpful typedefs:
    typedef Mat2<float> Mat2_f;
    typedef Mat2<double> Mat2_d;

    typedef Mat3<float> Mat3_f;
    typedef Mat3<double> Mat3_d;

    typedef Mat4<float> Mat4_f;
    typedef Mat4<double> Mat4_d;

    typedef Vec2<float> Vec2_f;
    typedef Vec2<double> Vec2_d;

    typedef Vec3<float> Vec3_f;
    typedef Vec3<double> Vec3_d;

    typedef Vec4<float> Vec4_f;
    typedef Vec4<double> Vec4_d;

    typedef Quaternion<float> Quaternion_f;
    typedef Quaternion<double> Quaternion_d;

    typedef ShusterQuaternion<float> ShusterQuaternion_f;
    typedef ShusterQuaternion<double> ShusterQuaternion_d;


    template <IsFloatingPoint TFloat>
    struct BasePixel {
        TFloat x = TFloat{ 0 };
        TFloat y = TFloat{ 0 };

        constexpr BasePixel() noexcept = default;
        constexpr BasePixel(TFloat x_val, TFloat y_val) noexcept : x(x_val), y(y_val) {}

        [[nodiscard]] constexpr TFloat& operator[](std::size_t i) noexcept { return i == 0 ? x : y; }
        [[nodiscard]] constexpr const TFloat& operator[](std::size_t i) const noexcept { return i == 0 ? x : y; }

        constexpr BasePixel operator+(const BasePixel& rhs) const noexcept { return { x + rhs.x, y + rhs.y }; }
        constexpr BasePixel operator-(const BasePixel& rhs) const noexcept { return { x - rhs.x, y - rhs.y }; }
        constexpr BasePixel operator*(TFloat s) const noexcept { return { x * s, y * s }; }

        friend constexpr BasePixel operator*(TFloat s, const BasePixel& p) noexcept { return p * s; }
    };

    using Pixel = BasePixel<float>;
    using Pixel_d = BasePixel<double>;

    struct Resolution {
        Resolution(int w, int h)
            : width(w), height(h), x(width), y(height) {
        }

        Resolution(const Resolution& other)
            : width(other.width), height(other.height), x(width), y(height) {
        }

        Resolution& operator=(const Resolution& other) {
            if (this != &other) {
                width = other.width;
                height = other.height;
                // Rebind x and y to the new width/height
                x = width;
                y = height;
            }
            return *this;
        }

        int width = 0;
        int height = 0;

        int& x = width;
        int& y = height;

        template <IsFloatingPoint TFloat>
        Vec2<TFloat> operator*(const Vec2<TFloat>& v) const {
            return Vec2<TFloat>(
                v.x * static_cast<TFloat>(width),
                v.y * static_cast<TFloat>(height));
        }

        template <IsFloatingPoint TFloat>
        Vec2<TFloat> operator*(TFloat s) const {
            return Vec2<TFloat>(
                static_cast<TFloat>(width) * s,
                static_cast<TFloat>(height) * s);
        }

        template <IsFloatingPoint TFloat>
        Vec2<float> operator/(const Vec2<TFloat>& v) const {
            return Vec2<TFloat>(
                static_cast<TFloat>(width) / v.x,
                static_cast<TFloat>(height) / v.y);
        }

        template <IsFloatingPoint TFloat>
        Vec2<float> operator/(TFloat s) const {
            return Vec2<TFloat>(
                static_cast<TFloat>(width) / s,
                static_cast<TFloat>(height) / s);
        }

        // Implement comparison operator:
        bool operator==(const Resolution res) const {
            return (width == res.width && height == res.height);
        }
    };

    template <IsFloatingPoint TFloat>
    inline Vec2<TFloat> operator*(const Vec2<TFloat>& v, const Resolution& res) {
        return Vec2<TFloat>(
            v.x * static_cast<TFloat>(res.width),
            v.y * static_cast<TFloat>(res.height));
    }

    template <IsFloatingPoint TFloat>
    inline Vec2<TFloat> operator*(TFloat s, const Resolution& res) {
        return Vec2<TFloat>(
            static_cast<TFloat>(res.width) * s,
            static_cast<TFloat>(res.height) * s);
    }

    template <IsFloatingPoint TFloat>
    inline Vec2<TFloat> operator/(const Vec2<TFloat>& v, const Resolution& res) {
        return Vec2<TFloat>(
            static_cast<TFloat>(res.width) / v.x,
            static_cast<TFloat>(res.height) / v.y);
    }

    template <IsFloatingPoint TFloat>
    inline Vec2<TFloat> operator/(TFloat s, const Resolution& res) {
        return Vec2<TFloat>(
            static_cast<TFloat>(res.width) / s,
            static_cast<TFloat>(res.height) / s);
    }


    template <IsSpectral TSpectral>
    struct Vertex {
        Vec3<float> position{};
        TSpectral albedo{ 1 };
        Vec3<float> normal{ 0 };
        Vec2<float> uv{ 0 };

        bool operator==(const Vertex& other) const {
            return position == other.position &&
                albedo == other.albedo &&
                normal == other.normal &&
                uv == other.uv;
        }
    };

    typedef std::vector<std::uint32_t> IndexBuffer;

    template <IsSpectral TSpectral>
    using VertexBuffer = std::vector<Vertex<TSpectral>>;
}
