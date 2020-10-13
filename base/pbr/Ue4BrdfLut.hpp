#pragma once

#include <exception>

#include <vulkan/vulkan.h>

#include "../VulkanDevice.h"
#include "../VulkanShader.h"
#include "../VulkanMaterial.h"
#include "../VulkanTexture.h"
#include "../VulkanDescriptorSet.h"

namespace vku {
	// Adapted from Sascha Willems' pbrtexture.cpp
	// Generate a BRDF integration map used as a look-up-table (stores roughness / NdotV)
	VulkanTexture* generateBRDFLUT(VulkanDevice *device, uint32_t dim = 512)
	{
		const VkFormat format = VK_FORMAT_R16G16_SFLOAT;	// R16G16 is supported pretty much everywhere

		// Image
		VulkanTexture* texture = new VulkanTexture;

		VulkanImageInfo imageInfo{};
		imageInfo.width = dim;
		imageInfo.height = dim;
		imageInfo.mipLevels = 1;
		imageInfo.numSamples = VK_SAMPLE_COUNT_1_BIT;
		imageInfo.format = format;
		imageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		texture->image = new VulkanImage(device, imageInfo);
		VulkanImageViewInfo viewInfo{};
		texture->image->writeImageViewInfo(&viewInfo);
		texture->view = new VulkanImageView(device, viewInfo);
		VulkanSamplerInfo samplerInfo{};
		samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		texture->sampler = new VulkanSampler(device, samplerInfo);

		// FB, Att, RP, Pipe, etc.
		VkAttachmentDescription attDesc = {};
		// Color attachment
		attDesc.format = format;
		attDesc.samples = VK_SAMPLE_COUNT_1_BIT;
		attDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attDesc.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		VkAttachmentReference colorReference = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

		VkSubpassDescription subpassDescription = {};
		subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpassDescription.colorAttachmentCount = 1;
		subpassDescription.pColorAttachments = &colorReference;

		// Use subpass dependencies for layout transitions
		std::array<VkSubpassDependency, 2> dependencies;
		dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
		dependencies[0].dstSubpass = 0;
		dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
		dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
		dependencies[1].srcSubpass = 0;
		dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
		dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
		dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

		// Create the actual renderpass
		VkRenderPassCreateInfo renderPassCI{};
		renderPassCI.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassCI.attachmentCount = 1;
		renderPassCI.pAttachments = &attDesc;
		renderPassCI.subpassCount = 1;
		renderPassCI.pSubpasses = &subpassDescription;
		renderPassCI.dependencyCount = 2;
		renderPassCI.pDependencies = dependencies.data();

		VkRenderPass renderpass;
		if (vkCreateRenderPass(*device, &renderPassCI, nullptr, &renderpass) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create render pass for BRDF integration map.");
		}

		VkFramebufferCreateInfo framebufferCI{};
		framebufferCI.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferCI.renderPass = renderpass;
		framebufferCI.attachmentCount = 1;
		framebufferCI.pAttachments = &texture->view->handle;
		framebufferCI.width = dim;
		framebufferCI.height = dim;
		framebufferCI.layers = 1;

		VkFramebuffer framebuffer;
		if (vkCreateFramebuffer(*device, &framebufferCI, nullptr, &framebuffer) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create framebuffer for BRDF integration map.");
		}

		// Descriptor set layout
		VulkanDescriptorSetLayout* descriptorsetlayout = new VulkanDescriptorSetLayout(device, {});

		// Descriptor Pool
		std::vector<VkDescriptorPoolSize> poolSizes{ {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1} };
		VkDescriptorPoolCreateInfo descriptorPoolCI{};
		descriptorPoolCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		descriptorPoolCI.poolSizeCount = 1;
		descriptorPoolCI.pPoolSizes = &poolSizes[0];
		descriptorPoolCI.maxSets = 1;
		VkDescriptorPool descriptorpool;
		if (vkCreateDescriptorPool(*device, &descriptorPoolCI, nullptr, &descriptorpool) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create temporary descriptor pool.");
		}

		// Descriptor sets
		VkDescriptorSet descriptorset;
		VkDescriptorSetAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = descriptorpool;
		allocInfo.descriptorSetCount = 1;
		allocInfo.pSetLayouts = &descriptorsetlayout->handle;
		if (vkAllocateDescriptorSets(*device, &allocInfo, &descriptorset) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create descriptor sets.");
		}

		// Pipeline layout
		VkPipelineLayout pipelinelayout;
		VkPipelineLayoutCreateInfo pipelineLayoutCI{};
		pipelineLayoutCI.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutCI.setLayoutCount = 1;
		pipelineLayoutCI.pSetLayouts = &descriptorsetlayout->handle;
		if (vkCreatePipelineLayout(*device, &pipelineLayoutCI, nullptr, &pipelinelayout) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create pipeline layout.");
		}

		// Pipeline
		VulkanMaterialInfo pipelineBuilder(device);
		pipelineBuilder.rasterizer.cullMode = VK_CULL_MODE_NONE;
		pipelineBuilder.colorBlendAttachment.blendEnable = VK_FALSE;
		pipelineBuilder.colorBlendAttachment.blendEnable = VK_FALSE;
		pipelineBuilder.depthStencil.depthTestEnable = VK_FALSE;
		pipelineBuilder.depthStencil.depthBoundsTestEnable = VK_FALSE;
		pipelineBuilder.depthStencil.stencilTestEnable = VK_FALSE;
		pipelineBuilder.viewport.width = dim;
		pipelineBuilder.viewport.height = dim;
		pipelineBuilder.scissor.extent = { dim,dim };

		// No vertex input
		VkPipelineVertexInputStateCreateInfo& vInputState = pipelineBuilder.vertexInput;
		vInputState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vInputState.vertexAttributeDescriptionCount = 0;
		vInputState.vertexBindingDescriptionCount = 0;

		std::vector<VkPipelineShaderStageCreateInfo> shaderStages{};
		shaderStages.resize(2);
		pipelineBuilder.pipeline.stageCount = 2;
		pipelineBuilder.pipeline.pStages = shaderStages.data();

		// Look-up-table (from BRDF) pipeline
		VulkanShader* vertStageModule = new VulkanShader(device, lazyLoadSpirv("res/shaders/pbr_gen/genbrdflut.vert"));
		shaderStages[0] = vertStageModule->getShaderStage(VK_SHADER_STAGE_VERTEX_BIT);
		VulkanShader* fragStageModule = new VulkanShader(device, lazyLoadSpirv("res/shaders/pbr_gen/genbrdflut.frag"));
		shaderStages[1] = fragStageModule->getShaderStage(VK_SHADER_STAGE_FRAGMENT_BIT);

		pipelineBuilder.pipeline.layout = pipelinelayout;
		pipelineBuilder.pipeline.renderPass = renderpass;

		VkPipeline pipeline;
		if (vkCreateGraphicsPipelines(*device, nullptr, 1, &pipelineBuilder.pipeline, nullptr, &pipeline) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create graphics pipeline!");
		}

		delete vertStageModule;
		delete fragStageModule;

		// Render
		VkClearValue clearValues[1];
		clearValues[0].color = { { 0.0f, 0.0f, 0.0f, 1.0f } };

		VkRenderPassBeginInfo renderPassBeginInfo{};
		renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassBeginInfo.renderPass = renderpass;
		renderPassBeginInfo.renderArea.extent.width = dim;
		renderPassBeginInfo.renderArea.extent.height = dim;
		renderPassBeginInfo.clearValueCount = 1;
		renderPassBeginInfo.pClearValues = clearValues;
		renderPassBeginInfo.framebuffer = framebuffer;

		VkCommandBuffer cmdBuf = device->beginCommandBuffer(true);
		vkCmdBeginRenderPass(cmdBuf, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
		{
			VkViewport viewport{};
			viewport.x = 0;
			viewport.y = 0;
			viewport.width = dim;
			viewport.height = dim;
			VkRect2D scissor{};
			scissor.extent = { dim,dim };
			scissor.offset = { 0, 0 };
			vkCmdSetViewport(cmdBuf, 0, 1, &viewport);
			vkCmdSetScissor(cmdBuf, 0, 1, &scissor);
			vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
			vkCmdDraw(cmdBuf, 3, 1, 0, 0);
		}
		vkCmdEndRenderPass(cmdBuf);
		device->submitCommandBuffer(cmdBuf, device->graphicsQueue);

		vkDestroyPipeline(*device, pipeline, nullptr);
		vkDestroyPipelineLayout(*device, pipelinelayout, nullptr);
		vkDestroyRenderPass(*device, renderpass, nullptr);
		vkDestroyFramebuffer(*device, framebuffer, nullptr);
		delete descriptorsetlayout;
		vkDestroyDescriptorPool(*device, descriptorpool, nullptr);

		return texture;
	}
};