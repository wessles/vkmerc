/* This class is a stub for later implementation. */

#include "VulkanCompute.h"

#include <vector>
#include <string>

#include <vulkan/vulkan.h>
#include <spirv_cross/spirv_glsl.hpp>

#include "scene/Scene.h"
#include "rendergraph/RenderGraph.h"

#include "VulkanDevice.h"
#include "VulkanSwapchain.h"
#include "VulkanDescriptorSet.h"
#include "VulkanShader.h"
#include "VulkanTexture.h"
#include "VulkanMesh.h"


namespace vku {
	VulkanComputeInfo::VulkanComputeInfo(VulkanDevice* const device) {
		this->device = device;

		this->computeInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;

		VkPushConstantRange maxPushConst;
		maxPushConst.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT;
		maxPushConst.offset = 0;
		maxPushConst.size = 128;
		pushConstRanges.push_back(maxPushConst);
	}
	VulkanCompute::VulkanCompute(VulkanComputeInfo* const info, Scene* scene, Pass* pass) {
		this->scene = scene;

		std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
		std::vector<VulkanShader*> shaderModules;
		std::vector<DescriptorLayout> reflDescriptors;

		VkComputePipelineCreateInfo& computeInfo = info->computeInfo;

		// load shaders into the pipeline state, and analyze shader for expected descriptors
		std::vector<uint32_t> fileData = lazyLoadSpirv(info->shaderFilename, info->macros);

		// now compile the actual shader
		VulkanShader* sModule = new VulkanShader(scene->device, fileData);
		VkPipelineShaderStageCreateInfo stage = sModule->getShaderStage(info->stage);

		// use reflection to enumerate the inputs of this shader
		{
			// convert from char vector to uint32_t vector, then reflect
			std::vector<uint32_t> spv(fileData.cbegin(), fileData.cend());
			spirv_cross::CompilerGLSL glsl(std::move(spv));
			spirv_cross::ShaderResources resources = glsl.get_shader_resources();

			for (auto& resource : resources.uniform_buffers) {
				unsigned set = glsl.get_decoration(resource.id, spv::DecorationDescriptorSet);
				if (set == 2) {
					unsigned binding = glsl.get_decoration(resource.id, spv::DecorationBinding);
					if (reflDescriptors.size() <= binding) { reflDescriptors.resize(binding + 1); }
					reflDescriptors[binding] = { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS };
				}
			}

			for (auto& resource : resources.sampled_images)
			{
				unsigned set = glsl.get_decoration(resource.id, spv::DecorationDescriptorSet);
				if (set == 2) {
					unsigned binding = glsl.get_decoration(resource.id, spv::DecorationBinding);
					const spirv_cross::SPIRType type = glsl.get_type(resource.base_type_id);

					if (reflDescriptors.size() <= binding) { reflDescriptors.resize(binding + 1); }
					reflDescriptors[binding] = { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_ALL_GRAPHICS };
				}
			}
		}
		computeInfo.stage = stage;

		// create descriptor layout (expand later)
		descriptorSetLayout = new VulkanDescriptorSetLayout(scene->device, reflDescriptors);

		// create pipeline layout to aggregate all descriptor sets
		std::vector<VkDescriptorSetLayout> dSetLayouts{
			scene->globalDescriptorSetLayout->handle,
			pass->inputLayout->handle,
			this->descriptorSetLayout->handle
		};

		VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
		pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(dSetLayouts.size());
		pipelineLayoutInfo.pSetLayouts = dSetLayouts.data();
		pipelineLayoutInfo.pushConstantRangeCount = static_cast<uint32_t>(info->pushConstRanges.size());
		pipelineLayoutInfo.pPushConstantRanges = info->pushConstRanges.data();

		VkResult result = vkCreatePipelineLayout(*scene->device, &pipelineLayoutInfo, nullptr, &pipelineLayout);
		if (result != VK_SUCCESS) {
			throw std::runtime_error("Failed to create pipeline layout for material!");
		}

		computeInfo.layout = pipelineLayout;
		result = vkCreateComputePipelines(*scene->device, VK_NULL_HANDLE, 1, &computeInfo, nullptr, &pipeline);
		if (result != VK_SUCCESS) {
			throw std::runtime_error("Failed to create compute pipeline!");
		}

		delete sModule;
	}

	VulkanCompute::~VulkanCompute() {
		vkDestroyDescriptorSetLayout(*scene->device, *descriptorSetLayout, nullptr);
		vkDestroyPipelineLayout(*scene->device, pipelineLayout, nullptr);
		vkDestroyPipeline(*scene->device, pipeline, nullptr);
	}

	VulkanComputeInstance::VulkanComputeInstance(VulkanCompute* compute) {
		this->compute = compute;

		descriptorSets.resize(compute->scene->device->swapchain->swapChainLength);
		for (uint32_t i = 0; i < descriptorSets.size(); i++) {
			descriptorSets[i] = new VulkanDescriptorSet(compute->descriptorSetLayout);
		}
	}

	VulkanComputeInstance::~VulkanComputeInstance() {
		for (VulkanDescriptorSet* dset : descriptorSets) {
			delete dset;
		}
	}
}