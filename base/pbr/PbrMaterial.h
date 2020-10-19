// these are just utility classes to abstract the complexities of the PBR Shaders.

#pragma once

#include <glm/glm.hpp>

#include "../VulkanTexture.h"
#include "../VulkanMaterial.h"


namespace vku {
	struct PbrMaterial {
		VulkanMaterial* mat;
		VulkanMaterialInstance* matInstance;

		PbrMaterial(
			VulkanTexture* albedo,
			VulkanTexture* normal,
			VulkanTexture* metallicRoughness,
			VulkanTexture* emissive,
			VulkanTexture* ao,
			VulkanTexture* spec_ibl,
			VulkanTexture* diffuse_ibl,
			VulkanTexture* brdf_lut,
			Scene* scene,
			Pass* pass);
		~PbrMaterial();
	};

	struct PbrUniform {
		glm::vec4 albedo;
		glm::vec4 emissive;
		glm::float32 metallic;
		glm::float32 roughnesss;
	};

	struct TexturelessPbrMaterial {
		VulkanMaterial* mat;
		VulkanMaterialInstance* matInstance;
		VulkanUniform* uniform;

		TexturelessPbrMaterial(
			PbrUniform uniform,
			VulkanTexture* spec_ibl,
			VulkanTexture* diffuse_ibl,
			VulkanTexture* brdf_lut,
			Scene* scene,
			Pass* pass);
		~TexturelessPbrMaterial();
	};
}