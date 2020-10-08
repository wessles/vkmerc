#include "VulkanShader.h"

// include stat command, independent of win32 or unix based
#include <sys/types.h>
#include <sys/stat.h>
#ifndef _WIN32 
#include <unistd.h>
#else
#define stat _stat
#endif

#include <iostream>
#include <fstream>
#include <string>
#include <vector>

#include <shaderc/shaderc.hpp>
#include <vulkan/vulkan.h>

#include "VulkanDevice.h"

std::vector<uint32_t> readFileIntVec(const std::string& filename) {
	std::ifstream file(filename, std::ios::ate | std::ios::binary);

	if (!file.is_open()) {
		throw std::runtime_error("Failed to open file!");
	}

	size_t fileSize = (size_t)file.tellg();
	std::vector<uint32_t> buffer(fileSize / sizeof(uint32_t), 0);

	file.seekg(0);
	file.read((char*)buffer.data(), fileSize);

	file.close();

	return buffer;
}

std::vector<char> readFileCharVec(const std::string& filename) {
	std::ifstream file(filename, std::ios::ate | std::ios::binary);

	if (!file.is_open()) {
		throw std::runtime_error("Failed to open file!");
	}

	size_t fileSize = (size_t)file.tellg();
	std::vector<char> buffer(fileSize);

	file.seekg(0);
	file.read((char*)buffer.data(), fileSize);

	file.close();

	return buffer;
}

namespace vku {
	VulkanShader::VulkanShader(VulkanDevice* device, std::vector<uint32_t> data) {
		this->device = device;

		VkShaderModuleCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		createInfo.codeSize = data.size() * sizeof(uint32_t);
		createInfo.pCode = data.data();

		if (vkCreateShaderModule(*device, &createInfo, nullptr, &handle) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create shader module!");
		}
	}

	VkPipelineShaderStageCreateInfo VulkanShader::getShaderStage(VkShaderStageFlagBits stage) {

		VkPipelineShaderStageCreateInfo shaderStageInfo{};
		shaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		shaderStageInfo.stage = stage;
		shaderStageInfo.module = this->handle;
		shaderStageInfo.pName = "main";

		return shaderStageInfo;
	}

	VulkanShader::~VulkanShader() {
		vkDestroyShaderModule(*device, *this, nullptr);
	}

	shaderc_shader_kind getShaderKind(const std::string& path) {
		int idx = path.rfind('.');
		if (idx != std::string::npos) {
			std::string ext = path.substr(idx + 1);
			if (ext == "vert") return shaderc_shader_kind::shaderc_glsl_vertex_shader;
			else if (ext == "tesc") return shaderc_shader_kind::shaderc_glsl_tess_control_shader;
			else if (ext == "tese") return shaderc_shader_kind::shaderc_glsl_tess_evaluation_shader;
			else if (ext == "geom") return shaderc_shader_kind::shaderc_glsl_geometry_shader;
			else if (ext == "frag") return shaderc_shader_kind::shaderc_glsl_fragment_shader;
			else if (ext == "comp") return shaderc_shader_kind::shaderc_glsl_compute_shader;
		}

		throw std::runtime_error("Error compiling shader, invalid filename: '" + path + "'");
	}

	/*
	Loads spirv shader binary. Before doing so, compiles glsl file
	at path IF spv is nonexistent or out of date.
	*/
	std::vector<uint32_t> lazyLoadSpirv(const std::string& path) {
		const std::string spvPath = path + ".spv";

		struct stat statResult;

		time_t spvModTime = 0;
		time_t shaderModTime = 0;

		if (stat(spvPath.c_str(), &statResult) == 0) {
			// spv file exists
			spvModTime = statResult.st_mtime;
		}

		if (stat(path.c_str(), &statResult) == 0) {
			// glsl file exists
			shaderModTime = statResult.st_mtime;
		}
		else {
			throw std::runtime_error(std::string("Shader source file not found: '") + path + "'");
		}

		std::vector<char> spirvData;

		// if spv not created (spvModTime = 0 & shaderModTime != 0 --> spvModTime < shaderModTime)
		// OR if spv is out of date (spvModTime < shaderModTime)
		// then compile to spv / cache next to the source file
		if (spvModTime < shaderModTime) {
			std::cout << "Compiling '" << path << "'" << std::endl;

			std::vector<char> glslSourceVec = readFileCharVec(path);
			std::string glslSource(glslSourceVec.begin(), glslSourceVec.end());

			shaderc_shader_kind shaderType = getShaderKind(path);

			shaderc::Compiler compiler{};
			shaderc::CompileOptions options{};
			options.SetWarningsAsErrors();
			shaderc::SpvCompilationResult result = compiler.CompileGlslToSpv(glslSource, shaderType, path.c_str(), options);
			if (result.GetCompilationStatus() != shaderc_compilation_status_success) {
				std::cout << result.GetErrorMessage() << std::endl;
				throw std::runtime_error("Error compiling '" + path + "'");
			}

			std::vector<uint32_t> spirvData(result.cbegin(), result.cend());

			std::ofstream outStream(spvPath, std::ios::binary);
			outStream.write((char*)spirvData.data(), spirvData.size() * sizeof(uint32_t));
			outStream.close();

			return spirvData;
		}
		else {
			std::cout << "Loading from cached spv '" << path << "'" << std::endl;
			return readFileIntVec(spvPath);
		}
	}
}