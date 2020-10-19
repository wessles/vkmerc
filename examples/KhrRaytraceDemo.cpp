#include <iostream>
#include <filesystem>

#define VK_ENABLE_BETA_EXTENSIONS

#include <VulkanContext.h>
#include <VulkanDevice.h>
#include <VulkanSwapchain.h>
#include <VulkanTexture.h>
#include <VulkanDescriptorSet.h>
#include <VulkanMesh.h>
#include <VulkanMaterial.h>
#include <VulkanUniform.h>

#include <util/CascadedShadowmap.hpp>

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <pbr/VulkanGltfModel.hpp>
#include <pbr/Ue4BrdfLut.hpp>
#include <pbr/CubemapFiltering.hpp>

#include <rendergraph/RenderGraph.h>
#include <scene/Scene.h>
#include <BaseEngine.h>

#include <util/OrbitCam.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtx/string_cast.hpp>

using namespace std;
using namespace vku;

class Engine : public BaseEngine {
public:
	Engine() {
		this->windowTitle = "KHR_rt Demo";
		this->debugEnabled = false;
		this->width = 900;
		this->height = 900;
	}

	OrbitCam* flycam;
	Scene* scene;

	RenderGraphSchema* graphSchema;
	RenderGraph* graph;
	Pass* main;

	VulkanGltfModel* gltf;
	VulkanTexture* brdf;
	VulkanTexture* irradiancemap;
	VulkanTexture* specmap;

	VulkanTexture* skybox;
	VulkanMeshBuffer* boxMeshBuf;
	VulkanMaterial* mat;
	VulkanMaterialInstance* matInst;

	std::vector<VkCommandBuffer> cmdBufs;

	// Inherited via BaseEngine
	void postInit()
	{
		boxMeshBuf = new VulkanMeshBuffer(context->device, vku::box);

		SceneInfo sInfo{};
		scene = new Scene(context->device, sInfo);

		// load skybox texture
		skybox = new VulkanTexture;
		skybox->image = new VulkanImage(context->device, {
			"res/cubemap/posx.jpg",
			"res/cubemap/negx.jpg",
			"res/cubemap/posy.jpg",
			"res/cubemap/negy.jpg",
			"res/cubemap/posz.jpg",
			"res/cubemap/negz.jpg"
			});
		VulkanImageViewInfo viewInfo{};
		skybox->image->writeImageViewInfo(&viewInfo);
		viewInfo.imageViewType = VK_IMAGE_VIEW_TYPE_CUBE;
		skybox->view = new VulkanImageView(context->device, viewInfo);
		skybox->sampler = new VulkanSampler(context->device, {});

		flycam = new OrbitCam(context->windowHandle);

		graphSchema = new RenderGraphSchema();
		{
			VkSampleCountFlagBits msaaCount = VK_SAMPLE_COUNT_8_BIT;

			PassSchema* main = graphSchema->pass("main", [&](uint32_t i, const VkCommandBuffer& cb) {
				matInst->bind(cb, i);
				matInst->material->bind(cb);
				boxMeshBuf->draw(cb);

				scene->render(cb, i, false);
				});
			main->samples = msaaCount;

			AttachmentSchema* edge = graphSchema->attachment("color", main, {});
			edge->format = context->device->swapchain->screenFormat;
			edge->samples = msaaCount;
			edge->isSwapchain = true;
			edge->resolve = true;

			AttachmentSchema* depth = graphSchema->attachment("depth", main, {});
			depth->format = context->device->swapchain->depthFormat;
			depth->samples = msaaCount;
			depth->isTransient = true;
		}

		graph = new RenderGraph(graphSchema, scene, context->device->swapchain->swapChainLength);
		graph->createLayouts();

		main = graph->getPass("main");

		brdf = generateBRDFLUT(context->device);
		irradiancemap = generateIrradianceCube(context->device, skybox);
		specmap = generatePrefilteredCube(context->device, skybox);
		gltf = new VulkanGltfModel("res/demo/DamagedHelmet.glb", scene, main, brdf, irradiancemap, specmap);
		scene->addObject(gltf);

		// skyboxes don't care about depth testing / writing
		VulkanMaterialInfo matInfo(context->device);
		matInfo.depthStencil.depthWriteEnable = false;
		matInfo.depthStencil.depthTestEnable = false;
		matInfo.rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
		matInfo.shaderStages.push_back({ "res/shaders/skybox/skybox.frag", VK_SHADER_STAGE_FRAGMENT_BIT, {} });
		matInfo.shaderStages.push_back({ "res/shaders/skybox/skybox.vert", VK_SHADER_STAGE_VERTEX_BIT, {} });
		mat = new VulkanMaterial(&matInfo, scene, main);
		matInst = new VulkanMaterialInstance(mat);
		for (VulkanDescriptorSet* set : matInst->descriptorSets) { set->write(0, skybox); }

		buildSwapchainDependants();

		createAccelerationStructure();
	}

