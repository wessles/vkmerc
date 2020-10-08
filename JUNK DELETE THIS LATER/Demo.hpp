#pragma once

#include "NewDemo.hpp"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
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

#include "gltfload.h"
#include "ue4_brdf_table.hpp"
#include "cubemapgen.hpp"



struct GlobalUniform
{
	glm::mat4 view;
	glm::mat4 proj;
	glm::vec4 camPos;
	glm::vec4 directionalLight;
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
	RNode *blitPass;

	std::array<VkCommandBuffer, 3> commandBuffers;
	UniformBuffer<GlobalUniform> globalUniforms;

	vku::gltf::GltfModel gltfModel;

	Texture cubeMap;
	Texture radianceMap;
	Texture specularMap;

	MeshBuffer boxMeshBuf;
	Material skyboxMat;
	MaterialInstance skyboxMatInstance;

	MeshBuffer blitMeshBuf;
	Material blitMat;
	MaterialInstance blitMatInstance;

	Material boxMat;
	MaterialInstance boxMatInstance;

	UE4BrdfTable brdfGen{};

public:
	Demo() {
		this->windowName = "MERC";
		this->windowSize = { 800,800 };
	}

	void postInit() {
		vku::descriptor::createDescriptorSetLayout(&globalDSetLayout, {
			// ubo
			{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT},
		});

		flycam = FlyCam(vku::state::windowHandle);

		buildRenderGraph();

		globalUniforms.create(sizeof(GlobalUniform));
		createDescriptorSets();

		graph.processLayouts(globalDSetLayout, descriptorPool);

		loadImages();
		createMaterials();
		loadMeshes();

		buildSwapchainDependants();
	}

	void loadImages() {
		loadCubemap({
				"res/cubemap/posx.jpg",
				"res/cubemap/negx.jpg",
				"res/cubemap/posy.jpg",
				"res/cubemap/negy.jpg",
				"res/cubemap/posz.jpg",
				"res/cubemap/negz.jpg",
			}, cubeMap.image);
		cubeMap.image.view = createImageView(cubeMap.image.handle, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT, cubeMap.image.mipLevels, VK_IMAGE_VIEW_TYPE_CUBE, 6);
		VkSamplerCreateInfo samplerCI = vku::create::createSamplerCI();
		if (vkCreateSampler(vku::state::device, &samplerCI, nullptr, &cubeMap.sampler) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create texture sampler!");
		}

		brdfGen.generateBRDFLUT();
		specularMap = cubemapgen::generatePrefilteredCube(cubeMap);
		radianceMap = cubemapgen::generateIrradianceCube(cubeMap);
	}

	void loadMeshes() {
		boxMeshBuf.load(box);
		blitMeshBuf.load(blit);
		gltfModel = vku::gltf::GltfModel("res/demo/DamagedHelmet.glb", globalDSetLayout, mainPass, descriptorPool,
			brdfGen.texture, radianceMap, specularMap);
	}

	void buildRenderGraph() {
		mainPass = graph.addPass([&](uint32_t i, VkCommandBuffer cb) {
			VkDeviceSize offsets[] = { 0 };

			vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, skyboxMat.pipeline);
			vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, skyboxMat.pipelineLayout, 2, 1, &skyboxMatInstance.descriptorSets[i], 0, nullptr);
			vkCmdBindVertexBuffers(cb, 0, 1, &boxMeshBuf.vBuffer, offsets);
			vkCmdBindIndexBuffer(cb, boxMeshBuf.iBuffer, 0, VK_INDEX_TYPE_UINT32);
			vkCmdDrawIndexed(cb, boxMeshBuf.indicesSize, 1, 0, 0, 0);

			gltfModel.draw(cb, i);
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
		graph.terminate(blitPass);

		REdge *color = graph.addAttachment(mainPass, { blitPass });
		color->format = vku::state::screenFormat;
		color->samples = VK_SAMPLE_COUNT_1_BIT;

		// transient depth texture
		REdge *depth = graph.addAttachment(mainPass, { nullptr });
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
		global.directionalLight = glm::rotate(glm::mat4(1.0), time, glm::vec3(0.0, 1.0, 0.0)) * glm::vec4(1.0, -1.0, 0.0, 0.0);

		globalUniforms.write(i, global);
	}

