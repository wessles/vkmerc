#pragma once

#include <string>
#include <vector>
#include <map>
#include <functional>

#include <vulkan/vulkan.h>

#include "../util/Semaphore.h"

namespace vku {
	struct VulkanDevice;
	struct ShaderVariant;
	struct ShaderModule;

	struct ShaderCacheHotReloadCallback {
		std::function<void()> fun;
		size_t handle;

		bool operator==(const ShaderCacheHotReloadCallback& x) const {
			return x.handle == handle;
		}
	};

	struct ShaderCache {
		VulkanDevice* device;

		std::map<size_t, ShaderModule*> runtimeShaderCache;
		std::map<size_t, time_t> lastFailedHotCompilation;

		std::string shaderDirectory = "res/shaders/";

		ShaderCache(VulkanDevice *device);

		void setSourceDirectory(const std::string &newShaderDirectory);

		ShaderModule* get(const std::string& name);
		ShaderModule* get(const ShaderVariant& variant);
		ShaderModule* get(size_t hash);

		void hotReloadCheck(Semaphore* initiateReload, Semaphore* allowContinue, std::atomic<bool> *requestReloadFlag);

		~ShaderCache();
	};

	void hotReloadCheckingThread(ShaderCache* shaderCache, Semaphore* initiateReload, Semaphore* allowContinue, std::atomic<bool>* requestReloadFlag, std::atomic<bool>* reloadThreadKill);
}

namespace std {
	template<>
	struct hash<vku::ShaderCacheHotReloadCallback>
	{
		size_t operator()(const vku::ShaderCacheHotReloadCallback& x) const
		{
			return x.handle;
		}
	};
}