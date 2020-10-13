#pragma once

#include <vector>
#include <string>

#include <vulkan/vulkan.h>

#include "VulkanShader.h"
#include "VulkanDescriptorSet.h"

namespace vku {
	struct Scene;
	struct VulkanTexture;
	struct VulkanDevice;
	struct VulkanDescriptorSet;
	struct VulkanDescriptorSetLayout;
	struct Pass;

	struct ShaderInfo {
		std::string filename;
		VkShaderStageFlagBits stage;
		std::vector<ShaderMacro> macros;
	};

	struct VulkanMaterialInfo {
		VkGraphicsPipelineCreateInfo pipeline{};

		VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
		VkVertexInputBindingDescription vertexBindingDescription;
		std::vector<VkVertexInputAttributeDescription> vertexAttributeDescriptions;
		VkPipelineVertexInputStateCreateInfo vertexInput{};
		VkViewport viewport{};
		VkRect2D scissor{};
		VkPipelineViewportStateCreateInfo viewportState{};
		VkPipelineColorBlendAttachmentState colorBlendAttachment{};
		VkPipelineColorBlendStateCreateInfo colorBlending{};
		VkPipelineRasterizationStateCreateInfo rasterizer{};
		VkPipelineDepthStencilStateCreateInfo depthStencil{};
		VkPipelineMultisampleStateCreateInfo multisampling{};
		VkPipelineTessellationStateCreateInfo tesselation{};
		std::vector<VkDynamicState> dynamicStateList{
			VK_DYNAMIC_STATE_VIEWPORT,
			VK_DYNAMIC_STATE_SCISSOR
		};
		VkPipelineDynamicStateCreateInfo dynamicState{};

		std::vector<VkPushConstantRange> pushConstRanges{};

		std::vector<ShaderInfo> shaderStages;

		VulkanDevice* device;

		VulkanMaterialInfo(VulkanDevice* const device);
	};

	struct VulkanMaterial {
		Scene* scene;

		VkPipeline pipeline;
		VkPipelineLayout pipelineLayout;

		VulkanDescriptorSetLayout* descriptorSetLayout;

		VulkanMaterial(VulkanMaterialInfo* const matInfo, Scene* const scene, Pass* const pass);

		VulkanMaterial() {}

		void bind(VkCommandBuffer cb);

		~VulkanMaterial();
	};

	struct VulkanMaterialInstance {
		VulkanMaterial* material;

		std::vector<VulkanDescriptorSet*> descriptorSets;

		VulkanMaterialInstance(VulkanMaterial* mat);

		~VulkanMaterialInstance();

		void bind(VkCommandBuffer cb, uint32_t i);
	};
}