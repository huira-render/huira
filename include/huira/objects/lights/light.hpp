//#pragma once
//
//#include <string>
//#include <memory>
//#include <random>
//
//#include "huira/core/types.hpp"
//#include "huira/detail/sampler.hpp"
//#include "huira/detail/concepts/numeric_concepts.hpp"
//#include "huira/detail/concepts/spectral_concepts.hpp"
//#include "huira/objects/scene_graph/node.hpp"
//
//namespace huira {
//    // Forward declare:
//    template <IsSpectral TSpectral>
//    class Scene;
//
//
//
//    enum class LightType {
//        Point,
//        Sphere
//    };
//
//    template <IsSpectral TSpectral>
//    struct LightSample {
//        Vec3<float> wi;
//        TSpectral Li;
//        float distance;
//        float pdf;
//    };
//
//    template <IsSpectral TSpectral>
//    class Light : public Node<TSpectral> {
//    public:
//        explicit Light(Scene<TSpectral>* scene)
//            : Node<TSpectral>(scene)
//        {
//
//        }
//
//        // Delete copying:
//        Light(const Light&) = delete;
//        Light& operator=(const Light&) = delete;
//
//        ~Light() override = default;
//
//        virtual LightSample<TSpectral> sample_Li(const Vec3<float>& point, Sampler<float>& sampler) const = 0;
//
//        virtual float pdf_Li(const Vec3<float>& point, const Vec3<float>& wi) const = 0;
//
//        virtual LightType get_type() const = 0;
//
//        std::string get_type_name() const override = 0;
//    };
//}
//
