#pragma once

#include <vulkan/vulkan.h>
#include <spirv_cross/spirv_glsl.hpp>

#include "Vku.hpp"
#include "shadercomp.h"

using namespace vku::mesh;
using namespace vku::image;

namespace vku::material {
	struct PipelineBuilder {
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

		PipelineBuilder() {
			inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
			inputAssembly.primitiveRestartEnable = VK_FALSE;
			inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

			vertexBindingDescription = Vertex::getBindingDescription();
			vertexAttributeDescriptions = Vertex::getAttributeDescriptions();

			vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
			vertexInputInfo.vertexBindingDescriptionCount = 1;
			vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexAttributeDescriptions.size());
			vertexInputInfo.pVertexBindingDescriptions = &vertexBindingDescription; // Optional
			vertexInputInfo.pVertexAttributeDescriptions = vertexAttributeDescriptions.data(); // Optional

			viewport.x = 0.0f;
			viewport.y = 0.0f;
			viewport.width = (float)vku::state::swapChainExtent.width;
			viewport.height = (float)vku::state::swapChainExtent.height;
			viewport.minDepth = 0.0f;
			viewport.maxDepth = 1.0f;
			scissor.offset = { 0, 0 };
			scissor.extent = vku::state::swapChainExtent;
			viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
			viewportState.viewportCount = 1;
			viewportState.pViewports = &viewport;
			viewportState.scissorCount = 1;
			viewportState.pScissors = &scissor;

			colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
			colorBlendAttachment.blendEnable = VK_TRUE;
			colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
			colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
			colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
			colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
			colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
			colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

			colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
			colorBlending.logicOpEnable = VK_FALSE;
			colorBlending.logicOp = VK_LOGIC_OP_COPY; // Optional
			colorBlending.attachmentCount = 1;
			colorBlending.pAttachments = &colorBlendAttachment;
			colorBlending.blendConstants[0] = 0.0f; // Optional
			colorBlending.blendConstants[1] = 0.0f; // Optional
			colorBlending.blendConstants[2] = 0.0f; // Optional
			colorBlending.blendConstants[3] = 0.0f; // Optional

			rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
			rasterizer.depthClampEnable = VK_FALSE;
			rasterizer.rasterizerDiscardEnable = VK_FALSE;
			rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
			rasterizer.lineWidth = 1.0f;
			rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
			rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
			rasterizer.depthBiasEnable = VK_FALSE;
			rasterizer.depthBiasConstantFactor = 0.0f; // Optional
			rasterizer.depthBiasClamp = 0.0f; // Optional
			rasterizer.depthBiasSlopeFactor = 0.0f; // Optional

			depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
			depthStencil.depthTestEnable = VK_TRUE;
			depthStencil.depthWriteEnable = VK_TRUE;
			depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
			depthStencil.depthBoundsTestEnable = VK_FALSE;
			depthStencil.minDepthBounds = 0.0f; // Optional
			depthStencil.maxDepthBounds = 1.0f; // Optional
			depthStencil.stencilTestEnable = VK_FALSE;
			depthStencil.front = {}; // Optional
			depthStencil.back = {}; // Optional

			multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
			multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
			multisampling.pSampleMask = nullptr; // Optional
			multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
			multisampling.alphaToOneEnable = VK_FALSE; // Optional
			multisampling.sampleShadingEnable = VK_TRUE; // enable sample shading in the pipeline
			multisampling.minSampleShading = .2f; // min fraction for sample shading; closer to one is smoother

			tesselation.sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO;
			tesselation.pNext = nullptr;
			tesselation.flags = 0;
			tesselation.patchControlPoints = 3;

			pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
			pipelineInfo.pNext = nullptr;
			pipelineInfo.flags = 0;
			pipelineInfo.pVertexInputState = &vertexInputInfo;
			pipelineInfo.pInputAssemblyState = &inputAssembly;
			pipelineInfo.pViewportState = &viewportState;
			pipelineInfo.pRasterizationState = &rasterizer;
			pipelineInfo.pMultisampleState = &multisampling;
			pipelineInfo.pColorBlendState = &colorBlending;
			pipelineInfo.pDepthStencilState = &depthStencil;
			pipelineInfo.pTessellationState = &tesselation;

