#define SHADOWMAP_CASCADE_SIZE 2048

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

using namespace std;
using namespace vku;

struct CascadesUniform {
	glm::mat4 cascades[4];
	glm::vec4 data[4] = {
		{0.0f, 0.00003f, 0, 0},
		{0.0f, 0.00014f, 0, 0},
		{0.0f, 0.00037f, 0, 0},
		{}
	};
};

class Engine : public BaseEngine {
public:
	Engine() {
		this->windowTitle = "Cascaded Shadowmap Demo";
		//this->fullscreen = true;
		this->width = 900;
		this->height = 900;
	}

	FlyCam* flycam;
	Scene* scene;

	RenderGraphSchema* graphSchema;
	RenderGraph* graph;
	Pass* shadowmapPass;
	Pass* mainPass;
	Pass* blitPass;

	VulkanImguiInstance* guiInstance;

	VulkanMaterial* shadowpassMat;
	VulkanMaterialInstance* shadowpassMatInst;

	VulkanMeshBuffer* blitMeshBuf;
	VulkanMaterial* blitMat;
	VulkanMaterialInstance* blitMatInst;

	VulkanObjModel* city;
	VulkanObjModel* platform;
	VulkanGltfModel* gltf;
	VulkanTexture* brdf;
	VulkanTexture* irradiancemap;
	VulkanTexture* specmap;

	VulkanMaterial* cascadeMat;
	VulkanMaterialInstance* cascadeMatInst;
	VulkanMaterial* visualizerMat;
	VulkanMaterialInstance* visualizerMatInstance;

	VulkanTexture* skybox;
	VulkanMeshBuffer* boxMeshBuf;
	VulkanMaterial* mat;
	VulkanMaterialInstance* matInst;

	std::vector<VkCommandBuffer> cmdBufs;

