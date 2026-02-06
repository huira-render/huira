#pragma once

#include <vector>

#include "huira/core/concepts/spectral_concepts.hpp"
#include "huira/images/image.hpp"

namespace huira {
    template <IsSpectral TSpectral>
    class PSF {
    public:
        PSF() = default;
        virtual ~PSF() = default;

        virtual TSpectral evaluate(float x, float y) = 0;

        void build_polyphase_cache(int radius, int banks);

        const Image<TSpectral>& get_kernel(float u, float v) const;
        std::vector<Image<TSpectral>> get_all_kernels() const { return cache_.kernels; }

        int get_radius() const { return cache_.radius; }

    protected:
        struct PolyphaseCache {
            int radius = 0;
            int dim = 0;
            int banks = 0;
            std::vector<Image<TSpectral>> kernels;
        } cache_;

    private:
        void generate_polyphase_data_();
        void normalize_kernel_(Image<TSpectral>& kernel, const TSpectral& total_energy);
    };

    template <typename T>
    struct is_psf : std::false_type {};

    template <template <typename> class Derived, typename TSpectral>
        requires std::derived_from<Derived<TSpectral>, PSF<TSpectral>>
    struct is_psf<Derived<TSpectral>> : std::true_type {};

    template <typename T>
    concept IsPSF = is_psf<T>::value;
}

#include "huira_impl/cameras/psf/psf.ipp"
