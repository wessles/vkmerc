#pragma once

#include <vulkan/vulkan.h>
#include "Vku.hpp"
#include "vku_image.hpp"
#include "material.h"

#include <glm/glm.hpp>

#define M_PI 3.1415926535

namespace vku::cubemapgen {

	struct CubemapFilterParams {
		Texture *cubemap;
		int32_t dim;
		VkFormat format;
		std::string filterVertShader;
		std::string filterFragShader;
		void** pushConstantData;
		uint32_t pushConstantSize;
	};

	Texture filterCubemap(CubemapFilterParams &cubemapFilterParams) {
		VkFormat format = cubemapFilterParams.format;
		uint32_t dim = cubemapFilterParams.dim;
		uint32_t numMips = static_cast<uint32_t>(floor(log2(dim))) + 1;

		// Pre-filtered cube map
		Texture texture;
		createImage(dim, dim, numMips, VK_SAMPLE_COUNT_1_BIT, format,
			VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			texture.image.handle,
			texture.image.memory,
			VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT, 6);
		texture.image.view = createImageView(texture.image.handle, format, VK_IMAGE_ASPECT_COLOR_BIT, numMips, VK_IMAGE_VIEW_TYPE_CUBE, 6);

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
		samplerCI.maxLod = static_cast<float>(numMips);
		samplerCI.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
		vkCreateSampler(vku::state::device, &samplerCI, nullptr, &texture.sampler);

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
		attDesc.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
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

		// Renderpass
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
			throw std::runtime_error("Failed to create render pass.");
		}

		struct {
			VkImage image;
			VkImageView view;
			VkDeviceMemory memory;
			VkFramebuffer framebuffer;
		} offscreen;

		// Offscreen framebuffer
		{
			// Color attachment
			createImage(dim, dim, 1, VK_SAMPLE_COUNT_1_BIT, format, VK_IMAGE_TILING_OPTIMAL,
				VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, offscreen.image, offscreen.memory);
			offscreen.view = createImageView(offscreen.image, format, VK_IMAGE_ASPECT_COLOR_BIT, 1);

			VkFramebufferCreateInfo fbufCreateInfo{};
			fbufCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			fbufCreateInfo.renderPass = renderpass;
			fbufCreateInfo.attachmentCount = 1;
			fbufCreateInfo.pAttachments = &offscreen.view;
			fbufCreateInfo.width = dim;
			fbufCreateInfo.height = dim;
			fbufCreateInfo.layers = 1;
			if (vkCreateFramebuffer(vku::state::device, &fbufCreateInfo, nullptr, &offscreen.framebuffer) != VK_SUCCESS) {
				throw std::runtime_error("Failed to create framebuffer.");
			}

			transitionImageLayout(offscreen.image, format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 1, 1);
		}

		// Descriptors
		VkDescriptorSetLayout descriptorsetlayout;
		vku::descriptor::createDescriptorSetLayout(&descriptorsetlayout, { {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT} });

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
		VkWriteDescriptorSet write = cubemapFilterParams.cubemap->getDescriptorWrite(0, descriptorset);
		vkUpdateDescriptorSets(vku::state::device, 1, &write, 0, nullptr);


		// Pipeline layout
		struct VertexPushBlock {
			glm::mat4 mvp;
		} pushBlock;

		VkPipelineLayout pipelinelayout;

		VkPushConstantRange pushConstantRangeVert{};
		pushConstantRangeVert.offset = 0;
		pushConstantRangeVert.size = sizeof(VertexPushBlock);
		pushConstantRangeVert.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

		VkPushConstantRange pushConstantRangeFrag{};
		pushConstantRangeFrag.offset = sizeof(VertexPushBlock);
		pushConstantRangeFrag.size = cubemapFilterParams.pushConstantSize;
		pushConstantRangeFrag.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

		std::array<VkPushConstantRange, 2> pushConstants = {pushConstantRangeVert, pushConstantRangeFrag};

		VkPipelineLayoutCreateInfo pipelineLayoutCI{};
		pipelineLayoutCI.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutCI.setLayoutCount = 1;
		pipelineLayoutCI.pSetLayouts = &descriptorsetlayout;
		pipelineLayoutCI.pushConstantRangeCount = pushConstants.size();
		pipelineLayoutCI.pPushConstantRanges = pushConstants.data();
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
		pipelineBuilder.scissor.extent = { dim, dim };

		std::vector<VkPipelineShaderStageCreateInfo> shaderStages{};
		shaderStages.resize(2);
		pipelineBuilder.pipelineInfo.stageCount = 2;
		pipelineBuilder.pipelineInfo.pStages = shaderStages.data();

