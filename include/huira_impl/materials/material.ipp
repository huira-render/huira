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

        if (eval_albedo_) {
            params.albedo = albedo_image_->sample_bilinear(uv.x, uv.y) * albedo_factor_ * isect.vertex_albedo;
        }

        if (eval_metallic_) {
            params.metallic = metallic_image_->sample_bilinear(uv.x, uv.y) * metallic_factor_;
        }

        if (eval_roughness_) {
            params.roughness = roughness_image_->sample_bilinear(uv.x, uv.y) * roughness_factor_;
        }

        params.emission = emissive_image_->sample_bilinear(uv.x, uv.y) * emissive_factor_;

        Interaction<TSpectral> shading_isect = isect;

        // TODO Fix the tangent space computations
        //if (eval_normal_) {
        //    if (isect.tangent != Vec3<float>{0.0f}) {
        //        Vec3<float> ts_normal = glm::normalize(normal_image_->sample_bilinear(uv.x, uv.y));

        //        ts_normal.x *= normal_factor_;
        //        ts_normal.y *= normal_factor_;
        //        ts_normal = glm::normalize(ts_normal);

        //        Vec3<float> perturbed =
        //            isect.tangent * ts_normal.x +
        //            isect.bitangent * ts_normal.y +
        //            isect.normal_s * ts_normal.z;
        //        shading_isect.normal_s = glm::normalize(perturbed);
        //    }
        //}

        // Keep the original tangent/bitangent, just orthogonalize against new normal
        shading_isect.tangent = glm::normalize(isect.tangent - glm::dot(isect.tangent, shading_isect.normal_s) * shading_isect.normal_s);
        shading_isect.bitangent = glm::cross(shading_isect.normal_s, shading_isect.tangent);

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
    void Material<TSpectral>::set_bsdf(const BSDF<TSpectral>& bsdf)
    {
        bsdf_ = bsdf.clone();

        // Cache the flags flat in the material memory layout
        auto reqs = bsdf_->requirements();
        eval_albedo_ = reqs.needs_albedo;
        eval_metallic_ = reqs.needs_metallic;
        eval_roughness_ = reqs.needs_roughness;
        eval_normal_ = reqs.needs_normal;
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
    void Material<TSpectral>::set_metallic_image(const Image<float>* metallic_image)
    {
        metallic_image_ = metallic_image;
    }

    template <IsSpectral TSpectral>
    void Material<TSpectral>::set_metallic_factor(float metallic_factor)
    {
        metallic_factor_ = metallic_factor;
    }

    template <IsSpectral TSpectral>
    void Material<TSpectral>::reset_metallic()
    {
        metallic_image_ = default_metallic_image_;
        metallic_factor_ = 1.0f;
    }

    template <IsSpectral TSpectral>
    void Material<TSpectral>::set_roughness_image(const Image<float>* roughness_image)
    {
        roughness_image_ = roughness_image;
    }

    template <IsSpectral TSpectral>
    void Material<TSpectral>::set_roughness_factor(float roughness_factor)
    {
        roughness_factor_ = roughness_factor;
    }

    template <IsSpectral TSpectral>
    void Material<TSpectral>::reset_roughness()
    {
        roughness_image_ = default_roughness_image_;
        roughness_factor_ = 1.0f;
    }

    template <IsSpectral TSpectral>
    void Material<TSpectral>::set_normal_image(const Image<Vec3<float>>* normal_image)
    {
        normal_image_ = normal_image;
    }

    template <IsSpectral TSpectral>
    void Material<TSpectral>::set_normal_factor(float normal_factor)
    {
        normal_factor_ = normal_factor;
    }

    template <IsSpectral TSpectral>
    void Material<TSpectral>::reset_normal()
    {
        normal_image_ = default_normal_image_;
        normal_factor_ = 1.0f;
    }

    template <IsSpectral TSpectral>

    void Material<TSpectral>::set_emissive_image(const Image<TSpectral>* emissive_image)
    {
        emissive_image_ = emissive_image;
    }

    template <IsSpectral TSpectral>
    void Material<TSpectral>::set_emissive_factor(TSpectral emissive_factor)
    {
        emissive_factor_ = emissive_factor;
    }

    template <IsSpectral TSpectral>
    void Material<TSpectral>::reset_emissive()
    {
        emissive_image_ = default_emissive_image_;
        emissive_factor_ = TSpectral{ 1.0f };
    }

    template <IsSpectral TSpectral>
    Material<TSpectral>::Material(
        const BSDF<TSpectral>& bsdf,
        const Image<TSpectral>* albedo_image,
        const Image<float>* metallic_image,
        const Image<float>* roughness_image,
        const Image<Vec3<float>>* normal_image,
        const Image<TSpectral>* emissive_image) :
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

        set_bsdf(bsdf);
    };
}
