#include "VulkanMaterial.h"

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
	VulkanMaterialInfo::VulkanMaterialInfo(VulkanDevice* const device) {
		this->device = device;

		inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		inputAssembly.primitiveRestartEnable = VK_FALSE;
		inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

		vertexBindingDescription = Vertex::getBindingDescription();
		vertexAttributeDescriptions = Vertex::getAttributeDescriptions();

		vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertexInput.vertexBindingDescriptionCount = 1;
		vertexInput.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexAttributeDescriptions.size());
		vertexInput.pVertexBindingDescriptions = &vertexBindingDescription; // Optional
		vertexInput.pVertexAttributeDescriptions = vertexAttributeDescriptions.data(); // Optional

		viewport.x = 0.0f;
		viewport.y = 0.0f;
		VkExtent2D& swapchainExtent = device->swapchain->swapChainExtent;
		viewport.width = (float)swapchainExtent.width;
		viewport.height = (float)swapchainExtent.height;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		scissor.offset = { 0, 0 };
		scissor.extent = swapchainExtent;
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

		dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStateList.size());
		dynamicState.pDynamicStates = dynamicStateList.data();

		pipeline.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipeline.pNext = nullptr;
		pipeline.flags = 0;
		pipeline.pVertexInputState = &vertexInput;
		pipeline.pInputAssemblyState = &inputAssembly;
		pipeline.pViewportState = &viewportState;
		pipeline.pRasterizationState = &rasterizer;
		pipeline.pMultisampleState = &multisampling;
		pipeline.pColorBlendState = &colorBlending;
		pipeline.pDepthStencilState = &depthStencil;
		pipeline.pTessellationState = &tesselation;
		pipeline.pDynamicState = &dynamicState;

		pipeline.basePipelineHandle = VK_NULL_HANDLE; // Optional
		pipeline.basePipelineIndex = -1; // Optional

		VkPushConstantRange maxPushConst;
		maxPushConst.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT;
		maxPushConst.offset = 0;
		maxPushConst.size = 128;
		pushConstRanges.push_back(maxPushConst);
	}

	VulkanMaterial::VulkanMaterial(VulkanMaterialInfo* const matInfo, Scene* const scene, Pass* const pass) {
		this->scene = scene;

		std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
		std::vector<VulkanShader*> shaderModules;
		std::vector<DescriptorLayout> reflDescriptors;

		matInfo->pipeline.subpass = 0;
		matInfo->pipeline.renderPass = pass->pass;
		matInfo->multisampling.rasterizationSamples = pass->schema->samples;

		// load shaders into the pipeline state, and analyze shader for expected descriptors
		for (const ShaderInfo& info : matInfo->shaderStages) {
			std::vector<uint32_t> fileData = lazyLoadSpirv(info.filename, info.macros);

			// now compile the actual shader
			VulkanShader* sModule = new VulkanShader(scene->device, fileData);
			VkPipelineShaderStageCreateInfo stage = sModule->getShaderStage(info.stage);
			shaderModules.push_back(sModule);
			shaderStages.push_back(stage);

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
		}
		matInfo->pipeline.stageCount = static_cast<uint32_t>(shaderStages.size());
		matInfo->pipeline.pStages = shaderStages.data();

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
		pipelineLayoutInfo.pushConstantRangeCount = static_cast<uint32_t>(matInfo->pushConstRanges.size());
		pipelineLayoutInfo.pPushConstantRanges = matInfo->pushConstRanges.data();

		VkResult result = vkCreatePipelineLayout(*scene->device, &pipelineLayoutInfo, nullptr, &pipelineLayout);
		if (result != VK_SUCCESS) {
			throw std::runtime_error("Failed to create pipeline layout for material!");
		}

		matInfo->pipeline.layout = pipelineLayout;
		vkCreateGraphicsPipelines(*scene->device, VK_NULL_HANDLE, 1, &matInfo->pipeline, nullptr, &pipeline);

		for (auto& sMod : shaderModules) {
			delete sMod;
		}
	}

	void VulkanMaterial::bind(VkCommandBuffer cb) {
		vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
	}

	VulkanMaterial::~VulkanMaterial() {
		vkDestroyDescriptorSetLayout(*scene->device, *descriptorSetLayout, nullptr);
		vkDestroyPipelineLayout(*scene->device, pipelineLayout, nullptr);
		vkDestroyPipeline(*scene->device, pipeline, nullptr);
	}


	VulkanMaterialInstance::VulkanMaterialInstance(VulkanMaterial* mat) {
		this->material = mat;

		descriptorSets.resize(mat->scene->device->swapchain->swapChainLength);
		for (uint32_t i = 0; i < descriptorSets.size(); i++) {
			descriptorSets[i] = new VulkanDescriptorSet(material->descriptorSetLayout);
		}
	}

	VulkanMaterialInstance::~VulkanMaterialInstance() {
		for (VulkanDescriptorSet* dset : descriptorSets) {
			delete dset;
		}
	}

	void VulkanMaterialInstance::bind(VkCommandBuffer cb, uint32_t i) {
		vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, material->pipelineLayout, 2, 1, &descriptorSets[i]->handle, 0, nullptr);
	}
}