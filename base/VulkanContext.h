#pragma once

#include <string>
#include <vector>
#include <functional>

#include <vulkan/vulkan.h>
#include <glfw3/glfw3.h>

namespace vku {
	struct VulkanDevice;

	struct VulkanContextInfo {
		std::string title = "vkmerc application";
		uint32_t width = 500, height = 500;
		std::function<void(GLFWwindow*, int, int)> resizeCallback;
		bool debugEnabled = false;
		bool haltOnValidationError = true;

		std::vector<const char*> validationLayers = { "VK_LAYER_KHRONOS_validation" };
		std::vector<const char*> deviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
	};

	/* Responsible for creating a window with a VkInstance, surface and VkDevice attached to it. */
	struct VulkanContext {
		bool debugEnabled;
		bool haltOnValidationError;
		std::function<void(GLFWwindow*, int, int)> resizeCallback;

		GLFWwindow* windowHandle;

		VkInstance instance;
		VkDebugUtilsMessengerEXT debugMessenger;
		VkSurfaceKHR surface;

		VulkanDevice* device;

		VulkanContext(VulkanContextInfo info);
		~VulkanContext();

		static void VulkanContext::framebufferResizeCallback(GLFWwindow* window, int width, int height);
		void createWindow(VulkanContextInfo& info);
		void destroyWindow();

		void createVulkanInstance(VulkanContextInfo& info);
		void destroyVulkanInstance();
	};
}