	// Inherited via BaseEngine
	void postInit()
	{
		boxMeshBuf = new VulkanMeshBuffer(context->device, vku::box);
		blitMeshBuf = new VulkanMeshBuffer(context->device, vku::blit);

		SceneInfo sInfo{};
		sInfo.uniformAllocSizes.push_back(sizeof(CascadesUniform));
		scene = new Scene(context->device, sInfo);

		// load skybox texture
		skybox = new VulkanTexture;
		skybox->image = new VulkanImage(context->device, {
			"res/textures/cubemap_night/posx.jpg",
			"res/textures/cubemap_night/negx.jpg",
			"res/textures/cubemap_night/posy.jpg",
			"res/textures/cubemap_night/negy.jpg",
			"res/textures/cubemap_night/posz.jpg",
			"res/textures/cubemap_night/negz.jpg"
			});
		VulkanImageViewInfo viewInfo{};
		skybox->image->writeImageViewInfo(&viewInfo);
		viewInfo.imageViewType = VK_IMAGE_VIEW_TYPE_CUBE;
		skybox->view = new VulkanImageView(context->device, viewInfo);
		skybox->sampler = new VulkanSampler(context->device, {});

		flycam = new FlyCam(context->windowHandle);

		graphSchema = new RenderGraphSchema();
		{
			VkSampleCountFlagBits msaaCount = VK_SAMPLE_COUNT_8_BIT;

			PassSchema* cascade0 = graphSchema->pass("shadowpass", [&](uint32_t i, const VkCommandBuffer& cb) {
				shadowpassMatInst->bind(cb, i);
				shadowpassMatInst->material->bind(cb);

				glm::uint cascadeIndex = 0;

				VkViewport viewport{};
				viewport.minDepth = 0.0;
				viewport.maxDepth = 1.0;
				viewport.width = viewport.height = SHADOWMAP_CASCADE_SIZE;
				for (int x = 0; x <= 1; x++) {
					viewport.x = x * SHADOWMAP_CASCADE_SIZE;
					for (int y = 0; y <= 1; y++) {
						if (cascadeIndex <= 2) {
							viewport.y = y * SHADOWMAP_CASCADE_SIZE;
							vkCmdSetViewport(cb, 0, 1, &viewport);
							vkCmdPushConstants(cb, mat->pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT, sizeof(glm::mat4), sizeof(glm::uint), &cascadeIndex);
							scene->render(cb, i, true);
							cascadeIndex++;
						}
					}
				}
				});

			PassSchema* main = graphSchema->pass("main", [&](uint32_t i, const VkCommandBuffer& cb) {
				if (enableDebugView) {
					cascadeMatInst->material->bind(cb);
					cascadeMatInst->bind(cb, i);

					scene->render(cb, i, true);

					visualizerMat->bind(cb);
					visualizerMatInstance->bind(cb, i);
					const glm::vec4 red(1.0, 1.0, 1.0, 1.0f), blue(0.0, 1.0, 0.0, 0.3);
					glm::mat4 frust = glm::inverse(cascades.cascades[currentFrustum]);
					if (showViewFrust) {
						vkCmdPushConstants(cb, visualizerMat->pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &testFrusta[currentFrustum]);
						vkCmdPushConstants(cb, visualizerMat->pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT, sizeof(glm::mat4), sizeof(glm::vec4), &red);
						int mode = true;
						vkCmdPushConstants(cb, visualizerMat->pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT, sizeof(glm::mat4) + sizeof(glm::vec4), sizeof(int), &mode);
						boxMeshBuf->draw(cb);
					}
					if (showCascFrust) {
						vkCmdPushConstants(cb, visualizerMat->pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &frust);
						vkCmdPushConstants(cb, visualizerMat->pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT, sizeof(glm::mat4), sizeof(glm::vec4), &blue);
						int mode = false;
						vkCmdPushConstants(cb, visualizerMat->pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT, sizeof(glm::mat4) + sizeof(glm::vec4), sizeof(int), &mode);
						boxMeshBuf->draw(cb);
					}
				}
				else {
					matInst->bind(cb, i);
					matInst->material->bind(cb);
					boxMeshBuf->draw(cb);

					scene->render(cb, i, false);
				}
				});

			PassSchema* blit = graphSchema->pass("blit", [&](uint32_t i, const VkCommandBuffer& cb) {
				blitMatInst->bind(cb, i);
				blitMatInst->material->bind(cb);
				blitMeshBuf->draw(cb);

				guiInstance->Render(cb);
				});

			main->samples = msaaCount;

			AttachmentSchema* edge = graphSchema->attachment("color", main, { blit });
			edge->format = context->device->swapchain->screenFormat;
			edge->isSampled = true;
			edge->samples = msaaCount;
			edge->resolve = true;

			AttachmentSchema* cascadeAtt = graphSchema->attachment("cascades", cascade0, { main, blit });
			cascadeAtt->format = context->device->swapchain->depthFormat;
			cascadeAtt->isDepth = true;
			cascadeAtt->isSampled = true;
			cascadeAtt->width = cascadeAtt->height = SHADOWMAP_CASCADE_SIZE * 2; // 2x2 grid of cascades. max 4.
			VulkanSamplerInfo samplerCI{};
			samplerCI.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
			samplerCI.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
			samplerCI.minFilter = VK_FILTER_LINEAR;
			samplerCI.magFilter = VK_FILTER_LINEAR;
			samplerCI.compareEnable = VK_TRUE;
			samplerCI.compareOp = VK_COMPARE_OP_LESS;
			cascadeAtt->samplerInfo = samplerCI;

			AttachmentSchema* depth = graphSchema->attachment("depth", main, {});
			depth->format = context->device->swapchain->depthFormat;
			depth->isDepth = true;
			depth->samples = msaaCount;
			depth->isTransient = true;

			AttachmentSchema* swap = graphSchema->attachment("swap", blit, {});
			swap->format = context->device->swapchain->screenFormat;
			swap->isSwapchain = true;
		}

		graph = new RenderGraph(graphSchema, scene, context->device->swapchain->swapChainLength);
		graph->createLayouts();

		shadowmapPass = graph->getPass("shadowpass");
		mainPass = graph->getPass("main");
		blitPass = graph->getPass("blit");

		// generate PBR stuff
		brdf = generateBRDFLUT(context->device);
		irradiancemap = generateIrradianceCube(context->device, skybox);
		specmap = generatePrefilteredCube(context->device, skybox);

		std::vector<ShaderMacro> pbrMacros = { { "AMBIENT_FACTOR", "0.3" }, {"USE_CASCADES", ""}, {"SUN_STRENGTH", "0.5" } };
		city = new VulkanObjModel("res/models/city.obj", scene, mainPass, specmap, irradiancemap, brdf, pbrMacros);
		city->localTransform *= glm::translate(glm::vec3(0, -2, 0));
		scene->addObject(city);

		platform = new VulkanObjModel("res/models/stand.obj", scene, mainPass, specmap, irradiancemap, brdf, pbrMacros);
		platform->localTransform *= glm::translate(glm::vec3(0, -2, 0));
		scene->addObject(platform);

		gltf = new VulkanGltfModel("res/models/DamagedHelmet.glb", scene, mainPass, brdf, irradiancemap, specmap, pbrMacros);
		gltf->localTransform *= glm::translate(glm::vec3(0.0f, 1.0f, 0.0f));
		scene->addObject(gltf);

		// skyboxes don't care about depth testing / writing
		VulkanMaterialInfo matInfo(context->device);
		matInfo.depthStencil.depthWriteEnable = false;
		matInfo.depthStencil.depthTestEnable = false;
		matInfo.rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
		matInfo.shaderStages.push_back({ "res/shaders/skybox/skybox.frag", VK_SHADER_STAGE_FRAGMENT_BIT, {} });
		matInfo.shaderStages.push_back({ "res/shaders/skybox/skybox.vert", VK_SHADER_STAGE_VERTEX_BIT, {} });
		mat = new VulkanMaterial(&matInfo, scene, mainPass);
		matInst = new VulkanMaterialInstance(mat);
		for (VulkanDescriptorSet* set : matInst->descriptorSets) { set->write(0, skybox); }

		// blit pass
		VulkanMaterialInfo blitMatInfo(context->device);
		blitMatInfo.shaderStages.push_back({ "res/shaders/misc/shadowpass_debug_blit.frag", VK_SHADER_STAGE_FRAGMENT_BIT, {} });
		blitMatInfo.shaderStages.push_back({ "res/shaders/blit/blit.vert", VK_SHADER_STAGE_VERTEX_BIT, {} });
		blitMat = new VulkanMaterial(&blitMatInfo, scene, blitPass);
		blitMatInst = new VulkanMaterialInstance(blitMat);

		// shadowpass material
		VulkanMaterialInfo shadowMatInfo(context->device);
		shadowMatInfo.depthStencil.depthTestEnable = true;
		shadowMatInfo.shaderStages.push_back({ "res/shaders/shadowpass/shadowpass.vert", VK_SHADER_STAGE_VERTEX_BIT, {} });
		shadowpassMat = new VulkanMaterial(&shadowMatInfo, scene, shadowmapPass);
		shadowpassMatInst = new VulkanMaterialInstance(shadowpassMat);

		// debug frustum visualizer
		VulkanMaterialInfo visualizerMatInfo(context->device);
		visualizerMatInfo.rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
		visualizerMatInfo.colorBlendAttachment.blendEnable = true;
		visualizerMatInfo.colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
		visualizerMatInfo.colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
		visualizerMatInfo.rasterizer.cullMode = VK_CULL_MODE_NONE;
		visualizerMatInfo.shaderStages.push_back({ "res/shaders/misc/frustum_debugger.frag", VK_SHADER_STAGE_FRAGMENT_BIT, {} });
		visualizerMatInfo.shaderStages.push_back({ "res/shaders/misc/frustum_debugger.vert", VK_SHADER_STAGE_VERTEX_BIT, {} });
		visualizerMat = new VulkanMaterial(&visualizerMatInfo, scene, mainPass);
		visualizerMatInstance = new VulkanMaterialInstance(visualizerMat);

		// visualize where cascades start and end
		VulkanMaterialInfo cascadeDebugMatInfo(context->device);
		cascadeDebugMatInfo.shaderStages.push_back({ "res/shaders/misc/cascade_debugger.frag", VK_SHADER_STAGE_FRAGMENT_BIT, {} });
		cascadeDebugMatInfo.shaderStages.push_back({ "res/shaders/pbr/pbr.vert", VK_SHADER_STAGE_VERTEX_BIT, {} });
		cascadeMat = new VulkanMaterial(&cascadeDebugMatInfo, scene, mainPass);
		cascadeMatInst = new VulkanMaterialInstance(cascadeMat);

		cmdBufs.resize(context->device->swapchain->swapChainLength);
		for (uint32_t i = 0; i < cmdBufs.size(); i++) {
			cmdBufs[i] = context->device->createCommandBuffer();
		}

		guiInstance = new VulkanImguiInstance(context, blitPass->pass);

		buildSwapchainDependants();
	}

