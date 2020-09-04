#pragma once

#include "NewDemo.hpp"

#include <glm/glm.hpp>
#include <spirv_cross/spirv_glsl.hpp>

#include "Vku.hpp"

#include "rgraph.h"
using namespace vku::rgraph;

#include "material.h"
using namespace vku::material;

#include "FlyCam.h"
using namespace vku::image;
using namespace vku::mesh;


template<class UniformStructType>
class UniformBuffer {
	std::array<VkDescriptorBufferInfo, vku::state::SWAPCHAIN_SIZE> bufferInfo;
	std::array<VkBuffer, vku::state::SWAPCHAIN_SIZE> buffer;
	std::array<VkDeviceMemory, vku::state::SWAPCHAIN_SIZE> memory;

public:
	void create(VkDeviceSize bufferSize) {
		for (size_t i = 0; i < vku::state::SWAPCHAIN_SIZE; i++) {
			vku::buffer::createBuffer(bufferSize,
				VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
				buffer[i], memory[i]);
		}
	}
	void write(uint32_t i, UniformStructType &global) {
		void* data;
		vkMapMemory(vku::state::device, memory[i], 0, sizeof(UniformStructType), 0, &data);
		memcpy(data, &global, sizeof(global));
		vkUnmapMemory(vku::state::device, memory[i]);
	}
	void destroy() {
		for (size_t i = 0; i < vku::state::SWAPCHAIN_SIZE; i++) {
			vkDestroyBuffer(vku::state::device, buffer[i], nullptr);
			vkFreeMemory(vku::state::device, memory[i], nullptr);
		}
	}
	VkWriteDescriptorSet getDescriptorWrite(uint32_t i, VkDescriptorSet &descriptorSet) {
		bufferInfo[i].buffer = buffer[i];
		bufferInfo[i].offset = 0;
		bufferInfo[i].range = sizeof(UniformStructType);

		VkWriteDescriptorSet descriptorWrite{};
		descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrite.dstSet = descriptorSet;
		descriptorWrite.dstBinding = 0;
		descriptorWrite.dstArrayElement = 0;
		descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descriptorWrite.descriptorCount = 1;
		descriptorWrite.pBufferInfo = &bufferInfo[i];
		return descriptorWrite;
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

class Demo : public Engine {

	FlyCam flycam;

	VkDescriptorPool descriptorPool;
	VkDescriptorSetLayout globalDSetLayout;
	std::array<VkDescriptorSet, vku::state::SWAPCHAIN_SIZE> globalDescriptorSets;
	VkPipelineLayout globalPipelineLayout;

	RGraph graph;
	RNode *mainPass;
	RNode *blurXPass;
	RNode *blurYPass;
	RNode *blitPass;

	std::array<VkCommandBuffer, 3> commandBuffers;
	UniformBuffer<GlobalUniform> globalUniforms;

	Image cubeMap;

	MeshBuffer boxMeshBuf;
	Material skyboxMat;
	MaterialInstance skyboxMatInstance;

	MeshBuffer blitMeshBuf;
	Material blitMat;
	MaterialInstance blitMatInstance;

	Material boxMat;
	MaterialInstance boxMatInstance;

	Material blurXMat;
	MaterialInstance blurXMatInstance;
	Material blurYMat;
	MaterialInstance blurYMatInstance;

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

		flycam = FlyCam(vku::state::windowHandle);

		loadMeshes();

		buildRenderGraph();

		buildSwapchainDependants();
	}

	void loadMeshes() {
		boxMeshBuf.load(box);
		blitMeshBuf.load(blit);
	}

	void buildRenderGraph() {
		mainPass = graph.addPass([&](uint32_t i, VkCommandBuffer cb) {
			VkDeviceSize offsets[] = { 0 };
			vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, skyboxMat.pipeline);
			vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, skyboxMat.pipelineLayout, 2, 1, &skyboxMatInstance.descriptorSets[i], 0, nullptr);
			vkCmdBindVertexBuffers(cb, 0, 1, &boxMeshBuf.vBuffer, offsets);
			vkCmdBindIndexBuffer(cb, boxMeshBuf.iBuffer, 0, VK_INDEX_TYPE_UINT32);
			vkCmdDrawIndexed(cb, boxMeshBuf.indicesSize, 1, 0, 0, 0);

			vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, boxMat.pipeline);
			vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, boxMat.pipelineLayout, 2, 1, &boxMatInstance.descriptorSets[i], 0, nullptr);
			vkCmdDrawIndexed(cb, boxMeshBuf.indicesSize, 1, 0, 0, 0);
		});
		blurXPass = graph.addPass([&](uint32_t i, VkCommandBuffer cb) {
			VkDeviceSize offsets[] = { 0 };
			vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, blurXMat.pipeline);
			vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, blurXMat.pipelineLayout, 2, 1, &blurXMatInstance.descriptorSets[i], 0, nullptr);
			vkCmdBindVertexBuffers(cb, 0, 1, &blitMeshBuf.vBuffer, offsets);
			vkCmdBindIndexBuffer(cb, blitMeshBuf.iBuffer, 0, VK_INDEX_TYPE_UINT32);
			vkCmdDrawIndexed(cb, blitMeshBuf.indicesSize, 1, 0, 0, 0);
		});
		blurYPass = graph.addPass([&](uint32_t i, VkCommandBuffer cb) {
			VkDeviceSize offsets[] = { 0 };
			vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, blurYMat.pipeline);
			vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, blurYMat.pipelineLayout, 2, 1, &blurYMatInstance.descriptorSets[i], 0, nullptr);
			vkCmdBindVertexBuffers(cb, 0, 1, &blitMeshBuf.vBuffer, offsets);
			vkCmdBindIndexBuffer(cb, blitMeshBuf.iBuffer, 0, VK_INDEX_TYPE_UINT32);
			vkCmdDrawIndexed(cb, blitMeshBuf.indicesSize, 1, 0, 0, 0);
		});
		blitPass = graph.addPass([&](uint32_t i, VkCommandBuffer cb) {
			VkDeviceSize offsets[] = { 0 };
			vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, blitMat.pipeline);
			vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, blitMat.pipelineLayout, 2, 1, &blitMatInstance.descriptorSets[i], 0, nullptr);
			vkCmdBindVertexBuffers(cb, 0, 1, &blitMeshBuf.vBuffer, offsets);
			vkCmdBindIndexBuffer(cb, blitMeshBuf.iBuffer, 0, VK_INDEX_TYPE_UINT32);
			vkCmdDrawIndexed(cb, blitMeshBuf.indicesSize, 1, 0, 0, 0);
		});

		graph.begin(mainPass);
		graph.terminate(mainPass);
		graph.terminate(blitPass);

		REdge *color = graph.addAttachment(mainPass, { blitPass, blurXPass });
		color->format = vku::state::screenFormat;
		color->samples = VK_SAMPLE_COUNT_1_BIT;

		REdge *blurred1 = graph.addAttachment(blurXPass, { blurYPass });
		blurred1->format = vku::state::screenFormat;
		blurred1->samples = VK_SAMPLE_COUNT_1_BIT;

		REdge *blurred2 = graph.addAttachment(blurYPass, { blitPass });
		blurred2->format = vku::state::screenFormat;
		blurred2->samples = VK_SAMPLE_COUNT_1_BIT;

		REdge *depth = graph.addAttachment(mainPass, { blitPass });
		depth->format = vku::state::depthFormat;
		depth->samples = VK_SAMPLE_COUNT_1_BIT;

		REdge *output = graph.addAttachment(blitPass, {});
		output->format = vku::state::screenFormat;
		output->samples = VK_SAMPLE_COUNT_1_BIT;
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

		globalUniforms.write(i, global);
	}

	void createMaterials() {
		// skybox
		{
			skyboxMat = Material(globalDSetLayout, mainPass->pass, mainPass->inputLayout, {
				{"res/shaders/skybox/vert.spv", VK_SHADER_STAGE_VERTEX_BIT},
				{"res/shaders/skybox/frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT}
				});
			// skyboxes don't care about depth testing / writing
			skyboxMat.pipelineBuilder->depthStencil.depthWriteEnable = false;
			skyboxMat.pipelineBuilder->depthStencil.depthTestEnable = false;
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
			if (vkCreateSampler(vku::state::device, &samplerCI, nullptr, &cubeMap.sampler) != VK_SUCCESS) {
				throw std::runtime_error("Failed to create texture sampler!");
			}
			skyboxMatInstance.write(0, cubeMap);
		}
		// blit
		{
			blitMat = Material(globalDSetLayout, blitPass->pass, blitPass->inputLayout, {
				{"res/shaders/post/vert.spv", VK_SHADER_STAGE_VERTEX_BIT},
				{"res/shaders/post/frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT}
				});
			blitMat.createPipeline();
			blitMatInstance = MaterialInstance(&blitMat, this->descriptorPool);
		}
		// blur X
		{
			blurXMat = Material(globalDSetLayout, blurXPass->pass, blurXPass->inputLayout, {
				{"res/shaders/post/vert.spv", VK_SHADER_STAGE_VERTEX_BIT},
				{"res/shaders/blurX/frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT}
				});
			blurXMat.createPipeline();
			blurXMatInstance = MaterialInstance(&blurXMat, this->descriptorPool);
		}
		// blur Y
		{
			blurYMat = Material(globalDSetLayout, blurYPass->pass, blurYPass->inputLayout, {
				{"res/shaders/post/vert.spv", VK_SHADER_STAGE_VERTEX_BIT},
				{"res/shaders/blurY/frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT}
				});
			blurYMat.createPipeline();
			blurYMatInstance = MaterialInstance(&blurYMat, this->descriptorPool);
		}
		// box
		{
			boxMat = Material(globalDSetLayout, mainPass->pass, mainPass->inputLayout, {
				{"res/shaders/box/vert.spv", VK_SHADER_STAGE_VERTEX_BIT},
				{"res/shaders/box/frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT}
				});
			boxMat.createPipeline();
			boxMatInstance = MaterialInstance(&boxMat, this->descriptorPool);
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

		std::vector<VkDescriptorSetLayout> layouts(vku::state::SWAPCHAIN_SIZE, globalDSetLayout);

		// create global descriptor sets
		{
			VkDescriptorSetAllocateInfo allocInfo{};
			allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
			allocInfo.descriptorPool = descriptorPool;
			allocInfo.descriptorSetCount = static_cast<uint32_t>(vku::state::SWAPCHAIN_SIZE);
			allocInfo.pSetLayouts = layouts.data();

			if (vkAllocateDescriptorSets(vku::state::device, &allocInfo, globalDescriptorSets.data()) != VK_SUCCESS) {
				throw std::runtime_error("Failed to allocate descriptor sets!");
			}

			for (size_t i = 0; i < vku::state::SWAPCHAIN_SIZE; i++) {
				std::array<VkWriteDescriptorSet, 1> descriptorWrites{globalUniforms.getDescriptorWrite(i, globalDescriptorSets[i])};
				vkUpdateDescriptorSets(vku::state::device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
			}
		}

		// Create pipeline layout
		VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
		pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(layouts.size());
		pipelineLayoutInfo.pSetLayouts = layouts.data();
		pipelineLayoutInfo.pushConstantRangeCount = 0; // Optional
		pipelineLayoutInfo.pPushConstantRanges = nullptr; // Optional

		VkResult result = vkCreatePipelineLayout(vku::state::device, &pipelineLayoutInfo, nullptr, &globalPipelineLayout);
		if (result != VK_SUCCESS) {
			throw std::runtime_error("Failed to create pipeline layout for material!");
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

			vkCmdBindDescriptorSets(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, globalPipelineLayout, 0, 1, &globalDescriptorSets[i], 0, nullptr);

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

				vkCmdBindDescriptorSets(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, node->pipelineLayout, 1, 1, &node->instances[i].descriptorSet, 0, nullptr);

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

	void buildSwapchainDependants() {
		globalUniforms.create(sizeof(GlobalUniform));
		createDescriptorSets();

		graph.process(globalDSetLayout, descriptorPool);
		createMaterials();
		createCommandBuffers();
	}

	void destroySwapchainDependents() {
		vkFreeCommandBuffers(vku::state::device, vku::state::commandPool, static_cast<uint32_t>(commandBuffers.size()), commandBuffers.data());
		
		skyboxMat.destroy(descriptorPool);
		blitMat.destroy(descriptorPool);
		blurXMat.destroy(descriptorPool);
		blurYMat.destroy(descriptorPool);
		boxMat.destroy(descriptorPool);
		destroyImage(vku::state::device, cubeMap);
		
		graph.destroy(descriptorPool);

		vkDestroyDescriptorPool(vku::state::device, descriptorPool, nullptr);

		globalUniforms.destroy();
	}

	void preCleanup() {
		destroySwapchainDependents();

		boxMeshBuf.destroy();
		blitMeshBuf.destroy();

		vkDestroyDescriptorSetLayout(vku::state::device, globalDSetLayout, nullptr);
		vkDestroyPipelineLayout(vku::state::device, globalPipelineLayout, nullptr);
	}
};