	void createAccelerationStructure(VulkanMeshBuffer* meshBuf) {

		VkAccelerationStructureCreateGeometryTypeInfoKHR geomType{};
		geomType.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_GEOMETRY_TYPE_INFO_KHR;
		geomType.indexType = VK_INDEX_TYPE_UINT32;
		geomType.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
		geomType.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
		geomType.maxPrimitiveCount = 1000;
		geomType.maxVertexCount = 1000;
		geomType.allowsTransforms = VK_FALSE;

		VkDeviceOrHostAddressConstKHR vertexAddress, indicesAddress;
		VkBufferDeviceAddressInfo addrInfo{};
		addrInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
		addrInfo.buffer = meshBuf->vBuffer;
		vertexAddress.deviceAddress = vkGetBufferDeviceAddress(context->device->handle, &addrInfo);
		addrInfo.buffer = meshBuf->iBuffer;
		indicesAddress.deviceAddress = vkGetBufferDeviceAddress(context->device->handle, &addrInfo);

		VkAccelerationStructureGeometryTrianglesDataKHR triangles;
		triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
		triangles.vertexFormat = geomType.vertexFormat;
		triangles.vertexData = vertexAddress;
		triangles.vertexStride = sizeof(Vertex);
		triangles.indexType = geomType.indexType;
		triangles.indexData = indicesAddress;
		triangles.transformData = {};

		VkAccelerationStructureGeometryKHR asGeom{};
		asGeom.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR
		VkAccelerationStructureKHR accelStructure;

		VkAccelerationStructureCreateInfoKHR accelCI{};
		accelCI.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
		accelCI.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;

		accelCI.pGeometryInfos = nullptr;


		vkCreateAccelerationStructureKHR(context->device->handle, &accelCI, nullptr, &accelStructure);
	}

	glm::mat4 cascade = glm::mat4(1.0f);

	void updateUniforms(uint32_t i) {
		flycam->update();

		static auto startTime = std::chrono::high_resolution_clock::now();
		auto currentTime = std::chrono::high_resolution_clock::now();
		float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

		float n = 0.01f;
		float f = 5.0f;
		float half = (n + f) / 2.0f;
		float quarter = (n + half) / 2.0f;

		SceneGlobalUniform global{};
		glm::mat4 transform = flycam->getTransform();
		global.view = glm::inverse(transform);
		VkExtent2D swapchainExtent = context->device->swapchain->swapChainExtent;
		global.proj = flycam->getProjMatrix(static_cast<float>(swapchainExtent.width), static_cast<float>(swapchainExtent.height), n, f);
		global.camPos = transform * glm::vec4(0.0, 0.0, 0.0, 1.0);
		global.screenRes = { swapchainExtent.width, swapchainExtent.height };
		global.time = time;
		global.directionalLight = glm::rotate(glm::mat4(1.0), time, glm::vec3(0.0, 1.0, 0.0)) * glm::vec4(1.0, -1.0, 0.0, 0.0);

		scene->updateUniforms(i, &global);
	}

	VkCommandBuffer draw(uint32_t i)
	{
		updateUniforms(i);

		return cmdBufs[i];
	}
	void buildSwapchainDependants()
	{
		graph->createInstances();

		vkFreeCommandBuffers(*context->device, context->device->commandPool, static_cast<uint32_t>(cmdBufs.size()), cmdBufs.data());
		cmdBufs.resize(context->device->swapchain->swapChainLength);
		for (uint32_t i = 0; i < cmdBufs.size(); i++) {
			cmdBufs[i] = context->device->beginCommandBuffer();
			graph->render(cmdBufs[i], i);
			vkEndCommandBuffer(cmdBufs[i]);
		}
	}
	void destroySwapchainDependents()
	{
		graph->destroyInstances();
	}
	void preCleanup()
	{
		destroySwapchainDependents();

		delete skybox;
		delete boxMeshBuf;
		delete matInst;
		delete mat;

		delete flycam;

		delete brdf;
		delete irradiancemap;
		delete specmap;

		graph->destroyLayouts();
		delete scene;
	}
};

int main()
{
	Engine* demo = new Engine();
	demo->run();
	return 0;
}