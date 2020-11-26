# pragma once

#include <vulkan/vulkan.h>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"

#include  "../VulkanContext.h"
#include  "../VulkanDevice.h"
#include  "../VulkanSwapchain.h"
#include "../scene/Object.h"

namespace vku {
	struct VulkanImguiInstance : Object {
		VulkanImguiInstance(VulkanContext *context, VkRenderPass pass) {
			IMGUI_CHECKVERSION();
			ImGui::CreateContext();
			ImGuiIO& io = ImGui::GetIO();
			io.IniFilename = NULL;
			ImGui::StyleColorsDark();

			ImGui_ImplGlfw_InitForVulkan(context->windowHandle, true);
			ImGui_ImplVulkan_InitInfo init_info = {};
			init_info.Instance = context->instance;
			init_info.PhysicalDevice = context->device->physicalDevice;
			init_info.Device = *context->device;
			init_info.QueueFamily = context->device->supportInfo.graphicsFamily.value();
			init_info.Queue = context->device->graphicsQueue;
			init_info.PipelineCache = VK_NULL_HANDLE;
			init_info.DescriptorPool = context->device->descriptorPool;
			init_info.Allocator = nullptr;
			init_info.MinImageCount = 2;
			init_info.ImageCount = context->device->swapchain->swapChainLength;
			init_info.CheckVkResultFn = nullptr;
			ImGui_ImplVulkan_Init(&init_info, pass);

			VkCommandBuffer cb = context->device->beginCommandBuffer();
			ImGui_ImplVulkan_CreateFontsTexture(cb);
			context->device->submitCommandBuffer(cb, context->device->graphicsQueue);

			ImGui::SetNextWindowPos({ 0,0 });
		}

		void NextFrame() {
			ImGui_ImplVulkan_NewFrame();
			ImGui_ImplGlfw_NewFrame();
			ImGui::NewFrame();
		}

		void InternalRender() {
			ImGui::Render();
		}

		virtual glm::mat4 getAABBTransform() {
			return glm::mat4(0.0);
		};
		virtual void render(VkCommandBuffer cmdBuf, uint32_t swapIdx, bool noMaterial) {
			ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmdBuf);
		}

		~VulkanImguiInstance() {
			ImGui_ImplVulkan_Shutdown();
			ImGui_ImplGlfw_Shutdown();
			ImGui::DestroyContext();
		}
	};
}