			pipelineInfo.pDynamicState = nullptr; // Optional
			pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Optional
			pipelineInfo.basePipelineIndex = -1; // Optional
		}
	};

	struct ShaderInfo {
		std::string filename;
		VkShaderStageFlagBits stage;
	};

	// from shader reflection
	struct DescriptorInfo {
		uint32_t set;
		uint32_t binding;
	};

	struct Material {
		VkPipelineLayout pipelineLayout;
		VkPipeline pipeline;

		PipelineBuilder *pipelineBuilder = new PipelineBuilder;

		std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
		std::vector<VkShaderModule> shaderModules;
		std::vector<vku::descriptor::DescriptorLayout> reflDescriptors;
		VkDescriptorSetLayout descriptorSetLayout;

		Material(VkDescriptorSetLayout &globalDSetLayout, const VkRenderPass &pass, VkDescriptorSetLayout &passInputDSetLayout, const std::initializer_list<ShaderInfo> shaderStageInfo) {
			pipelineBuilder->pipelineInfo.subpass = 0;
			pipelineBuilder->pipelineInfo.renderPass = pass;

			// load shaders into the pipeline state, and analyze shader for expected descriptors
			for (const ShaderInfo &info : shaderStageInfo) {
				std::vector<uint32_t> fileData = vku::shadercomp::lazyLoadSpirv(info.filename);

				// now compile the actual shader
				VkShaderModule sModule = vku::create::createShaderModule(fileData);
				VkPipelineShaderStageCreateInfo stage = vku::create::createShaderStage(sModule, info.stage);
				shaderModules.push_back(sModule);
				shaderStages.push_back(stage);

				// use reflection to enumerate the inputs of this shader
				{
					// convert from char vector to uint32_t vector, then reflect
					std::vector<uint32_t> spv(fileData.cbegin(), fileData.cend());
					spirv_cross::CompilerGLSL glsl(std::move(spv));
					spirv_cross::ShaderResources resources = glsl.get_shader_resources();

					for (auto &resource : resources.sampled_images)
					{
						unsigned set = glsl.get_decoration(resource.id, spv::DecorationDescriptorSet);
						unsigned binding = glsl.get_decoration(resource.id, spv::DecorationBinding);
						if (set == 2)
							reflDescriptors.push_back({ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_ALL_GRAPHICS });
					}
				}
			}
			pipelineBuilder->pipelineInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
			pipelineBuilder->pipelineInfo.pStages = shaderStages.data();

			// create descriptor layout (expand later)
			vku::descriptor::createDescriptorSetLayout(&descriptorSetLayout, reflDescriptors);

			// create pipeline layout to aggregate all descriptor sets
			std::vector<VkDescriptorSetLayout> dSetLayouts{
				globalDSetLayout,
				passInputDSetLayout,
				this->descriptorSetLayout
			};

			VkPushConstantRange transformPushConst;
			transformPushConst.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
			transformPushConst.offset = 0;
			transformPushConst.size = sizeof(glm::mat4);

			VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
			pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
			pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(dSetLayouts.size());
			pipelineLayoutInfo.pSetLayouts = dSetLayouts.data();
			pipelineLayoutInfo.pushConstantRangeCount = 1;
			pipelineLayoutInfo.pPushConstantRanges = &transformPushConst;

			VkResult result = vkCreatePipelineLayout(vku::state::device, &pipelineLayoutInfo, nullptr, &pipelineLayout);
			if (result != VK_SUCCESS) {
				throw std::runtime_error("Failed to create pipeline layout for material!");
			}

			pipelineBuilder->pipelineInfo.layout = pipelineLayout;
		}

		Material() {}

		void createPipeline() {
			vkCreateGraphicsPipelines(vku::state::device, VK_NULL_HANDLE, 1, &pipelineBuilder->pipelineInfo, nullptr, &pipeline);
			delete pipelineBuilder;
		}

		void bind(VkCommandBuffer cb) {
			vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
		}

		void destroy(VkDescriptorPool &descriptorPool) {
			for (auto &sMod : shaderModules) {
				vkDestroyShaderModule(vku::state::device, sMod, nullptr);
			}

			vkDestroyDescriptorSetLayout(vku::state::device, descriptorSetLayout, nullptr);
			vkDestroyPipelineLayout(vku::state::device, pipelineLayout, nullptr);
			vkDestroyPipeline(vku::state::device, pipeline, nullptr);
		}
	};

	struct MaterialInstance {
		Material *material;

		std::array<VkDescriptorSet, 3> descriptorSets;

		MaterialInstance(Material *mat, VkDescriptorPool &descriptorPool) {
			this->material = mat;

			VkDescriptorSetLayout dSetLayout = this->material->descriptorSetLayout;

			std::vector<VkDescriptorSetLayout> layouts(vku::state::SWAPCHAIN_SIZE, dSetLayout);
			VkDescriptorSetAllocateInfo allocInfo{};
			allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
			allocInfo.descriptorPool = descriptorPool;
			allocInfo.descriptorSetCount = static_cast<uint32_t>(layouts.size());
			allocInfo.pSetLayouts = layouts.data();

			if (vkAllocateDescriptorSets(vku::state::device, &allocInfo, descriptorSets.data()) != VK_SUCCESS) {
				throw std::runtime_error("Failed to allocate descriptor sets!");
			}
		}

		MaterialInstance() {}

		void write(int id, Texture &texture) {
			for (size_t i = 0; i < vku::state::SWAPCHAIN_SIZE; i++) {
				VkWriteDescriptorSet setWrite = texture.getDescriptorWrite(id, descriptorSets[i]);
				vkUpdateDescriptorSets(vku::state::device, 1, &setWrite, 0, nullptr);
			}
		}

		void bind(VkCommandBuffer cb, uint32_t i) {
			vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, material->pipelineLayout, 2, 1, &descriptorSets[i], 0, nullptr);
		}
	};
}