	SceneGlobalUniform global{};
	CascadesUniform cascades{};
	glm::mat4 testFrustum, testCascadeFrustum;
	glm::mat4 testFrusta[3];
	float lightZenith = 3.14f / 2.0f, lightAzimuth = 3.14 / 2.0f;
	float slopeBias = 0.0000060000f, baseBias = 0.0000020000f;
	bool keepCascadesSquare = true, roundCascadesToPixel = true;

	bool frozen = false;
	bool showViewFrust = false;
	bool showCascFrust = false;
	bool enableDebugView = false;
	int currentFrustum = 0;

	float n = .01f;
	float f = 100.0f;
	float split1 = 2.0f/100.0f, split2 = 15.0f/100.0f;

	glm::mat4 camTransform;
	float savedCamFOV;

	void updateUniforms(uint32_t i) {
		flycam->update();

		static auto startTime = std::chrono::high_resolution_clock::now();
		auto currentTime = std::chrono::high_resolution_clock::now();
		float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

		// camera numbers
		VkExtent2D swapchainExtent = context->device->swapchain->swapChainExtent;
		float width = static_cast<float>(swapchainExtent.width), height = static_cast<float>(swapchainExtent.height);

		glm::mat4 transform = flycam->getTransform();
		global.view = glm::inverse(transform);
		global.proj = flycam->getProjMatrix(width, height, 0.05f, 500.0);
		global.camPos = transform * glm::vec4(0.0, 0.0, 0.0, 1.0);
		global.screenRes = { swapchainExtent.width, swapchainExtent.height };
		global.time = time;

		glm::mat4 sceneAABB = scene->getAABBTransform();

		global.directionalLight = glm::vec4(1.0, 0.0, 0.0, 0.0);
		global.directionalLight = glm::rotate(glm::mat4(1.0f), -lightZenith, glm::vec3(0.0, 0.0, 1.0)) * global.directionalLight;
		global.directionalLight = glm::rotate(glm::mat4(1.0f), lightAzimuth, glm::vec3(0.0, 1.0, 0.0)) * global.directionalLight;

		if (!frozen) {
			camTransform = transform;
			savedCamFOV = flycam->getFOV();
		}

		float quarter = n + (f - n) * split1;
		float half = n + (f - n) * split2;

		testFrusta[0] = camTransform * glm::inverse(flycam->getProjMatrix(width, height, n, quarter, savedCamFOV));
		testFrusta[1] = camTransform * glm::inverse(flycam->getProjMatrix(width, height, quarter, half, savedCamFOV));
		testFrusta[2] = camTransform * glm::inverse(flycam->getProjMatrix(width, height, half, f, savedCamFOV));

		float size;
		cascades.cascades[0] = fitLightProjMatToCameraFrustum(testFrusta[0], global.directionalLight, SHADOWMAP_CASCADE_SIZE, sceneAABB, &size, keepCascadesSquare, roundCascadesToPixel);
		cascades.data[0][1] = slopeBias * size;
		cascades.data[0][0] = baseBias * size;
		cascades.cascades[1] = fitLightProjMatToCameraFrustum(testFrusta[1], global.directionalLight, SHADOWMAP_CASCADE_SIZE, sceneAABB, &size, keepCascadesSquare, roundCascadesToPixel);
		cascades.data[1][1] = slopeBias * size;
		cascades.data[1][0] = baseBias * size;
		cascades.cascades[2] = fitLightProjMatToCameraFrustum(testFrusta[2], global.directionalLight, SHADOWMAP_CASCADE_SIZE, sceneAABB, &size, keepCascadesSquare, roundCascadesToPixel);
		cascades.data[2][1] = slopeBias * size;
		cascades.data[2][0] = baseBias * size;

		scene->updateUniforms(i, 0, &global);
		scene->updateUniforms(i, 1, &cascades);
	}

