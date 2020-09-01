#pragma once

#include "NewDemo.hpp"

#include <glm/glm.hpp>

#include "rgraph.h"
using namespace vku::rgraph;

#include "Bvk.hpp"
using namespace vku::image;
using namespace vku::mesh;

#include <spirv_cross/spirv_glsl.hpp>
#include "FlyCam.h"


MeshData box = {
	{
		{{-1, -1, -1}, {}, {}, {1, 1, 1}},
		{{1, -1, -1}, {}, {}, {-1, 1, 1}},
		{{-1, 1, -1}, {}, {}, {1, -1, 1}},
		{{1, 1, -1}, {}, {}, {-1, -1, 1}},
		{{-1, -1, 1}, {}, {}, {1, 1, -1}},
		{{1, -1, 1}, {}, {}, {-1, 1, -1}},
		{{-1, 1, 1}, {}, {}, {1, -1, -1}},
		{{1, 1, 1}, {}, {}, {-1, -1, -1}},
	},
	{
		0, 3, 2,
		0, 1, 3,
		1, 5, 7,
		1, 7, 3,
		4, 7, 5,
		4, 6, 7,
		4, 2, 6,
		4, 0, 2,
		6, 2, 7,
		2, 3, 7,
		1, 4, 5,
		0, 4, 1
	}
};

struct GlobalUniform
{
	glm::mat4 view;
	glm::mat4 proj;
	glm::vec4 camPos;
	glm::vec2 screenRes;
	glm::float32 time;
};

struct UniformBuffer {
	VkBuffer buffer;
	VkDeviceMemory memory;
};

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
			std::vector<char> fileData = vku::io::readFile(info.filename);

			// FIRST: use reflection to enumerate the inputs of this shader
			{
				// convert from char vector to uint32_t vector, then reflect
				std::vector<uint32_t> spv(fileData.size() / sizeof(uint32_t));
				memcpy(spv.data(), fileData.data(), fileData.size());
				spirv_cross::CompilerGLSL glsl(std::move(spv));
				spirv_cross::ShaderResources resources = glsl.get_shader_resources();

				for (auto &resource : resources.sampled_images)
				{
					unsigned set = glsl.get_decoration(resource.id, spv::DecorationDescriptorSet);
					unsigned binding = glsl.get_decoration(resource.id, spv::DecorationBinding);
					if(set == 2)
						reflDescriptors.push_back({ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_ALL_GRAPHICS });
				}
			}

			VkShaderModule sModule = vku::create::createShaderModule(vku::io::readFile(info.filename));
			VkPipelineShaderStageCreateInfo stage = vku::create::createShaderStage(sModule, info.stage);
			shaderModules.push_back(sModule);
			shaderStages.push_back(stage);
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

		VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
		pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(dSetLayouts.size());
		pipelineLayoutInfo.pSetLayouts = dSetLayouts.data();
		pipelineLayoutInfo.pushConstantRangeCount = 0; // Optional
		pipelineLayoutInfo.pPushConstantRanges = nullptr; // Optional

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
};

struct MaterialInstance {
	const Material *material;

	std::array<VkDescriptorSet, 3> descriptorSets;

	MaterialInstance(const Material *mat, VkDescriptorPool &descriptorPool) {
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

	void write(int id, Image &image) {
		for (size_t i = 0; i < vku::state::SWAPCHAIN_SIZE; i++) {
			VkWriteDescriptorSet setWrite = image.getDescriptorWrite(id, descriptorSets[i]);
			vkUpdateDescriptorSets(vku::state::device, 1, &setWrite, 0, nullptr);
		}
	}
};

class Demo : public Engine {

	FlyCam flycam;

	VkDescriptorPool descriptorPool;
	VkDescriptorSetLayout globalDSetLayout;
	std::array<VkDescriptorSet, vku::state::SWAPCHAIN_SIZE> globalDescriptorSets;

	RGraph graph;
	RNode *mainPass;
	RNode *blitPass;

	std::array<VkCommandBuffer, 3> commandBuffers;
	std::array<UniformBuffer, 3> uniformBuffers;

	Image depthImage;
	Image colorImage;
	Image postImage;

	Image cubeMap;

	MeshBuffer skyboxMeshBuf;
	Material skyboxMat;
	MaterialInstance skyboxMatInstance;

	Material testBoxMat;
	MaterialInstance testBoxMatInstance;

public:
	Demo() {
		this->windowName = "MERC";
		this->windowSize = { 1280,720 };
	}

	void postInit() {
		vku::descriptor::createDescriptorSetLayout(&globalDSetLayout, {
			// ubo
			{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT},
		});

		loadMeshes();

		createUniforms();
		createDescriptorSets();

		createRenderGraph();
		createMaterials();
		createCommandBuffers();

		flycam = FlyCam(vku::state::windowHandle);
	}

