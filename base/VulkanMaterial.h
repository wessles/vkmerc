#pragma once

#include <vector>
#include <string>

#include <vulkan/vulkan.h>

#include "VulkanDescriptorSet.h"

namespace vku {
	struct Scene;
	struct VulkanTexture;
	struct VulkanDevice;
	struct VulkanShader;
	struct VulkanDescriptorSet;
	struct VulkanDescriptorSetLayout;
	struct Pass;

	struct VulkanMaterialInfo {
		VkGraphicsPipelineCreateInfo pipelineInfo{};

		VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
		VkVertexInputBindingDescription vertexBindingDescription;
		std::vector<VkVertexInputAttributeDescription> vertexAttributeDescriptions;
		VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
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

		VulkanDevice* device;

		VulkanMaterialInfo(VulkanDevice* const device);
	};

	struct ShaderInfo {
		std::string filename;
		VkShaderStageFlagBits stage;
	};

	struct VulkanMaterial {
		Scene* scene;

		VkPipeline pipeline;
		VkPipelineLayout pipelineLayout;

		VulkanDescriptorSetLayout *descriptorSetLayout;

		VulkanMaterial(VulkanMaterialInfo* const matInfo, Scene* const scene, Pass* const pass, const std::vector<ShaderInfo> shaderStageInfo);

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