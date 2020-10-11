#pragma once

#include <vulkan/vulkan.h>

#include "../VulkanDevice.h"
#include "../VulkanTexture.h"
#include "../VulkanMaterial.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>

#define M_PI 3.1415926535

namespace vku {

	struct CubemapFilterParams {
		VulkanTexture* cubemap;
		int32_t dim;
		VkFormat format;
		std::string filterVertShader;
		std::string filterFragShader;
		void** pushConstantData;
		uint32_t pushConstantSize;
	};

	VulkanTexture* filterCubemap(VulkanDevice* device, CubemapFilterParams& cubemapFilterParams) {
		VkFormat format = cubemapFilterParams.format;
		uint32_t dim = cubemapFilterParams.dim;
		uint32_t numMips = static_cast<uint32_t>(floor(log2(dim))) + 1;

		// Pre-filtered cube map
		VulkanTexture* texture = new VulkanTexture;
		VulkanImageInfo imageInfo{};
		imageInfo.width = dim;
		imageInfo.height = dim;
		imageInfo.mipLevels = numMips;
		imageInfo.arrayLayers = 6;
		imageInfo.numSamples = VK_SAMPLE_COUNT_1_BIT;
		imageInfo.format = format;
		imageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		imageInfo.imageCreateFlags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
		texture->image = new VulkanImage(device, imageInfo);
		VulkanImageViewInfo viewInfo{};
		texture->image->writeImageViewInfo(&viewInfo);
		viewInfo.imageViewType = VK_IMAGE_VIEW_TYPE_CUBE;
		texture->view = new VulkanImageView(device, viewInfo);
		VulkanSamplerInfo samplerInfo{};
		samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerInfo.minLod = 0.0f;
		samplerInfo.maxLod = static_cast<float>(numMips);
		samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
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
		if (vkCreateRenderPass(*device, &renderPassCI, nullptr, &renderpass) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create render pass.");
		}

		struct {
			VulkanImage* image;
			VulkanImageView* view;
			VkFramebuffer framebuffer;
		} offscreen;

		// Offscreen framebuffer
		{
			// Color attachment
			VulkanImageInfo info{};
			info.width = dim;
			info.height = dim;
			info.arrayLayers = 1;
			info.numSamples = VK_SAMPLE_COUNT_1_BIT;
			info.format = format;
			info.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
			offscreen.image = new VulkanImage(device, info);
			VulkanImageViewInfo viewInfo{};
			offscreen.image->writeImageViewInfo(&viewInfo);
			offscreen.view = new VulkanImageView(device, viewInfo);

			VkFramebufferCreateInfo fbufCreateInfo{};
			fbufCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			fbufCreateInfo.renderPass = renderpass;
			fbufCreateInfo.attachmentCount = 1;
			fbufCreateInfo.pAttachments = &offscreen.view->handle;
			fbufCreateInfo.width = dim;
			fbufCreateInfo.height = dim;
			fbufCreateInfo.layers = 1;
			if (vkCreateFramebuffer(*device, &fbufCreateInfo, nullptr, &offscreen.framebuffer) != VK_SUCCESS) {
				throw std::runtime_error("Failed to create framebuffer.");
			}

			offscreen.image->transitionImageLayout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
		}

		// Descriptors
		VulkanDescriptorSetLayout* descriptorsetlayout = new VulkanDescriptorSetLayout(device, { {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT} });
		VulkanDescriptorSet *descriptorset = new VulkanDescriptorSet(descriptorsetlayout);
		descriptorset->write(0, cubemapFilterParams.cubemap);


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

		std::array<VkPushConstantRange, 2> pushConstants = { pushConstantRangeVert, pushConstantRangeFrag };

		VkPipelineLayoutCreateInfo pipelineLayoutCI{};
		pipelineLayoutCI.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutCI.setLayoutCount = 1;
		pipelineLayoutCI.pSetLayouts = &descriptorsetlayout->handle;
		pipelineLayoutCI.pushConstantRangeCount = pushConstants.size();
		pipelineLayoutCI.pPushConstantRanges = pushConstants.data();
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
		pipelineBuilder.scissor.extent = { dim, dim };
		pipelineBuilder.dynamicState.dynamicStateCount = 0;

		std::vector<VkPipelineShaderStageCreateInfo> shaderStages{};
		shaderStages.resize(2);
		pipelineBuilder.pipelineInfo.stageCount = 2;
		pipelineBuilder.pipelineInfo.pStages = shaderStages.data();

		// Look-up-table (from BRDF) pipeline
		VulkanShader* vertStageModule = new VulkanShader(device, lazyLoadSpirv(cubemapFilterParams.filterVertShader));
		shaderStages[0] = vertStageModule->getShaderStage(VK_SHADER_STAGE_VERTEX_BIT);
		VulkanShader* fragStageModule = new VulkanShader(device, lazyLoadSpirv(cubemapFilterParams.filterFragShader));
		shaderStages[1] = fragStageModule->getShaderStage(VK_SHADER_STAGE_FRAGMENT_BIT);

		pipelineBuilder.pipelineInfo.layout = pipelinelayout;
		pipelineBuilder.pipelineInfo.renderPass = renderpass;

		VkPipeline pipeline;
		if (vkCreateGraphicsPipelines(*device, nullptr, 1, &pipelineBuilder.pipelineInfo, nullptr, &pipeline) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create graphics pipeline.");
		}

		delete vertStageModule;
		delete fragStageModule;

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
		VulkanMeshBuffer* skyboxMesh = new VulkanMeshBuffer(device, box);

		VkCommandBuffer cmdBuf = device->beginCommandBuffer(true);
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
			texture->image->transitionImageLayout(cmdBuf, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

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
					vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelinelayout, 0, 1, &descriptorset->handle, 0, NULL);

					skyboxMesh->draw(cmdBuf);

					vkCmdEndRenderPass(cmdBuf);

					offscreen.image->transitionImageLayout(cmdBuf, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

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

					vkCmdBlitImage(cmdBuf, offscreen.image->handle, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, texture->image->handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_NEAREST);

					//offscreen.image->transitionImageLayout(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
				}
			}