	void loadMeshes() {
		skyboxMeshBuf.load(box);
	}

	void createRenderGraph() {
		mainPass = graph.addPass([&](uint32_t i, VkCommandBuffer cb) {
			VkDeviceSize offsets[] = { 0 };
			vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, skyboxMat.pipeline);
			std::vector<VkDescriptorSet> descriptors{
				globalDescriptorSets[i],
				mainPass->instances[i].descriptorSet,
				skyboxMatInstance.descriptorSets[i]
			};
			vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, skyboxMat.pipelineLayout, 0, static_cast<uint32_t>(descriptors.size()), descriptors.data(), 0, nullptr);
			VkBuffer skyboxVBuffers[] = { skyboxMeshBuf.vBuffer };
			vkCmdBindVertexBuffers(cb, 0, 1, skyboxVBuffers, offsets);
			vkCmdBindIndexBuffer(cb, skyboxMeshBuf.iBuffer, 0, VK_INDEX_TYPE_UINT32);
			vkCmdDrawIndexed(cb, skyboxMeshBuf.indicesSize, 1, 0, 0, 0);
		});
		//blitPass = graph.addPass([&](uint32_t i, VkCommandBuffer cb) {
		//});
		graph.begin(mainPass);
		graph.terminate(mainPass);
		//graph.terminate(blitPass);

		// one color attachment
		//REdge *color = graph.addAttachment(mainPass, blitPass);
		//color->format = vku::state::screenFormat;
		//color->samples = VK_SAMPLE_COUNT_1_BIT;

		// present attachment
		REdge *output = graph.addAttachment(mainPass, nullptr);
		output->format = vku::state::screenFormat;
		output->samples = VK_SAMPLE_COUNT_1_BIT;

