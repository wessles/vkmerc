#include "PbrMaterial.h"

#include <vulkan/vulkan.h>

#include "../shader/ShaderVariant.h"
#include "../VulkanUniform.h"
#include "../scene/Scene.h"

namespace vku {
	PbrMaterial::PbrMaterial(
		VulkanTexture* albedo,
		VulkanTexture* normal,
		VulkanTexture* metallicRoughness,
		VulkanTexture* emissive,
		VulkanTexture* ao,
		Scene* scene,
		Pass* pass,
		std::map<std::string, std::string> macros) {

		VulkanMaterialInfo info;
		info.shaderStages.push_back({ "pbr/pbr_gbuf.vert", macros });
		info.shaderStages.push_back({ "pbr/pbr_gbuf.frag", macros });
		this->mat = new VulkanMaterial(&info, scene, pass);
		this->matInstance = new VulkanMaterialInstance(mat);

		for (uint32_t i = 0; i < matInstance->descriptorSets.size(); i++) {
			this->matInstance->descriptorSets[i]->write(0, albedo);
			this->matInstance->descriptorSets[i]->write(1, normal);
			this->matInstance->descriptorSets[i]->write(2, metallicRoughness);
			this->matInstance->descriptorSets[i]->write(3, emissive);
			this->matInstance->descriptorSets[i]->write(4, ao);
		}
	}
	PbrMaterial::~PbrMaterial() {
		delete mat;
		delete matInstance;
	}

	TexturelessPbrMaterial::TexturelessPbrMaterial(
		PbrUniform data,
		Scene* scene,
		Pass* pass,
		std::map<std::string, std::string> macros) {

		uniform = new VulkanUniform(scene->device, sizeof(PbrUniform));
		uniform->write(&data);

		macros["TEXTURELESS"] = "";

		VulkanMaterialInfo info{};
		info.shaderStages.push_back({ "pbr/pbr_gbuf.vert", macros });
		info.shaderStages.push_back({ "pbr/pbr_gbuf.frag", macros });
		this->mat = new VulkanMaterial(&info, scene, pass);
		this->matInstance = new VulkanMaterialInstance(mat);

		for (uint32_t i = 0; i < matInstance->descriptorSets.size(); i++) {
			this->matInstance->descriptorSets[i]->write(0, uniform);
		}
	}

	TexturelessPbrMaterial::~TexturelessPbrMaterial() {
		delete mat;
		delete matInstance;
		delete uniform;
	}
}