			texture->image->transitionImageLayout(cmdBuf, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		}
		device->submitCommandBuffer(cmdBuf, device->graphicsQueue);

		delete skyboxMesh;

		vkDestroyRenderPass(*device, renderpass, nullptr);
		vkDestroyFramebuffer(*device, offscreen.framebuffer, nullptr);
		delete offscreen.view;
		delete offscreen.image;
		delete descriptorset;
		delete descriptorsetlayout;
		vkDestroyPipeline(*device, pipeline, nullptr);
		vkDestroyPipelineLayout(*device, pipelinelayout, nullptr);

		return texture;
	}

	VulkanTexture* generateIrradianceCube(VulkanDevice *device, VulkanTexture *cubemap) {
		CubemapFilterParams filter{};
		filter.cubemap = cubemap;
		filter.dim = 64;
		filter.format = VK_FORMAT_R32G32B32A32_SFLOAT;
		filter.filterVertShader = "res/shaders/pbr_gen/filtercube.vert";
		filter.filterFragShader = "res/shaders/pbr_gen/irradiancecube.frag";

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

		return filterCubemap(device, filter);
	}

	// Adapted from Sascha Willems' pbrtexture.cpp
	// Prefilter environment cubemap
	// See https://placeholderart.wordpress.com/2015/07/28/implementation-notes-runtime-environment-map-filtering-for-image-based-lighting/
	VulkanTexture* generatePrefilteredCube(VulkanDevice* device, VulkanTexture* cubemap)
	{
		CubemapFilterParams filter{};
		filter.cubemap = cubemap;
		filter.dim = 512;
		filter.format = VK_FORMAT_R32G32B32A32_SFLOAT;
		filter.filterVertShader = "res/shaders/pbr_gen/filtercube.vert";
		filter.filterFragShader = "res/shaders/pbr_gen/prefilterenvmap.frag";

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

		return filterCubemap(device, filter);
	}
}