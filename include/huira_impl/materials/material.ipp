namespace huira {
    /**
     * @brief Evaluate all material textures at the given interaction point.
     *
     * Samples all image slots (no branching), applies scalar factors,
     * incorporates vertex albedo, and perturbs the shading normal from
     * the normal map. Returns everything the BSDF needs on the stack.
     *
     * @param isect The surface interaction from the ray-geometry hit
     * @return EvalResult with ShadingParams and modified Interaction
     */
    template <IsSpectral TSpectral>
    MaterialEval Material<TSpectral>::evaluate(const Interaction<TSpectral>& isect) const {
        const Vec2<float>& uv = isect.uv;

        ShadingParams<TSpectral> params;

        params.base_color = base_color_image->sample_bilinear(uv.x, uv.y)
            * base_color_factor
            * isect.vertex_albedo;

        params.metallic = metallic_image->sample_bilinear(uv.x, uv.y)
            * metallic_factor;

        params.roughness = roughness_image->sample_bilinear(uv.x, uv.y)
            * roughness_factor;

        // Normal mapping (unconditional — default 1x1 image yields {0,0,1}
        // after remap, which leaves the shading normal unchanged)
        Vec3<float> ts_normal = normal_image->sample_bilinear(uv.x, uv.y);
        ts_normal = ts_normal * 2.0f - Vec3<float>{ 1.0f };
        ts_normal.x *= normal_scale;
        ts_normal.y *= normal_scale;
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

        MaterialEval result{};
        result.params = params;
        result.isect = shading_isect;
        return result;
    }

    template <IsSpectral TSpectral>
    bool Material<TSpectral>::is_emissive() const noexcept {
        return emissive_factor.total() > 0.0f;
    }


    /**
     * @brief Evaluate emitted radiance at a surface point.
     */
    template <IsSpectral TSpectral>
    TSpectral Material<TSpectral>::emitted(const Interaction<TSpectral>& isect) const {
        if (!is_emissive()) {
            return TSpectral{ 0 };
        }
        return emissive_image->sample_bilinear(isect.uv.x, isect.uv.y) * emissive_factor;
    }

    template <IsSpectral TSpectral>
    TSpectral Material<TSpectral>::bsdf_eval(
        const Vec3<float>& wo,
        const Vec3<float>& wi,
        const MaterialEval& eval) const
    {
        return bsdf->eval(wo, wi, eval.isect, eval.params);
    }

    template <IsSpectral TSpectral>
    TSpectral Material<TSpectral>::bsdf_sample(
        const Vec3<float>& wo,
        const MaterialEval& eval,
        float u1, float u2) const
    {
        return bsdf->sample(wo, eval.isect, eval.params, u1, u2);
    }

    template <IsSpectral TSpectral>
    float Material<TSpectral>::bsdf_pdf(
        const Vec3<float>& wo,
        const Vec3<float>& wi,
        const MaterialEval& eval) const
    {
        return bsdf->pdf(wo, wi, eval.isect, eval.params);
    }
}
