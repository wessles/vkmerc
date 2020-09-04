#pragma once

#include <vulkan/vulkan.h>

#include <string>
#include <stack>

#include <chrono>
#include <thread>

#include "Vku.hpp"

struct IResource {
	virtual void alloc(VkDevice device) = 0;
	virtual void free(VkDevice device) = 0;
};

/*
	A class which provides an empty Vulkan application.
	It takes care of boilerplate stuff so you can limit
	your concern to Demo-specific things. It handles:
	- Window initialization
	- Swapchain initialization
	- Stack-based resource management (IResource.alloc() / free())
	- Vulkan initialization
	- Resizing
	- Swapchain synchronization
	- Game-loop
*/
class Engine {
public:
	std::string windowName = "Untitled Demo";
	VkExtent2D windowSize{ 1280,720 };
	size_t framerateLimit = 60;
	bool debug = true;

	void run() {
		init();
		postInit();
		loop();
		preCleanup();
		cleanup();
	}

private:
	// this flag will be tripped on the window-resize event
	bool framebufferResized = false;

	unsigned long currentFrame = 0;

	// runtime-level (textures, pipelines, render passes)
	std::stack<IResource> resourceStack;
	// resize-affected (framebuffers, screenbuffers, swapchain, etc.)
	std::stack<IResource> resizeAffectedResourceStack;

	virtual void postInit() = 0;
	virtual VkCommandBuffer *draw(uint32_t swapIdx) = 0;
	virtual void destroySwapchainDependents() = 0;
	virtual void buildSwapchainDependants() = 0;
	virtual void preCleanup() = 0;

	void init() {
		const auto& resizeCallback = [this](GLFWwindow* window, int width, int height) { framebufferResized = true; };
		const auto& resizeCallbackFunc = std::function<void(GLFWwindow*, int, int)>(resizeCallback);
		vku::init(debug, windowName, windowSize, resizeCallbackFunc);
	}

	void initSwapchain() {
		vku::swap::createSwapChain();
	}

	void refreshSwapchain() {
		// very important for synchronization
		vkDeviceWaitIdle(state::device);

		destroySwapchainDependents();
		vku::swap::recreateSwapchain();
		buildSwapchainDependants();
	}

	void cleanup() {
		vku::swap::cleanupSwapchain();
		vku::cleanup();
	}

	void loop() {
		std::chrono::system_clock::time_point a = std::chrono::system_clock::now();
		std::chrono::system_clock::time_point b = std::chrono::system_clock::now();
		std::chrono::duration<double, std::milli> elapsed;
		double ms = 1000.0 / static_cast<double>(framerateLimit);

		while (!glfwWindowShouldClose(vku::state::windowHandle)) {
			// framerate limiting code
			a = std::chrono::system_clock::now();
			elapsed = a - b;
			if (elapsed.count() < ms)
			{
				std::chrono::duration<double, std::milli> delta_ms(ms - elapsed.count());
				auto delta_ms_duration = std::chrono::duration_cast<std::chrono::milliseconds>(delta_ms);
				std::this_thread::sleep_for(std::chrono::milliseconds(delta_ms_duration.count()));
			}
			elapsed = std::chrono::system_clock::now() - b;
			b = std::chrono::system_clock::now();
			glfwPollEvents();

			{
				vkWaitForFences(vku::state::device, 1, &vku::state::inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);

				uint32_t imageIndex;
				VkResult result = vkAcquireNextImageKHR(vku::state::device, vku::state::swapChain, UINT64_MAX,
					vku::state::imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);

				// check for out of date swap chain
				if (result == VK_ERROR_OUT_OF_DATE_KHR) {
					refreshSwapchain();
					continue;
				}
				else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
					throw std::runtime_error("Failed to acquire swap chain image!");
				}

				// Check if a previous frame is using this image (i.e. there is its fence to wait on)
				if (vku::state::imagesInFlight[imageIndex] != VK_NULL_HANDLE) {
					vkWaitForFences(vku::state::device, 1, &vku::state::imagesInFlight[imageIndex], VK_TRUE, UINT64_MAX);
				}
				// Mark the image as now being in use by this frame
				vku::state::imagesInFlight[imageIndex] = vku::state::inFlightFences[currentFrame];

				VkSubmitInfo submitInfo{};
				submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
				VkSemaphore waitSemaphores[] = { vku::state::imageAvailableSemaphores[currentFrame] };
				VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
				submitInfo.waitSemaphoreCount = 1;
				submitInfo.pWaitSemaphores = waitSemaphores;
				submitInfo.pWaitDstStageMask = waitStages;
				submitInfo.commandBufferCount = 1;

				VkCommandBuffer *commandBuffer = this->draw(imageIndex);
				submitInfo.pCommandBuffers = commandBuffer;

				VkSemaphore signalSemaphores[] = { vku::state::renderFinishedSemaphores[currentFrame] };
				submitInfo.signalSemaphoreCount = 1;
				submitInfo.pSignalSemaphores = signalSemaphores;

				vkResetFences(vku::state::device, 1, &vku::state::inFlightFences[currentFrame]);
				if (vkQueueSubmit(vku::state::graphicsQueue, 1, &submitInfo, vku::state::inFlightFences[currentFrame]) != VK_SUCCESS) {
					throw std::runtime_error("Failed to submit draw command buffer!");
				}

				VkSubpassDependency dependency{};
				dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
				dependency.dstSubpass = 0;
				dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
				dependency.srcAccessMask = 0;
				dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
				dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

				VkPresentInfoKHR presentInfo{};
				presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

				presentInfo.waitSemaphoreCount = 1;
				presentInfo.pWaitSemaphores = signalSemaphores;

				VkSwapchainKHR swapChains[] = { vku::state::swapChain };
				presentInfo.swapchainCount = 1;
				presentInfo.pSwapchains = swapChains;
				presentInfo.pImageIndices = &imageIndex;
				presentInfo.pResults = nullptr; // Optional

				result = vkQueuePresentKHR(vku::state::presentQueue, &presentInfo);

				if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebufferResized) {
					framebufferResized = false;
					refreshSwapchain();
				} else if (result != VK_SUCCESS) {
					throw std::runtime_error("Failed to present swap chain image!");
				}

				currentFrame = (currentFrame + 1) % vku::state::MAX_FRAMES_IN_FLIGHT;
			}
		}

		vkDeviceWaitIdle(vku::state::device);
	}
};