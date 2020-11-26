#pragma once

#include <vector>
#include <string>

#include <vulkan/vulkan.h>

#include "shader/ShaderCache.h"
#include "VulkanDescriptorSet.h"

namespace vku {
	struct Scene;
	struct VulkanTexture;
	struct VulkanDevice;
	struct VulkanDescriptorSet;
	struct VulkanDescriptorSetLayout;
	struct Pass;

	struct VulkanMaterialInfo {
		VkGraphicsPipelineCreateInfo pipeline{};

		VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
		VkVertexInputBindingDescription vertexBindingDescription;
		std::vector<VkVertexInputAttributeDescription> vertexAttributeDescriptions;
		VkPipelineVertexInputStateCreateInfo vertexInput{};
		VkViewport viewport{};
		VkRect2D scissor{};
		VkPipelineViewportStateCreateInfo viewportState{};
		std::vector<VkPipelineColorBlendAttachmentState> colorBlendAttachments{};
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
		std::vector<ShaderVariant> shaderStages;

		VulkanMaterialInfo();

		// link pointers inside structs to reference each other
		// useful right after a copy constructor
		void linkPointers();
	};

	struct VulkanMaterial {
		VulkanMaterialInfo* info;

		Scene* scene;
		Pass* pass;

		VkPipeline pipeline;
		VkPipelineLayout pipelineLayout;

		VulkanDescriptorSetLayout* descriptorSetLayout;

		VulkanMaterial(VulkanMaterialInfo* const matInfo, Scene* const scene, Pass* const pass);

		void rebuild();
		void bind(VkCommandBuffer cb);

		~VulkanMaterial();

	private:
		void init();
		void destroy();
	};

	struct VulkanMaterialInstance {
		VulkanMaterial* material;

		std::vector<VulkanDescriptorSet*> descriptorSets;

		VulkanMaterialInstance(VulkanMaterial* mat);

		~VulkanMaterialInstance();

		void bind(VkCommandBuffer cb, uint32_t i);
	};
}