// these are just utility classes to abstract the complexities of the PBR Shaders.

#pragma once

#include <map>
#include <string>
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
			Scene* scene,
			Pass* pass,
			std::map<std::string, std::string> macros = {});
		~PbrMaterial();
	};

	struct PbrUniform {
		glm::vec4 albedo;
		glm::vec4 emissive;
		glm::float32 metallic;
		glm::float32 roughness;
	};

	struct TexturelessPbrMaterial {
		VulkanMaterial* mat;
		VulkanMaterialInstance* matInstance;
		VulkanUniform* uniform;

		TexturelessPbrMaterial(
			PbrUniform uniform,
			Scene* scene,
			Pass* pass,
			std::map<std::string, std::string> macros = {});
		~TexturelessPbrMaterial();
	};
}