	int timesDrawn = 0;
	VkCommandBuffer draw(uint32_t i)
	{
		updateUniforms(i);

		guiInstance->NextFrame();
		ImGui::Begin("CSM Demo");
		ImGui::Checkbox("Freeze View Frustum", &frozen);
		ImGui::SliderFloat("Light Zenith", &lightZenith, 0.0f, 3.14f);
		ImGui::SliderFloat("Light Azimuth", &lightAzimuth, 0.0f, 6.28f);

		ImGui::NewLine();
		
		ImGui::Text("Cascade Options", "");
		ImGui::InputFloat("Near Plane", &n, 0, 0, "%.10f");
		ImGui::InputFloat("Far Plane", &f, 0, 0, "%.10f");
		ImGui::SliderFloat("Cascade Split #1", &split1, 0.0f, 1.0f, "%.10f");
		ImGui::SliderFloat("Cascade Split #2", &split2, 0.0f, 1.0f, "%.10f");
		split2 = std::max(split2, split1);
		ImGui::InputFloat("Base Bias Factor", &baseBias, 0, 0, "%.10f");
		ImGui::InputFloat("Slope Bias Factor", &slopeBias, 0, 0, "%.10f");

		ImGui::NewLine();

		ImGui::Text("Cascade Options", "");
		ImGui::Checkbox("Keep Cascades Square", &keepCascadesSquare);
		ImGui::Checkbox("Round Cascades to Pixel Size", &roundCascadesToPixel);

		ImGui::NewLine();

		ImGui::Text("Frustum/Cascade Debugging", "");
		ImGui::Checkbox("Enable Debug View", &enableDebugView);
		ImGui::Checkbox("Show Cascade Frustum", &showCascFrust);
		ImGui::Checkbox("Show View Frustum", &showViewFrust);
		if (!frozen)
			showViewFrust = false;
		ImGui::SliderInt("Frustum Index", &currentFrustum, 0, 2);
		ImGui::End();
		guiInstance->InternalRender();

		if (timesDrawn >= cmdBufs.size())
			vkResetCommandBuffer(cmdBufs[i], 0);
		else
			timesDrawn++;
		cmdBufs[i] = context->device->beginCommandBuffer();
		graph->render(cmdBufs[i], i);
		vkEndCommandBuffer(cmdBufs[i]);

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
		delete guiInstance;

		destroySwapchainDependents();

		delete skybox;
		delete boxMeshBuf;
		delete matInst;
		delete mat;

		delete blitMeshBuf;
		delete blitMatInst;
		delete blitMat;

		delete shadowpassMat;
		delete shadowpassMatInst;

		delete cascadeMat;
		delete cascadeMatInst;
		delete visualizerMat;
		delete visualizerMatInstance;

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
