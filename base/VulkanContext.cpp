#include "VulkanContext.h"

#include <iostream>
#include <stdexcept>

#include "VulkanDevice.h"

namespace vku {
	/* Functions for setting up the validation layer. */

	static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData) {
		if (messageSeverity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT || messageSeverity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
			std::cerr << pCallbackData->pMessage << std::endl;
			VulkanContext* context = static_cast<VulkanContext*>(pUserData);
			std::cerr << "Halting on validation error." << std::endl;
			std::cin.get();
		}

		return VK_TRUE;
	}
	bool checkValidationLayerSupport(const std::vector<const char*>& validationLayers) {
		uint32_t layerCount;
		vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

		std::vector<VkLayerProperties> availableLayers(layerCount);
		vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

		for (const char* layerName : validationLayers) {
			bool layerFound = false;

			for (const auto& layerProperties : availableLayers) {
				if (strcmp(layerName, layerProperties.layerName) == 0) {
					layerFound = true;
					break;
				}
			}

			if (!layerFound) {
				return false;
			}
		}

		return true;
	}
	void getDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo, VulkanContext* vkContext) {
		createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		createInfo.pfnUserCallback = debugCallback;
		createInfo.pUserData = vkContext;
	}
	VkDebugUtilsMessengerEXT setupDebugMessenger(VulkanContext* vkContext) {
		VkDebugUtilsMessengerCreateInfoEXT createInfo{};
		getDebugMessengerCreateInfo(createInfo, vkContext);

		VkResult result;
		auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(vkContext->instance, "vkCreateDebugUtilsMessengerEXT");
		if (func != nullptr) {
			VkDebugUtilsMessengerEXT debugMessenger;
			func(vkContext->instance, &createInfo, nullptr, &debugMessenger);
			return debugMessenger;
		}
		else {
			throw std::runtime_error("Could not set up debug messenger.");
		}
	}
	void destroyDebugMessenger(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger) {
		auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
		if (func != nullptr) {
			func(instance, debugMessenger, nullptr);
		}
	}

	std::vector<const char*> getRequiredGLFWExtensions() {
		uint32_t glfwExtensionCount = 0;
		const char** glfwExtensions;
		glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

		std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

		return extensions;
	}

	VulkanContext::VulkanContext(VulkanContextInfo info) {
		this->resizeCallback = info.resizeCallback;
		createWindow(info);
		createVulkanInstance(info);
		VkCommandBuffer tracySetupCommandBuf = device->createCommandBuffer(true);
		this->tracyContext = TracyVkContextCalibrated(device->physicalDevice, device->handle, device->graphicsQueue, tracySetupCommandBuf,
			(PFN_vkGetPhysicalDeviceCalibrateableTimeDomainsEXT)vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceCalibrateableTimeDomainsEXT"),
			(PFN_vkGetCalibratedTimestampsEXT)vkGetInstanceProcAddr(instance, "vkGetCalibratedTimestampsEXT"));
		vkFreeCommandBuffers(*device, device->commandPool, 1, &tracySetupCommandBuf);
	}

	VulkanContext::~VulkanContext() {
		TracyVkDestroy(this->tracyContext);
		destroyVulkanInstance();
		destroyWindow();
	}

	void VulkanContext::framebufferResizeCallback(GLFWwindow* window, int width, int height) {
		VulkanContext* vkContext = static_cast<VulkanContext*>(glfwGetWindowUserPointer(window));
		vkContext->resizeCallback(window, width, height);
	}

	void VulkanContext::createWindow(VulkanContextInfo& info)
	{
		glfwInit();
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
		GLFWmonitor* monitor = nullptr;
		uint32_t width = info.width, height = info.height;
		if (info.fullscreen) {
			monitor = glfwGetPrimaryMonitor();
			const GLFWvidmode* mode = glfwGetVideoMode(monitor);
			width = mode->width;
			height = mode->height;
		}
		this->windowHandle = glfwCreateWindow(width, height, info.title.c_str(), monitor, nullptr);
		glfwSetWindowUserPointer(windowHandle, this);
		glfwSetFramebufferSizeCallback(windowHandle, framebufferResizeCallback);
	}

	void VulkanContext::destroyWindow()
	{
		glfwDestroyWindow(windowHandle);
		glfwTerminate();
	}

	void VulkanContext::createVulkanInstance(VulkanContextInfo& info)
	{
		// Create VkInstance and hook the validation layer in
#ifdef VK_VALIDATION
		if (!checkValidationLayerSupport(info.validationLayers)) {
			throw std::runtime_error("Validation layers requested, but not available!");
		}
		else {
			std::cout << "Will attempt to enable validation layer" << std::endl;
		}
#endif

		// basic information about the instance
		VkApplicationInfo appInfo{};
		appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		appInfo.pApplicationName = info.title.c_str();
		appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.pEngineName = "vkmerc";
		appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.apiVersion = VK_API_VERSION_1_2;

		// creation definition begins, pass app info
		VkInstanceCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		createInfo.pApplicationInfo = &appInfo;

		// find required extensions from GLFW
		auto extensions = getRequiredGLFWExtensions();
#ifdef VK_VALIDATION
			extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif
		createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
		createInfo.ppEnabledExtensionNames = extensions.data();

		// enable validation layers if need be
		VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo;
#ifdef VK_VALIDATION
		createInfo.enabledLayerCount = static_cast<uint32_t>(info.validationLayers.size());
		createInfo.ppEnabledLayerNames = info.validationLayers.data();

		getDebugMessengerCreateInfo(debugCreateInfo, this);
		createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
#else
		createInfo.enabledLayerCount = 0;
		createInfo.pNext = nullptr;
#endif


		std::cout << "Extensions:\n";
		for (const auto& ext : extensions) {
			std::cout << '\t' << ext << '\n';
		}

		VkResult result = vkCreateInstance(&createInfo, nullptr, &this->instance);
		if (result != VK_SUCCESS) {
			throw std::runtime_error("Failed to create Vulkan instance!");
		}

#ifdef VK_VALIDATION
		this->debugMessenger = setupDebugMessenger(this);
#endif

		// Create Surface
		if (glfwCreateWindowSurface(instance, windowHandle, nullptr, &surface) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create window surface!");
		}

		// create device
		VulkanDeviceInfo deviceInfo{};
		deviceInfo.context = this;
		this->device = new VulkanDevice(deviceInfo);
	}

	void VulkanContext::destroyVulkanInstance()
	{
		delete this->device;

		vkDestroySurfaceKHR(instance, surface, nullptr);
		destroyDebugMessenger(instance, debugMessenger);
		vkDestroyInstance(instance, nullptr);
	}
}