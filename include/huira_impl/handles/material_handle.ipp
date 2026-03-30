namespace huira {
    template <IsSpectral TSpectral>
    void MaterialHandle<TSpectral>::set_bsdf(std::unique_ptr<BSDF<TSpectral>> bsdf)
    {
        this->get_()->set_bsdf(std::move(bsdf));
    }

    template <IsSpectral TSpectral>
    void MaterialHandle<TSpectral>::set_cook_torrance_bsdf()
    {
        this->get_()->set_cook_torrance_bsdf();
    }

    template <IsSpectral TSpectral>
    void MaterialHandle<TSpectral>::set_lambert_bsdf()
    {
        this->get_()->set_lambert_bsdf();
    }

    template <IsSpectral TSpectral>
    void MaterialHandle<TSpectral>::set_lommel_seeliger_bsdf()
    {
        this->get_()->set_lommel_seeliger_bsdf();
    }

    template <IsSpectral TSpectral>
    void MaterialHandle<TSpectral>::set_mcewen_bsdf()
    {
        this->get_()->set_mcewen_bsdf();
    }

    template <IsSpectral TSpectral>
    void MaterialHandle<TSpectral>::set_oren_nayar_bsdf()
    {
        this->get_()->set_oren_nayar_bsdf();
    }


    template <IsSpectral TSpectral>
    void MaterialHandle<TSpectral>::set_albedo(const TextureHandle<TSpectral>& albedo_texture)
    {
        this->get_()->set_albedo(albedo_texture.image());
    }

    template <IsSpectral TSpectral>
    void MaterialHandle<TSpectral>::set_albedo_factor(TSpectral albedo_factor)
    {
        this->get_()->set_albedo_factor(albedo_factor);
    }

    template <IsSpectral TSpectral>
    void MaterialHandle<TSpectral>::reset_albedo()
    {
        this->get_()->reset_albedo();
    }

    template <IsSpectral TSpectral>
    void MaterialHandle<TSpectral>::set_metallic_image(const TextureHandle<float>& metallic_texture)
    {
        this->get_()->set_metallic_image(metallic_texture.image());
    }

    template <IsSpectral TSpectral>
    void MaterialHandle<TSpectral>::set_metallic_factor(float metallic_factor)
    {
        this->get_()->set_metallic_factor(metallic_factor);
    }

    template <IsSpectral TSpectral>
    void MaterialHandle<TSpectral>::reset_metallic()
    {
        this->get_()->reset_metallic();
    }

    template <IsSpectral TSpectral>
    void MaterialHandle<TSpectral>::set_roughness_image(const TextureHandle<float>& roughness_texture)
    {
        this->get_()->set_roughness_image(roughness_texture.image());
    }

    template <IsSpectral TSpectral>
    void MaterialHandle<TSpectral>::set_roughness_factor(float roughness_factor)
    {
        this->get_()->set_roughness_factor(roughness_factor);
    }

    template <IsSpectral TSpectral>
    void MaterialHandle<TSpectral>::reset_roughness()
    {
        this->get_()->reset_roughness();
    }

    template <IsSpectral TSpectral>
    void MaterialHandle<TSpectral>::set_normal_image(const TextureHandle<Vec3<float>>& normal_texture)
    {
        this->get_()->set_normal_image(normal_texture.image());
    }

    template <IsSpectral TSpectral>
    void MaterialHandle<TSpectral>::set_normal_factor(float normal_factor)
    {
        this->get_()->set_normal_factor(normal_factor);
    }

    template <IsSpectral TSpectral>
    void MaterialHandle<TSpectral>::reset_normal()
    {
        this->get_()->reset_normal();
    }

    template <IsSpectral TSpectral>
    void MaterialHandle<TSpectral>::set_emissive_image(const TextureHandle<TSpectral>& emissive_texture)
    {
        this->get_()->set_emissive_image(emissive_texture.image());
    }

    template <IsSpectral TSpectral>
    void MaterialHandle<TSpectral>::set_emissive_factor(TSpectral emissive_factor)
    {
        this->get_()->set_emissive_factor(emissive_factor);
    }

    template <IsSpectral TSpectral>
    void MaterialHandle<TSpectral>::reset_emissive()
    {
        this->get_()->reset_emissive();
    }
}
