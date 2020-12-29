#include <Tracy.hpp>

#include <iostream>
#include <filesystem>

#include <VulkanContext.h>
#include <VulkanDevice.h>
#include <VulkanSwapchain.h>
#include <VulkanTexture.h>
#include <VulkanDescriptorSet.h>
#include <VulkanMesh.h>
#include <VulkanMaterial.h>
#include <VulkanUniform.h>
#include <shader/ShaderCache.h>
#include <scene/Skybox.hpp>

#include <util/CascadedShadowmap.hpp>

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <pbr/VulkanGltfModel.hpp>
#include <pbr/VulkanObjModel.h>
#include <pbr/Ue4BrdfLut.hpp>
#include <pbr/CubemapFiltering.hpp>

#include <rendergraph/RenderGraph.h>
#include <scene/Scene.h>
#include <BaseEngine.h>

#include <util/FlyCam.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtx/string_cast.hpp>
#include <imgui\VulkanImguiInstance.hpp>
#include <glm/gtc/random.hpp>
#include <glm/gtx/transform2.hpp>

using namespace std;
using namespace vku;

void* operator new(std::size_t count)
{
	auto ptr = malloc(count);
	TracyAlloc(ptr, count);
	return ptr;
}
void operator delete(void* ptr) noexcept
{
	TracyFree(ptr);
	free(ptr);
}

class Engine : public BaseEngine {
public:
	Engine() {
		this->windowTitle = "SSAO Demo";
		this->width = 900;
		this->height = 900;
	}

	FlyCam* flycam;
	Scene* scene;

	RenderGraphSchema* graphSchema;
	RenderGraph* graph;
	Pass* mainPass;
	Pass* blitPass;

	VulkanObjModel* platform;
	VulkanGltfModel* gltf;
	VulkanTexture* brdf;
	VulkanTexture* irradiancemap;
	VulkanTexture* specmap;

	VulkanTexture* skybox;

	VulkanImguiInstance* gui;

	std::vector<VkCommandBuffer> cmdBufs;

	struct SSAOParams {
		float nearPlane;
		float farPlane;
		float intensity;
	};

	SSAOParams ssao{};
	VulkanUniform* ssaoUniform;

	struct BlurUniform {
		glm::vec2 dir;
		int pixelStep;
	} ssaoHorizBlurParams{ {1,0}, 4 }, ssaoVertiBlurParams{ {0,1}, 4 };
	VulkanUniform* ssaoHorizBlurUniform;
	VulkanUniform* ssaoVertiBlurUniform;



	////////////
	// LIGHTS //
	////////////

	struct LightUniform {
		glm::vec4 colorAndIntensity; // rgb=color, a=intensity
	};

	struct LightPassUniform {
		uint32_t lightCount;
		LightUniform lights[];
	};

	VulkanUniform* lightDataUniform;

	struct Light : Object {
		VulkanMeshBuffer *meshBuf;
		Pass* pass;
		Light(VulkanDevice* device, Pass *pass) {
			meshBuf = new VulkanMeshBuffer(device, vku::sphere);
			this->pass = pass;
		}
		~Light() {
			delete meshBuf;
		}
		virtual void render(VkCommandBuffer cmdBuf, uint32_t swapIdx, bool noMaterial) override
		{
			vkCmdPushConstants(cmdBuf, this->pass->material->pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(glm::mat4), &this->localTransform);
			meshBuf->draw(cmdBuf);
		}
		virtual glm::mat4 getAABBTransform() override
		{
			return glm::mat4();
		}
	};

	void initLights(Scene* scene, Pass * lights) {
		auto x = new Light(context->device, lights);
		scene->addObject(x);
		x->layer = 1 << 3; // light layer
		x->localTransform = glm::scale(glm::vec3(30.0,30.0,30.0));
	}

