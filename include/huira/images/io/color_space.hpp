#pragma once

#include <cmath>

namespace huira {
enum class ColorSpace { Linear, sRGB, Gamma };

inline float srgb_to_linear(float s)
{
    if (s <= 0.04045f) {
        return s / 12.92f;
    }
    return std::pow((s + 0.055f) / 1.055f, 2.4f);
}

// Simple gamma to linear
inline float gamma_to_linear(float encoded, float decoding_gamma)
{
    return std::pow(encoded, decoding_gamma);
}

inline float linear_to_srgb(float linear)
{
    if (linear <= 0.0031308f) {
        return linear * 12.92f;
    }
    return 1.055f * std::pow(linear, 1.0f / 2.4f) - 0.055f;
}

inline ImageBundle<RGB> linear_to_srgb(const ImageBundle<RGB>& linear_bundle)
{
    Image<RGB> srgb_image(linear_bundle.image.width(), linear_bundle.image.height());
    for (int y = 0; y < linear_bundle.image.height(); ++y) {
        for (int x = 0; x < linear_bundle.image.width(); ++x) {
            const RGB& linear_pixel = linear_bundle.image(x, y);
            srgb_image(x, y) = RGB{linear_to_srgb(linear_pixel[0]),
                                   linear_to_srgb(linear_pixel[1]),
                                   linear_to_srgb(linear_pixel[2])};
        }
    }
    ImageBundle<RGB> output_bundle(std::move(srgb_image), linear_bundle.alpha);
    output_bundle.color_space = ColorSpaceHint::sRGB;
    return output_bundle;
}

inline ImageBundle<float> linear_to_srgb(const ImageBundle<float>& linear_bundle)
{
    Image<float> srgb_image(linear_bundle.image.width(), linear_bundle.image.height());
    for (int y = 0; y < linear_bundle.image.height(); ++y) {
        for (int x = 0; x < linear_bundle.image.width(); ++x) {
            const float& linear_pixel = linear_bundle.image(x, y);
            srgb_image(x, y) = linear_to_srgb(linear_pixel);
        }
    }
    ImageBundle<float> output_bundle(std::move(srgb_image), linear_bundle.alpha);
    output_bundle.color_space = ColorSpaceHint::sRGB;
    return output_bundle;
}

inline ImageBundle<RGB> srgb_to_linear(const ImageBundle<RGB>& srgb_bundle)
{
    Image<RGB> linear_image(srgb_bundle.image.width(), srgb_bundle.image.height());
    for (int y = 0; y < srgb_bundle.image.height(); ++y) {
        for (int x = 0; x < srgb_bundle.image.width(); ++x) {
            const RGB& srgb_pixel = srgb_bundle.image(x, y);
            linear_image(x, y) = RGB{srgb_to_linear(srgb_pixel[0]),
                                     srgb_to_linear(srgb_pixel[1]),
                                     srgb_to_linear(srgb_pixel[2])};
        }
    }
    ImageBundle<RGB> output_bundle(std::move(linear_image), srgb_bundle.alpha);
    output_bundle.color_space = ColorSpaceHint::Linear;
    return output_bundle;
}

inline ImageBundle<float> srgb_to_linear(const ImageBundle<float>& srgb_bundle)
{
    Image<float> linear_image(srgb_bundle.image.width(), srgb_bundle.image.height());
    for (int y = 0; y < srgb_bundle.image.height(); ++y) {
        for (int x = 0; x < srgb_bundle.image.width(); ++x) {
            const float& srgb_pixel = srgb_bundle.image(x, y);
            linear_image(x, y) = srgb_to_linear(srgb_pixel);
        }
    }
    ImageBundle<float> output_bundle(std::move(linear_image), srgb_bundle.alpha);
    output_bundle.color_space = ColorSpaceHint::Linear;
    return output_bundle;
}
} // namespace huira