		graph.process(descriptorPool);
	}

	void createUniforms() {
		VkDeviceSize bufferSize = sizeof(GlobalUniform);

		for (size_t i = 0; i < 3; i++) {
			vku::buffer::createBuffer(bufferSize,
				VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
				uniformBuffers[i].buffer, uniformBuffers[i].memory);
		}
	}
	void updateUniform(uint32_t i) {
		static auto startTime = std::chrono::high_resolution_clock::now();
		auto currentTime = std::chrono::high_resolution_clock::now();
		float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

		flycam.update();

		GlobalUniform global{};
		glm::mat4 transform = flycam.getTransform();
		global.view = glm::inverse(transform);
		global.proj = flycam.getProjMatrix(vku::state::swapChainExtent.width, vku::state::swapChainExtent.height, 0.01f, 10.0f);
		global.camPos = transform * glm::vec4(0.0, 0.0, 0.0, 1.0);
		global.screenRes = {vku::state::swapChainExtent.width, vku::state::swapChainExtent.height};
		global.time = time;

		void* data;
		vkMapMemory(vku::state::device, uniformBuffers[i].memory, 0, sizeof(global), 0, &data);
		memcpy(data, &global, sizeof(global));
		vkUnmapMemory(vku::state::device, uniformBuffers[i].memory);
	}

	void createMaterials() {
		{
			// skybox shaders
			skyboxMat = Material(globalDSetLayout, mainPass->pass, mainPass->inputLayout, {
				{"res/shaders/skybox/vert.spv", VK_SHADER_STAGE_VERTEX_BIT},
				{"res/shaders/skybox/frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT}
				});
			skyboxMat.createPipeline();

			// create skybox material instance
			skyboxMatInstance = MaterialInstance(&skyboxMat, this->descriptorPool);
			loadCubemap({
				"res/cubemap/posx.jpg",
				"res/cubemap/negx.jpg",
				"res/cubemap/posy.jpg",
				"res/cubemap/negy.jpg",
				"res/cubemap/posz.jpg",
				"res/cubemap/negz.jpg",
				}, cubeMap);
			cubeMap.view = createImageView(cubeMap.handle, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT, cubeMap.mipLevels, VK_IMAGE_VIEW_TYPE_CUBE, 6);
			VkSamplerCreateInfo samplerCI = vku::create::createSamplerCI();
			samplerCI.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
			samplerCI.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
			if (vkCreateSampler(vku::state::device, &samplerCI, nullptr, &cubeMap.sampler) != VK_SUCCESS) {
				throw std::runtime_error("Failed to create texture sampler!");
			}
			skyboxMatInstance.write(0, cubeMap);
		}
		{
			// 
		}
	}

	void createDescriptorSets() {
		// create descriptor pool
		std::array<VkDescriptorPoolSize, 3> poolSizes{};
		poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		poolSizes[0].descriptorCount = static_cast<uint32_t>(100);
		poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		poolSizes[1].descriptorCount = static_cast<uint32_t>(100);
		poolSizes[2].type = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
		poolSizes[2].descriptorCount = static_cast<uint32_t>(100);

		VkDescriptorPoolCreateInfo poolInfo{};
		poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
		poolInfo.pPoolSizes = poolSizes.data();
		poolInfo.maxSets = static_cast<uint32_t>(100);

		if (vkCreateDescriptorPool(vku::state::device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create descriptor pool!");
		}

		// create global descriptor sets
		{
			std::vector<VkDescriptorSetLayout> layouts(vku::state::SWAPCHAIN_SIZE, globalDSetLayout);
			VkDescriptorSetAllocateInfo allocInfo{};
			allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
			allocInfo.descriptorPool = descriptorPool;
			allocInfo.descriptorSetCount = static_cast<uint32_t>(vku::state::SWAPCHAIN_SIZE);
			allocInfo.pSetLayouts = layouts.data();

			if (vkAllocateDescriptorSets(vku::state::device, &allocInfo, globalDescriptorSets.data()) != VK_SUCCESS) {
				throw std::runtime_error("Failed to allocate descriptor sets!");
			}

			for (size_t i = 0; i < vku::state::SWAPCHAIN_SIZE; i++) {
				VkDescriptorBufferInfo bufferInfo{};
				bufferInfo.buffer = uniformBuffers[i].buffer;
				bufferInfo.offset = 0;
				bufferInfo.range = sizeof(GlobalUniform);

				std::array<VkWriteDescriptorSet, 1> descriptorWrites{};

				descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				descriptorWrites[0].dstSet = globalDescriptorSets[i];
				descriptorWrites[0].dstBinding = 0;
				descriptorWrites[0].dstArrayElement = 0;
				descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
				descriptorWrites[0].descriptorCount = 1;
				descriptorWrites[0].pBufferInfo = &bufferInfo;

				vkUpdateDescriptorSets(vku::state::device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
			}
		}
	}

	void createCommandBuffers() {
		// allocate
		VkCommandBufferAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.commandPool = vku::state::commandPool;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandBufferCount = (uint32_t)commandBuffers.size();

		if (vkAllocateCommandBuffers(vku::state::device, &allocInfo, commandBuffers.data()) != VK_SUCCESS) {
			throw std::runtime_error("Failed to allocate command buffers!");
		}

		// prepare recording
		VkCommandBufferBeginInfo cmdBufInfo{};
		cmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		cmdBufInfo.flags = 0; // Optional
		cmdBufInfo.pInheritanceInfo = nullptr; // Optional

		// record identical command buffers for each swap buffer
		for (int32_t i = 0; i < commandBuffers.size(); i++) {
			if (vkBeginCommandBuffer(commandBuffers[i], &cmdBufInfo) != VK_SUCCESS) {
				throw std::runtime_error("Could not begin command buffer!");
			}

			for (auto &node : graph.nodes) {
				std::vector<VkClearValue> clearValues{};
				for (REdge *edge : node->in) {
					VkClearValue clear{};
					clear.color = { 0.0f, 0.0f, 0.0f, 1.0f };
					clear.depthStencil = { 1, 0 };
					clearValues.push_back(clear);
				}
				for (REdge *edge : node->out) {
					VkClearValue clear{};
					clear.color = { 1.0f, 1.0f, 1.0f, 1.0f };
					clear.depthStencil = { 1, 0 };
					clearValues.push_back(clear);
				}

				VkRenderPassBeginInfo passBeginInfo{};
				passBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
				passBeginInfo.renderArea.offset = { 0, 0 };
				passBeginInfo.renderArea.extent = vku::state::swapChainExtent;
				passBeginInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
				passBeginInfo.pClearValues = clearValues.data();

				passBeginInfo.framebuffer = node->instances[i].framebuffer;
				passBeginInfo.renderPass = node->pass;

				vkCmdBeginRenderPass(commandBuffers[i], &passBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
				{
					node->commands(i, commandBuffers[i]);
				}
				vkCmdEndRenderPass(commandBuffers[i]);
			}


			if (vkEndCommandBuffer(commandBuffers[i]) != VK_SUCCESS) {
				throw std::runtime_error("Failed to end command buffer record!");
			}
		}
	}

	VkCommandBuffer *draw(uint32_t imageIdx) {
		updateUniform(imageIdx);
		return &commandBuffers[imageIdx];
	}

	void postSwapchainRefresh() {
		graph.destroy();
		graph.process(descriptorPool);
	}

	void preCleanup() {
		vkDestroyDescriptorSetLayout(vku::state::device, globalDSetLayout, nullptr);
		vkDestroyPipeline(vku::state::device, skyboxMat.pipeline, nullptr);
		vkDestroyPipelineLayout(vku::state::device, skyboxMat.pipelineLayout, nullptr);

		graph.destroy();
	}
};