	// Inherited via BaseEngine
	void postInit()
	{
		SceneInfo sInfo{};
		scene = new Scene(context->device, sInfo);

		flycam = new FlyCam(context->windowHandle);

		graphSchema = new RenderGraphSchema();
		{
			// ATTACHMENTS

			AttachmentSchema* albedo = graphSchema->attachment("albedo");
			albedo->format = VK_FORMAT_R8G8B8A8_SRGB;
			albedo->isSampled = true;

			AttachmentSchema* emissive = graphSchema->attachment("emissive");
			emissive->format = VK_FORMAT_R8G8B8A8_SRGB;
			emissive->isSampled = true;

			AttachmentSchema* normal = graphSchema->attachment("normal");
			normal->format = VK_FORMAT_R16G16B16A16_SNORM;
			normal->isSampled = true;

			AttachmentSchema* position = graphSchema->attachment("position");
			position->format = VK_FORMAT_R16G16B16A16_SFLOAT;
			position->isSampled = true;

			AttachmentSchema* material = graphSchema->attachment("material");
			material->format = VK_FORMAT_R8G8B8A8_UNORM;
			material->isSampled = true;

			AttachmentSchema* ao = graphSchema->attachment("ao");
			ao->format = VK_FORMAT_R8_UNORM;
			ao->isSampled = true;

			AttachmentSchema* ao2 = graphSchema->attachment("ao2");
			ao2->format = VK_FORMAT_R8_UNORM;
			ao2->isSampled = true;

			AttachmentSchema* depth = graphSchema->attachment("depth");
			depth->format = context->device->swapchain->depthFormat;
			depth->isDepth = true;
			depth->isSampled = true;

			AttachmentSchema* light = graphSchema->attachment("light");
			light->format = VK_FORMAT_R8G8B8A8_SRGB;
			light->isSampled = true;

			AttachmentSchema* swap = graphSchema->attachment("swap");
			swap->format = context->device->swapchain->screenFormat;
			swap->isSwapchain = true;

			// PASSES

			PassSchema* main = graphSchema->pass("main");
			main->layerMask = 1 << 0;

			PassSchema* ssao = graphSchema->blitPass("ssao", { "ssao/ssao.frag", {} });
			ssao->layerMask = 0;

			PassSchema* ssao_blur_x = graphSchema->blitPass("ssao_blur_x", { "bloom/blur.frag", {} });
			ssao_blur_x->layerMask = 0;
			PassSchema* ssao_blur_y = graphSchema->blitPass("ssao_blur_y", { "bloom/blur.frag", {} });
			ssao_blur_y->layerMask = 0;

			PassSchema* skybox = graphSchema->pass("skybox");
			skybox->layerMask = 1 << 1;

			//PassSchema* merge = graphSchema->blitPass("merge", { "pbr/pbr_merge.frag", {} });
			//merge->layerMask = 0;

			PassSchema* lights = graphSchema->pass("lights");
			lights->materialOverride = true;
			VulkanMaterialInfo lightMatInfo{};
			lightMatInfo.rasterizer.cullMode = VK_CULL_MODE_FRONT_BIT;
			lightMatInfo.shaderStages = {
				{"pbr/pbr_light.vert", {}},
				{"pbr/pbr_light.frag", {}}
			};
			lights->overrideMaterialInfo = lightMatInfo;
			lights->layerMask = 1 << 3;

			PassSchema* blit = graphSchema->blitPass("blit", { "blit/blit.frag",{} });
			blit->layerMask = 1 << 2;

			// ATTACHMENT ACCESSES

			main->write(0, albedo, PassWriteOptions{
				.clear = true,
				.colorClearValue = { 0.0f, 0.0f, 0.0f, 0.0f }
				});
			main->write(1, emissive, PassWriteOptions{});
			main->write(2, material, PassWriteOptions{});
			main->write(3, normal, PassWriteOptions{});
			main->write(4, position, PassWriteOptions{});
			main->write(5, ao, PassWriteOptions{ .colorClearValue = {1.0} });
			main->write(6, depth, PassWriteOptions{});

			ssao->read(0, position, PassReadOptions{});
			ssao->read(1, normal, PassReadOptions{});
			ssao->read(2, depth, PassReadOptions{});
			ssao->read(3, ao, PassReadOptions{});
			ssao->write(0, ao2, PassWriteOptions{});

			ssao_blur_x->read(0, ao2, PassReadOptions{});
			ssao_blur_x->write(0, ao, PassWriteOptions{});

			ssao_blur_y->read(0, ao, PassReadOptions{});
			ssao_blur_y->write(0, ao2, PassWriteOptions{});

			skybox->write(0, light, PassWriteOptions{ .clear = false, .sampled = false });

			//merge->read(0, albedo);
			//merge->read(1, emissive);
			//merge->read(2, material);
			//merge->read(3, normal);
			//merge->read(4, position);
			//merge->read(5, ao2);
			//merge->write(0, light, PassWriteOptions{ .clear = false, .sampled = false });

			lights->read(0, albedo);
			lights->read(1, emissive);
			lights->read(2, material);
			lights->read(3, normal);
			lights->read(4, position);
			lights->read(5, ao2);
			lights->write(0, light, PassWriteOptions{ .clear = false });

			blit->read(0, light);
			blit->write(0, swap, PassWriteOptions{ .sampled = false });
		}

		graph = new RenderGraph(graphSchema, scene, context->device->swapchain->swapChainLength);
		graph->createLayouts();

		mainPass = graph->getPass("main");
		Pass* merge = graph->getPass("merge");
		Pass* lights = graph->getPass("lights");

		initLights(scene, lights);

		gui = new VulkanImguiInstance(context, graph->getPass("blit")->pass);
		gui->layer = 1 << 2;
		scene->addObject(gui);


		ssaoUniform = new VulkanUniform(context->device, sizeof(SSAOParams));
		for (VulkanDescriptorSet* set : graph->getPass("ssao")->materialInstance->descriptorSets) {
			set->write(0, this->ssaoUniform);
		}
		ssaoHorizBlurUniform = new VulkanUniform(context->device, sizeof(BlurUniform));
		ssaoVertiBlurUniform = new VulkanUniform(context->device, sizeof(BlurUniform));
		for (VulkanDescriptorSet* set : graph->getPass("ssao_blur_x")->materialInstance->descriptorSets) {
			set->write(0, ssaoHorizBlurUniform);
		}
		for (VulkanDescriptorSet* set : graph->getPass("ssao_blur_y")->materialInstance->descriptorSets) {
			set->write(0, ssaoVertiBlurUniform);
		}

		Skybox* skybox = new Skybox("res/textures/cubemap_day/", scene, graph->getPass("skybox"));
		skybox->layer = 1 << 1;
		scene->addObject(skybox);

		brdf = generateBRDFLUT(context->device);
		irradiancemap = generateIrradianceCube(context->device, skybox->skybox);
		specmap = generatePrefilteredCube(context->device, skybox->skybox);

		//for (VulkanDescriptorSet* set : merge->materialInstance->descriptorSets) {
		//	set->write(0, specmap);
		//	set->write(1, irradiancemap);
		//	set->write(2, brdf);
		//}

		for (VulkanDescriptorSet* set : lights->materialInstance->descriptorSets) {
			set->write(0, specmap);
			set->write(1, irradiancemap);
			set->write(2, brdf);
		}

		gltf = new VulkanGltfModel("res/models/DamagedHelmet.glb", scene, mainPass);
		gltf->localTransform *= glm::scale(glm::vec3(0.5f));
		gltf->localTransform *= glm::translate(glm::vec3(0, 0.8, 0));
		scene->addObject(gltf);

		scene->addObject(new VulkanGltfModel("res/models/Sponza/glTF/Sponza.gltf", scene, mainPass));

		cmdBufs.resize(context->device->swapchain->swapChainLength);
		for (uint32_t i = 0; i < cmdBufs.size(); i++) {
			cmdBufs[i] = context->device->createCommandBuffer();
		}

		buildSwapchainDependants();
	}
	float lightZenith{};
	float lightAzimuth{};
	int k = 0;
	void updateUniforms(uint32_t i) {
		flycam->update();

		static auto startTime = std::chrono::high_resolution_clock::now();
		auto currentTime = std::chrono::high_resolution_clock::now();
		float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

		float n = 0.01f;
		float f = 100.0f;
		VkExtent2D swapchainExtent = context->device->swapchain->swapChainExtent;

		SceneGlobalUniform global{};
		float tanFOVx = glm::tan(flycam->getFOV() / 2.0f);
		float screenSizeX = 2 * n * tanFOVx;
		float screenSizeY = 2 * n * tanFOVx * swapchainExtent.height / swapchainExtent.width;
		glm::vec3 pixelSize = glm::vec3(screenSizeX / swapchainExtent.width, screenSizeY / swapchainExtent.height, 0.0);
		glm::vec3 jitter{};
		glm::vec2 rand = glm::sphericalRand(1.0);
		jitter += pixelSize * glm::vec3(rand.x, rand.y, 0);
		glm::mat4 transform = flycam->getTransform();
		global.view = glm::translate(jitter)* glm::inverse(transform);
		global.proj = flycam->getProjMatrix(static_cast<float>(swapchainExtent.width), static_cast<float>(swapchainExtent.height), n, f);
		global.camPos = transform * glm::vec4(0.0, 0.0, 0.0, 1.0);
		global.screenRes = { swapchainExtent.width, swapchainExtent.height };
		global.time = time;

		global.directionalLight = glm::vec4(1.0, 0.0, 0.0, 0.0);
		global.directionalLight = glm::rotate(glm::mat4(1.0f), -lightZenith, glm::vec3(0.0, 0.0, 1.0)) * global.directionalLight;
		global.directionalLight = glm::rotate(glm::mat4(1.0f), lightAzimuth, glm::vec3(0.0, 1.0, 0.0)) * global.directionalLight;

		scene->updateUniforms(i, 0, &global);
		ssao.nearPlane = n;
		ssao.farPlane = f;
		ssaoUniform->write(&ssao);
		ssaoHorizBlurUniform->write(&ssaoHorizBlurParams);
		ssaoVertiBlurUniform->write(&ssaoVertiBlurParams);
	}

