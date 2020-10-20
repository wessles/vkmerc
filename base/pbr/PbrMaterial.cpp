#include "PbrMaterial.h"

#include <vulkan/vulkan.h>

#include "../VulkanShader.h"
#include "../VulkanUniform.h"
#include "../scene/Scene.h"

namespace vku {
	PbrMaterial::PbrMaterial(
		VulkanTexture* albedo,
		VulkanTexture* normal,
		VulkanTexture* metallicRoughness,
		VulkanTexture* emissive,
		VulkanTexture* ao,
		VulkanTexture* spec_ibl,
		VulkanTexture* diffuse_ibl,
		VulkanTexture* brdf_lut,
		Scene* scene,
		Pass* pass,
		std::vector<ShaderMacro> macros) {

		VulkanMaterialInfo info(scene->device);
		info.shaderStages.push_back({ "res/shaders/pbr/pbr.vert", VK_SHADER_STAGE_VERTEX_BIT, macros });
		info.shaderStages.push_back({ "res/shaders/pbr/pbr.frag", VK_SHADER_STAGE_FRAGMENT_BIT, macros });
		this->mat = new VulkanMaterial(&info, scene, pass);
		this->matInstance = new VulkanMaterialInstance(mat);

		for (uint32_t i = 0; i < matInstance->descriptorSets.size(); i++) {
			this->matInstance->descriptorSets[i]->write(0, albedo);
			this->matInstance->descriptorSets[i]->write(1, normal);
			this->matInstance->descriptorSets[i]->write(2, metallicRoughness);
			this->matInstance->descriptorSets[i]->write(3, emissive);
			this->matInstance->descriptorSets[i]->write(4, ao);
			this->matInstance->descriptorSets[i]->write(5, spec_ibl);
			this->matInstance->descriptorSets[i]->write(6, diffuse_ibl);
			this->matInstance->descriptorSets[i]->write(7, brdf_lut);
		}
	}
	PbrMaterial::~PbrMaterial() {
		delete mat;
		delete matInstance;
	}

	TexturelessPbrMaterial::TexturelessPbrMaterial(
		PbrUniform data,
		VulkanTexture* spec_ibl,
		VulkanTexture* diffuse_ibl,
		VulkanTexture* brdf_lut,
		Scene* scene,
		Pass* pass,
		std::vector<ShaderMacro> macros) {

		uniform = new VulkanUniform(scene->device, sizeof(PbrUniform));
		uniform->write(&data);

		VulkanMaterialInfo info(scene->device);
		info.shaderStages.push_back({ "res/shaders/pbr/pbr.vert", VK_SHADER_STAGE_VERTEX_BIT, macros });
		info.shaderStages.push_back({ "res/shaders/pbr/pbr_textureless.frag", VK_SHADER_STAGE_FRAGMENT_BIT, macros });
		this->mat = new VulkanMaterial(&info, scene, pass);
		this->matInstance = new VulkanMaterialInstance(mat);

		for (uint32_t i = 0; i < matInstance->descriptorSets.size(); i++) {
			this->matInstance->descriptorSets[i]->write(0, uniform);
			this->matInstance->descriptorSets[i]->write(1, spec_ibl);
			this->matInstance->descriptorSets[i]->write(2, diffuse_ibl);
			this->matInstance->descriptorSets[i]->write(3, brdf_lut);
		}
	}

	TexturelessPbrMaterial::~TexturelessPbrMaterial() {
		delete mat;
		delete matInstance;
		delete uniform;
	}
}