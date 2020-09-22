#pragma once

#include <exception>

#include <vulkan/vulkan.h>

#include "Vku.hpp"
#include "shadercomp.h"
#include "material.h"
#include "vku_image.hpp"

using namespace vku::image;

struct UE4BrdfTable {
	Texture texture;

	// Adapted from Sascha Willems' pbrtexture.cpp
	// Generate a BRDF integration map used as a look-up-table (stores roughness / NdotV)
	void generateBRDFLUT()
	{
		const VkFormat format = VK_FORMAT_R16G16_SFLOAT;	// R16G16 is supported pretty much everywhere
		const int32_t dim = 512;

		// Image
		createImage(dim, dim, 1, VK_SAMPLE_COUNT_1_BIT, format, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, texture.image.handle, texture.image.memory);
		texture.image.view = createImageView(texture.image.handle, format, VK_IMAGE_ASPECT_COLOR_BIT, 1);
		
		// Sampler
		VkSamplerCreateInfo samplerCI{};
		samplerCI.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerCI.magFilter = VK_FILTER_LINEAR;
		samplerCI.minFilter = VK_FILTER_LINEAR;
		samplerCI.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerCI.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerCI.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerCI.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerCI.minLod = 0.0f;
		samplerCI.maxLod = 1.0f;
		samplerCI.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
		if (vkCreateSampler(vku::state::device, &samplerCI, nullptr, &texture.sampler) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create sampler for BRDF integration map.");
		}

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
		if (vkCreateRenderPass(vku::state::device, &renderPassCI, nullptr, &renderpass) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create render pass for BRDF integration map.");
		}

		VkFramebufferCreateInfo framebufferCI{};
		framebufferCI.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferCI.renderPass = renderpass;
		framebufferCI.attachmentCount = 1;
		framebufferCI.pAttachments = &texture.image.view;
		framebufferCI.width = dim;
		framebufferCI.height = dim;
		framebufferCI.layers = 1;

		VkFramebuffer framebuffer;
		if (vkCreateFramebuffer(vku::state::device, &framebufferCI, nullptr, &framebuffer) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create framebuffer for BRDF integration map.");
		}

		// Descriptor set layout
		VkDescriptorSetLayout descriptorsetlayout;
		vku::descriptor::createDescriptorSetLayout(&descriptorsetlayout, {});

		// Descriptor Pool
		std::vector<VkDescriptorPoolSize> poolSizes{ {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1} };
		VkDescriptorPoolCreateInfo descriptorPoolCI{};
		descriptorPoolCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		descriptorPoolCI.poolSizeCount = 1;
		descriptorPoolCI.pPoolSizes = &poolSizes[0];
		descriptorPoolCI.maxSets = 1;
		VkDescriptorPool descriptorpool;
		if (vkCreateDescriptorPool(vku::state::device, &descriptorPoolCI, nullptr, &descriptorpool) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create temporary descriptor pool.");
		}

		// Descriptor sets
		VkDescriptorSet descriptorset;
		VkDescriptorSetAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = descriptorpool;
		allocInfo.descriptorSetCount = 1;
		allocInfo.pSetLayouts = &descriptorsetlayout;
		if (vkAllocateDescriptorSets(vku::state::device, &allocInfo, &descriptorset) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create descriptor sets.");
		}

		// Pipeline layout
		VkPipelineLayout pipelinelayout;
		VkPipelineLayoutCreateInfo pipelineLayoutCI{};
		pipelineLayoutCI.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutCI.setLayoutCount = 1;
		pipelineLayoutCI.pSetLayouts = &descriptorsetlayout;
		if (vkCreatePipelineLayout(vku::state::device, &pipelineLayoutCI, nullptr, &pipelinelayout) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create pipeline layout.");
		}

		// Pipeline
		vku::material::PipelineBuilder pipelineBuilder{};
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
		VkPipelineVertexInputStateCreateInfo &vInputState = pipelineBuilder.vertexInputInfo;
		vInputState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vInputState.vertexAttributeDescriptionCount = 0;
		vInputState.vertexBindingDescriptionCount = 0;

		std::vector<VkPipelineShaderStageCreateInfo> shaderStages{};
		shaderStages.resize(2);
		pipelineBuilder.pipelineInfo.stageCount = 2;
		pipelineBuilder.pipelineInfo.pStages = shaderStages.data();

		// Look-up-table (from BRDF) pipeline
		VkShaderModule vertStageModule = vku::create::createShaderModule(vku::shadercomp::lazyLoadSpirv("res/shaders/cubemapgen/genbrdflut.vert"));
		shaderStages[0] = vku::create::createShaderStage(vertStageModule, VK_SHADER_STAGE_VERTEX_BIT);
		VkShaderModule fragStageModule = vku::create::createShaderModule(vku::shadercomp::lazyLoadSpirv("res/shaders/cubemapgen/genbrdflut.frag"));
		shaderStages[1] = vku::create::createShaderStage(fragStageModule, VK_SHADER_STAGE_FRAGMENT_BIT);

		pipelineBuilder.pipelineInfo.layout = pipelinelayout;
		pipelineBuilder.pipelineInfo.renderPass = renderpass;

		VkPipeline pipeline;
		if (vkCreateGraphicsPipelines(vku::state::device, nullptr, 1, &pipelineBuilder.pipelineInfo, nullptr, &pipeline) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create graphics pipeline!");
		}

		vkDestroyShaderModule(vku::state::device, vertStageModule, nullptr);
		vkDestroyShaderModule(vku::state::device, fragStageModule, nullptr);

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

		VkCommandBuffer cmdBuf = vku::cmd::beginCommandBuffer(true);
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
		vku::cmd::submitCommandBuffer(cmdBuf, vku::state::graphicsQueue);

		vkDestroyPipeline(vku::state::device, pipeline, nullptr);
		vkDestroyPipelineLayout(vku::state::device, pipelinelayout, nullptr);
		vkDestroyRenderPass(vku::state::device, renderpass, nullptr);
		vkDestroyFramebuffer(vku::state::device, framebuffer, nullptr);
		vkDestroyDescriptorSetLayout(vku::state::device, descriptorsetlayout, nullptr);
		vkDestroyDescriptorPool(vku::state::device, descriptorpool, nullptr);
	}
};