	float ssaoRadius = 0.1f;
	bool aoEnabled = true;
	int timesDrawn = 0;
	VkCommandBuffer draw(uint32_t i)
	{
		{
			ZoneScopedN("Uniforms Updating");

			updateUniforms(i);
		}
		
		{
			ZoneScopedN("ImGUI Processing");
			gui->NextFrame();
			ImGui::Begin("AO Demo");
			ImGui::SliderFloat("Light Zenith", &lightZenith, 0.0f, 3.14f);
			ImGui::SliderFloat("Light Azimuth", &lightAzimuth, 0.0f, 6.28f);
			ImGui::SliderFloat("AO Intensity", &ssaoRadius, 0, 1);
			ImGui::Checkbox("Enable AO", &aoEnabled);
			if (aoEnabled)
				ssao.intensity = ssaoRadius;
			else
				ssao.intensity = 0.0;
			ImGui::SliderInt("Blur Size", &ssaoHorizBlurParams.pixelStep, 0.f, 8.f);
			ssaoVertiBlurParams.pixelStep = ssaoHorizBlurParams.pixelStep;
			ImGui::End();
			gui->InternalRender();
		}

		if (timesDrawn >= cmdBufs.size()) {
			ZoneScopedN("Resetting Command Buffer");
			vkResetCommandBuffer(cmdBufs[i], 0);
		}
		else
			timesDrawn++;

		{
			ZoneScopedN("Begin Command Buffer");
			cmdBufs[i] = context->device->beginCommandBuffer();
		}
		{
			ZoneScopedN("Graph Recording");
			graph->render(cmdBufs[i], i);
		}
		{
			ZoneScopedN("End Command Buffer");
			vkEndCommandBuffer(cmdBufs[i]);
		}

		return cmdBufs[i];
	}
	void buildSwapchainDependants()
	{
		graph->createInstances();
	}
	void destroySwapchainDependents()
	{
		graph->destroyInstances();
	}
	void preCleanup()
	{
		destroySwapchainDependents();

		delete ssaoUniform;
		delete ssaoHorizBlurUniform;
		delete ssaoVertiBlurUniform;

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
