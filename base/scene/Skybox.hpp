#pragma once

#include <string>

#include <vulkan/vulkan.h>

#include "Object.h"
#include "Scene.h"
#include "../VulkanMaterial.h"
#include "../VulkanTexture.h"
#include "../rendergraph/RenderGraph.h"

namespace vku {
	struct Skybox : Object {
		VulkanTexture *skybox;
		VulkanMaterial *mat;
		VulkanMaterialInstance *matInst;
		VulkanMeshBuffer* boxMeshBuf;

		Skybox(const std::string& cubemapPath, Scene *scene, Pass *pass) {

			boxMeshBuf = new VulkanMeshBuffer(scene->device, vku::box);

			// load skybox texture
			skybox = new VulkanTexture;
			skybox->image = new VulkanImage(scene->device, {
				cubemapPath + "/posx.jpg",
				cubemapPath + "/negx.jpg",
				cubemapPath + "/posy.jpg",
				cubemapPath + "/negy.jpg",
				cubemapPath + "/posz.jpg",
				cubemapPath + "/negz.jpg"
				});
			VulkanImageViewInfo viewInfo{};
			skybox->image->writeImageViewInfo(&viewInfo);
			viewInfo.imageViewType = VK_IMAGE_VIEW_TYPE_CUBE;
			skybox->view = new VulkanImageView(scene->device, viewInfo);
			skybox->sampler = new VulkanSampler(scene->device, {});

			// make material
			// skyboxes don't care about depth testing / writing
			VulkanMaterialInfo matInfo{};
			matInfo.depthStencil.depthWriteEnable = false;
			matInfo.depthStencil.depthTestEnable = false;
			matInfo.rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
			matInfo.shaderStages.push_back({ "skybox/skybox.frag", {} });
			matInfo.shaderStages.push_back({ "skybox/skybox.vert", {} });
			mat = new VulkanMaterial(&matInfo, scene, pass);
			matInst = new VulkanMaterialInstance(mat);
			for (VulkanDescriptorSet* set : matInst->descriptorSets) { set->write(0, skybox); }
		}

		virtual void render(VkCommandBuffer cmdBuf, uint32_t swapIdx, bool noMaterial) {
			if (!noMaterial) {
				mat->bind(cmdBuf);
				matInst->bind(cmdBuf, swapIdx);
			}
			boxMeshBuf->draw(cmdBuf);
		}
		virtual glm::mat4 getAABBTransform() {
			return glm::mat4(0.0);
		}

		~Skybox() {
			delete boxMeshBuf;
			delete matInst;
			delete mat;
			delete skybox;
		}
	};
}