		// Look-up-table (from BRDF) pipeline
		VkShaderModule vertStageModule = vku::create::createShaderModule(vku::shadercomp::lazyLoadSpirv(cubemapFilterParams.filterVertShader));
		shaderStages[0] = vku::create::createShaderStage(vertStageModule, VK_SHADER_STAGE_VERTEX_BIT);
		VkShaderModule fragStageModule = vku::create::createShaderModule(vku::shadercomp::lazyLoadSpirv(cubemapFilterParams.filterFragShader));
		shaderStages[1] = vku::create::createShaderStage(fragStageModule, VK_SHADER_STAGE_FRAGMENT_BIT);

		pipelineBuilder.pipelineInfo.layout = pipelinelayout;
		pipelineBuilder.pipelineInfo.renderPass = renderpass;

		VkPipeline pipeline;
		if (vkCreateGraphicsPipelines(vku::state::device, nullptr, 1, &pipelineBuilder.pipelineInfo, nullptr, &pipeline) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create graphics pipeline.");
		}

		vkDestroyShaderModule(vku::state::device, vertStageModule, nullptr);
		vkDestroyShaderModule(vku::state::device, fragStageModule, nullptr);

		// Render

		VkClearValue clearValues[1];
		clearValues[0].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };

		VkRenderPassBeginInfo renderPassBeginInfo{};
		// Reuse render pass from example pass
		renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassBeginInfo.renderPass = renderpass;
		renderPassBeginInfo.framebuffer = offscreen.framebuffer;
		renderPassBeginInfo.renderArea.extent.width = dim;
		renderPassBeginInfo.renderArea.extent.height = dim;
		renderPassBeginInfo.clearValueCount = 1;
		renderPassBeginInfo.pClearValues = clearValues;

		std::vector<glm::mat4> matrices = {
			// POSITIVE_X
			glm::rotate(glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f)), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
			// NEGATIVE_X
			glm::rotate(glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(0.0f, 1.0f, 0.0f)), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
			// POSITIVE_Y
			glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
			// NEGATIVE_Y
			glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
			// POSITIVE_Z
			glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
			// NEGATIVE_Z
			glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
		};
		MeshBuffer skyboxMesh;
		skyboxMesh.load(box);

		VkCommandBuffer cmdBuf = vku::cmd::beginCommandBuffer(true);
		{
			VkViewport viewport{};
			viewport.width = viewport.height = dim;
			viewport.minDepth = 0.0f;
			viewport.maxDepth = 1.0f;
			VkRect2D scissor{};
			scissor.extent = { dim,dim };

			vkCmdSetViewport(cmdBuf, 0, 1, &viewport);
			vkCmdSetScissor(cmdBuf, 0, 1, &scissor);

			// Change image layout for all cubemap faces to transfer destination
			vku::image::transitionImageLayout(cmdBuf, texture.image.handle, format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, numMips, 6);

			for (uint32_t m = 0; m < numMips; m++) {
				
				for (uint32_t f = 0; f < 6; f++) {
					viewport.width = static_cast<float>(dim * std::pow(0.5f, m));
					viewport.height = static_cast<float>(dim * std::pow(0.5f, m));
					vkCmdSetViewport(cmdBuf, 0, 1, &viewport);

					// Render scene from cube face's point of view
					vkCmdBeginRenderPass(cmdBuf, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

					// Update shader push constant block
					pushBlock.mvp = glm::perspective((float)(M_PI / 2.0), 1.0f, 0.1f, 512.0f) * matrices[f];

					vkCmdPushConstants(cmdBuf, pipelinelayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(VertexPushBlock), &pushBlock);
					vkCmdPushConstants(cmdBuf, pipelinelayout, VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(VertexPushBlock), cubemapFilterParams.pushConstantSize, cubemapFilterParams.pushConstantData[m]);

					vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
					vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelinelayout, 0, 1, &descriptorset, 0, NULL);

					VkDeviceSize offset[] = { 0 };
					vkCmdBindVertexBuffers(cmdBuf, 0, 1, &skyboxMesh.vBuffer, offset);
					vkCmdBindIndexBuffer(cmdBuf, skyboxMesh.iBuffer, 0, VK_INDEX_TYPE_UINT32);
					vkCmdDrawIndexed(cmdBuf, skyboxMesh.indicesSize, 1, 0, 0, 0);

					vkCmdEndRenderPass(cmdBuf);

					vku::image::transitionImageLayout(cmdBuf, offscreen.image, format,
						VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
						VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, 1, 1);

					int32_t width = static_cast<int32_t>(viewport.width);
					int32_t height = static_cast<int32_t>(viewport.height);

					VkImageBlit blit{};

					blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
					blit.srcSubresource.baseArrayLayer = 0;
					blit.srcSubresource.mipLevel = 0;
					blit.srcSubresource.layerCount = 1;

					blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
					blit.dstSubresource.baseArrayLayer = f;
					blit.dstSubresource.mipLevel = m;
					blit.dstSubresource.layerCount = 1;

					blit.srcOffsets[0] = { 0, 0, 0 };
					blit.srcOffsets[1] = { static_cast<int32_t>(dim), static_cast<int32_t>(dim), 1 };

					blit.dstOffsets[0] = { 0, 0, 0 };
					blit.dstOffsets[1] = { width, height, 1 };

					vkCmdBlitImage(cmdBuf, offscreen.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, texture.image.handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_NEAREST);

					vku::image::transitionImageLayout(
						cmdBuf, offscreen.image, format,
						VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
						VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 1, 1);
				}
			}

			vku::image::transitionImageLayout(cmdBuf, texture.image.handle, format, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, numMips, 6);
		}
		vku::cmd::submitCommandBuffer(cmdBuf, vku::state::graphicsQueue);

		skyboxMesh.destroy();
		vkDestroyRenderPass(vku::state::device, renderpass, nullptr);
		vkDestroyFramebuffer(vku::state::device, offscreen.framebuffer, nullptr);
		vkFreeMemory(vku::state::device, offscreen.memory, nullptr);
		vkDestroyImageView(vku::state::device, offscreen.view, nullptr);
		vkDestroyImage(vku::state::device, offscreen.image, nullptr);
		vkDestroyDescriptorPool(vku::state::device, descriptorpool, nullptr);
		vkDestroyDescriptorSetLayout(vku::state::device, descriptorsetlayout, nullptr);
		vkDestroyPipeline(vku::state::device, pipeline, nullptr);
		vkDestroyPipelineLayout(vku::state::device, pipelinelayout, nullptr);

		return texture;
	}

	Texture generateIrradianceCube(Texture &cubemap) {
		CubemapFilterParams filter{};
		filter.cubemap = &cubemap;
		filter.dim = 64;
		filter.format = VK_FORMAT_R32G32B32A32_SFLOAT;
		filter.filterVertShader = "res/shaders/cubemapgen/filtercube.vert";
		filter.filterFragShader = "res/shaders/cubemapgen/irradiancecube.frag";

		uint32_t numMips = static_cast<uint32_t>(floor(log2(filter.dim))) + 1;

		struct PushBlock {
			float deltaPhi = (2.0f * float(M_PI)) / 180.0f;
			float deltaTheta = (0.5f * float(M_PI)) / 64.0f;
		};

		std::vector<PushBlock> pushBlocks{};
		pushBlocks.resize(numMips);
		std::vector<void*> pushBlockPtrs{};
		pushBlockPtrs.resize(numMips);
		for (int i = 0; i < numMips; i++) {
			pushBlockPtrs[i] = &pushBlocks[i];
		}

		filter.pushConstantData = pushBlockPtrs.data();
		filter.pushConstantSize = sizeof(PushBlock);

		return filterCubemap(filter);
	}

	// Adapted from Sascha Willems' pbrtexture.cpp
	// Prefilter environment cubemap
	// See https://placeholderart.wordpress.com/2015/07/28/implementation-notes-runtime-environment-map-filtering-for-image-based-lighting/
	Texture generatePrefilteredCube(Texture &cubemap)
	{
		CubemapFilterParams filter{};
		filter.cubemap = &cubemap;
		filter.dim = 512;
		filter.format = VK_FORMAT_R32G32B32A32_SFLOAT;
		filter.filterVertShader = "res/shaders/cubemapgen/filtercube.vert";
		filter.filterFragShader = "res/shaders/cubemapgen/prefilterenvmap.frag";

		uint32_t numMips = static_cast<uint32_t>(floor(log2(filter.dim))) + 1;

		struct PushBlock {
			float roughness;
			uint32_t numSamples;
		};

		std::vector<PushBlock> pushBlocks{};
		pushBlocks.resize(numMips);
		std::vector<void*> pushBlockPtrs{};
		pushBlockPtrs.resize(numMips);
		for (int i = 0; i < numMips; i++) {
			pushBlocks[i].roughness = (float)i / (float)(numMips - 1);
			pushBlocks[i].numSamples = 128u;
			pushBlockPtrs[i] = &pushBlocks[i];
		}

		filter.pushConstantData = pushBlockPtrs.data();
		filter.pushConstantSize = sizeof(PushBlock);

		return filterCubemap(filter);
	}

}