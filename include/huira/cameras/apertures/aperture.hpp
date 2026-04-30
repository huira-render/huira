#pragma once

#include <memory>

#include "huira/core/types.hpp"
#include "huira/images/image.hpp"
#include "huira/cameras/psfs/psf.hpp"
#include "huira/concepts/spectral_concepts.hpp"
#include "huira/units/units.hpp"
#include "huira/render/sampler.hpp"

namespace huira {

    /**
     * @brief Abstract base class for optical apertures.
     *
     * Defines the interface for all aperture types, including area management and PSF creation.
     *
     * @tparam TSpectral The spectral representation type.
     */
    template <IsSpectral TSpectral>
    class Aperture {
    public:
        virtual ~Aperture() = default;

        virtual units::SquareMeter get_area() const = 0;
        virtual void set_area(units::SquareMeter area) = 0;

        virtual Vec2<float> sample(Sampler<float>& sample) const = 0;

        virtual std::unique_ptr<PSF<TSpectral>> make_psf(
            units::Meter focal_length,
            units::Meter pitch_x,
            units::Meter pitch_y,
            int radius, int banks) = 0;

        void build_defocus_kernel(
            units::Diopter defocus,
            units::Meter focal_length,
            units::Meter pitch_x,
            units::Meter pitch_y,
            int banks);

        const Image<float>& get_defocus_kernel(float u, float v) const;

        float get_defocus_radius() const { return defocus_cache_.radius; }
        int get_defocus_half_extent() const { return defocus_cache_.half_extent; }
        bool has_defocus() const { return defocus_cache_.radius > 0; }

        virtual units::Meter get_bounding_radius() const = 0;

    protected:
        struct PolyphaseCache {
            float radius = 0.f;
            int half_extent = 0;
            int banks = 0;
            int dim = 0;
            std::vector<Image<float>> kernels;
        } defocus_cache_;

        virtual void rasterize_kernel_(Image<float>& kernel, float radius_pixels, float offset_x, float offset_y) = 0;

    private:
        void generate_polyphase_data_();

    };

    template <typename T>
    struct is_aperture : std::false_type {};

    template <template <typename> class Derived, typename TSpectral>
        requires std::derived_from<Derived<TSpectral>, Aperture<TSpectral>>
    struct is_aperture<Derived<TSpectral>> : std::true_type {};

    template <typename T>
    concept IsAperture = is_aperture<T>::value;
}

#include "huira_impl/cameras/apertures/aperture.ipp"
