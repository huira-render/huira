namespace huira {
    template <IsSpectral TSpectral>
    void MaterialHandle<TSpectral>::set_bsdf(const BSDF<TSpectral>& bsdf)
    {
        this->get_()->set_bsdf(bsdf);
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
    void MaterialHandle<TSpectral>::set_alpha(const TextureHandle<float>& alpha_texture)
    {
        this->get_()->set_alpha(alpha_texture.image());
    }

    template <IsSpectral TSpectral>
    void MaterialHandle<TSpectral>::set_alpha_factor(float alpha_factor)
    {
        this->get_()->set_alpha_factor(alpha_factor);
    }

    template <IsSpectral TSpectral>
    void MaterialHandle<TSpectral>::reset_alpha()
    {
        this->get_()->reset_alpha();
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