	void createMaterials() {

		// skybox
		{
			skyboxMat = Material(globalDSetLayout, mainPass->pass, mainPass->inputLayout, {
				{"res/shaders/skybox/skybox.vert", VK_SHADER_STAGE_VERTEX_BIT},
				{"res/shaders/skybox/skybox.frag", VK_SHADER_STAGE_FRAGMENT_BIT}
				});
			// skyboxes don't care about depth testing / writing
			skyboxMat.pipelineBuilder->depthStencil.depthWriteEnable = false;
			skyboxMat.pipelineBuilder->depthStencil.depthTestEnable = false;
			skyboxMat.pipelineBuilder->rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
			skyboxMat.createPipeline();

			// create skybox material instance
			skyboxMatInstance = MaterialInstance(&skyboxMat, this->descriptorPool);
			skyboxMatInstance.write(0, cubeMap);
		}
		// blit
		{
			blitMat = Material(globalDSetLayout, blitPass->pass, blitPass->inputLayout, {
				{"res/shaders/post/post.vert", VK_SHADER_STAGE_VERTEX_BIT},
				{"res/shaders/post/post.frag", VK_SHADER_STAGE_FRAGMENT_BIT}
				});
			blitMat.createPipeline();
			blitMatInstance = MaterialInstance(&blitMat, this->descriptorPool);
		}
		// box
		{
			boxMat = Material(globalDSetLayout, mainPass->pass, mainPass->inputLayout, {
				{"res/shaders/box/box.vert", VK_SHADER_STAGE_VERTEX_BIT},
				{"res/shaders/box/box.frag", VK_SHADER_STAGE_FRAGMENT_BIT}
				});
			boxMat.createPipeline();
			boxMatInstance = MaterialInstance(&boxMat, this->descriptorPool);

			boxMatInstance.write(0, brdfGen.texture);
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
		poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
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

		// transformation matrix push constant
		VkPushConstantRange transformPushConst;
		transformPushConst.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
		transformPushConst.offset = 0;
		transformPushConst.size = sizeof(glm::mat4);

		// Create pipeline layout
		VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
		pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(layouts.size());
		pipelineLayoutInfo.pSetLayouts = layouts.data();
		pipelineLayoutInfo.pushConstantRangeCount = 1;
		pipelineLayoutInfo.pPushConstantRanges = &transformPushConst;

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

		uint32_t width = vku::state::swapChainExtent.width;
		uint32_t height = vku::state::swapChainExtent.height;

		// we'll set width/height in the loop
		VkViewport viewport{};
		viewport.x = 0.0;
		viewport.y = 0.0;
		viewport.width = width;
		viewport.height = height;
		viewport.minDepth = 0.0;
		viewport.maxDepth = 1.0;
		VkRect2D scissor{};
		scissor.offset = { 0,0 };
		scissor.extent = { width,height };

		// record identical command buffers for each swap buffer
		for (int32_t i = 0; i < commandBuffers.size(); i++) {
			if (vkBeginCommandBuffer(commandBuffers[i], &cmdBufInfo) != VK_SUCCESS) {
				throw std::runtime_error("Could not begin command buffer!");
			}

			// set viewport / scissor before any drawing
			// this is because materials have dynamic viewport & scissor state by default
			vkCmdSetViewport(commandBuffers[i], 0, 1, &viewport);
			vkCmdSetScissor(commandBuffers[i], 0, 1, &scissor);

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
				passBeginInfo.renderArea.extent = scissor.extent;
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
		graph.processInstances(descriptorPool);

		createCommandBuffers();
	}

	void destroySwapchainDependents() {
		vkFreeCommandBuffers(vku::state::device, vku::state::commandPool, static_cast<uint32_t>(commandBuffers.size()), commandBuffers.data());
		
		graph.destroyInstances(descriptorPool);
	}

	void preCleanup() {
		destroySwapchainDependents();

		skyboxMat.destroy(descriptorPool);
		blitMat.destroy(descriptorPool);
		boxMat.destroy(descriptorPool);
		gltfModel.destroy(descriptorPool);

		globalUniforms.destroy();

		graph.destroyLayouts();

		destroyTexture(vku::state::device, cubeMap);
		destroyTexture(vku::state::device, radianceMap);
		destroyTexture(vku::state::device, specularMap);
		destroyTexture(vku::state::device, brdfGen.texture);

		boxMeshBuf.destroy();
		blitMeshBuf.destroy();
		
		vkDestroyDescriptorPool(vku::state::device, descriptorPool, nullptr);
		vkDestroyDescriptorSetLayout(vku::state::device, globalDSetLayout, nullptr);
		vkDestroyPipelineLayout(vku::state::device, globalPipelineLayout, nullptr);
	}
};