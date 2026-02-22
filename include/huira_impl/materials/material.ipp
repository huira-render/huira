namespace huira {
    /**
     * @brief Evaluate all material textures at the given interaction point.
     *
     * Samples all image slots (no branching), applies scalar factors,
     * incorporates vertex albedo, and perturbs the shading normal from
     * the normal map. Returns everything the BSDF needs on the stack.
     *
     * @param isect The surface interaction from the ray-geometry hit
     * @return MaterialEval<TSpectral> with ShadingParams and modified Interaction
     */
    template <IsSpectral TSpectral>
    MaterialEval<TSpectral> Material<TSpectral>::evaluate(const Interaction<TSpectral>& isect) const {
        const Vec2<float>& uv = isect.uv;

        ShadingParams<TSpectral> params;

        params.albedo = albedo_image_->sample_bilinear(uv.x, uv.y) * albedo_factor_ * isect.vertex_albedo;

        params.metallic = metallic_image_->sample_bilinear(uv.x, uv.y) * metallic_factor_;

        params.roughness = roughness_image_->sample_bilinear(uv.x, uv.y) * roughness_factor_;

        params.emission = emissive_image_->sample_bilinear(isect.uv.x, isect.uv.y) * emissive_factor_;

        // Normal mapping (unconditional — default 1x1 image yields {0,0,1}
        // after remap, which leaves the shading normal unchanged)
        Vec3<float> ts_normal = normal_image_->sample_bilinear(uv.x, uv.y);
        ts_normal = ts_normal * 2.0f - Vec3<float>{ 1.0f };
        ts_normal.x *= normal_scale_;
        ts_normal.y *= normal_scale_;
        ts_normal = glm::normalize(ts_normal);

        Interaction<TSpectral> shading_isect = isect;

        Vec3<float> perturbed =
            isect.tangent * ts_normal.x +
            isect.bitangent * ts_normal.y +
            isect.normal_s * ts_normal.z;

        shading_isect.normal_s = glm::normalize(perturbed);
        build_default_tangent_frame(
            shading_isect.normal_s,
            shading_isect.tangent,
            shading_isect.bitangent);

        MaterialEval<TSpectral> result{};
        result.params = params;
        result.isect = shading_isect;
        return result;
    }

    template <IsSpectral TSpectral>
    TSpectral Material<TSpectral>::bsdf_eval(
        const Vec3<float>& wo,
        const Vec3<float>& wi,
        const MaterialEval<TSpectral>& eval) const
    {
        return bsdf_->eval(wo, wi, eval.isect, eval.params);
    }

    template <IsSpectral TSpectral>
    BSDFSample<TSpectral> Material<TSpectral>::bsdf_sample(
        const Vec3<float>& wo,
        const MaterialEval<TSpectral>& eval,
        float u1, float u2) const
    {
        return bsdf_->sample(wo, eval.isect, eval.params, u1, u2);
    }

    template <IsSpectral TSpectral>
    float Material<TSpectral>::bsdf_pdf(
        const Vec3<float>& wo,
        const Vec3<float>& wi,
        const MaterialEval<TSpectral>& eval) const
    {
        return bsdf_->pdf(wo, wi, eval.isect, eval.params);
    }

    template <IsSpectral TSpectral>
    void Material<TSpectral>::set_albedo(const Image<TSpectral>* albedo_image)
    {
        albedo_image_ = albedo_image;
    }

    template <IsSpectral TSpectral>
    void Material<TSpectral>::set_albedo_factor(TSpectral albedo_factor)
    {
        albedo_factor_ = albedo_factor;
    }

    template <IsSpectral TSpectral>
    void Material<TSpectral>::reset_albedo()
    {
        albedo_image_ = default_albedo_image_;
        albedo_factor_ = TSpectral{ 1.0f };
    }

    template <IsSpectral TSpectral>
    Material<TSpectral>::Material(
        std::unique_ptr<BSDF<TSpectral>> bsdf,
        Image<TSpectral>* albedo_image,
        Image<float>* metallic_image,
        Image<float>* roughness_image,
        Image<Vec3<float>>* normal_image,
        Image<TSpectral>* emissive_image) :
        bsdf_{ std::move(bsdf) },
        default_albedo_image_{ albedo_image },
        default_metallic_image_{ metallic_image },
        default_roughness_image_{ roughness_image },
        default_normal_image_{ normal_image },
        default_emissive_image_{ emissive_image },
        id_(next_id_++)
    {
        albedo_image_ = default_albedo_image_;
        metallic_image_ = default_metallic_image_;
        roughness_image_ = default_roughness_image_;
        normal_image_ = default_normal_image_;
        emissive_image_ = default_emissive_image_;
    };
}
