#include "ShaderCache.h"

// include stat command, independent of win32 or unix based
#include <sys/types.h>
#include <sys/stat.h>
#ifndef _WIN32 
#include <unistd.h>
#else
#define stat _stat
#endif

#include <unordered_set>
#include <filesystem>
#include <sstream>
#include <fstream>
#include <iostream>

#include <shaderc/shaderc.hpp>

#include "../VulkanDevice.h"
#include "ShaderVariant.h"
#include "ShaderModule.h"
#include "ShadercIncluder.h"

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

shaderc_shader_kind getShaderKind(const std::string& path) {
	int idx = path.rfind('.');
	if (idx != std::string::npos) {
		std::string ext = path.substr(idx + 1);
		if (ext == "vert") return shaderc_glsl_vertex_shader;
		else if (ext == "tesc") return shaderc_glsl_tess_control_shader;
		else if (ext == "tese") return shaderc_glsl_tess_evaluation_shader;
		else if (ext == "geom") return shaderc_glsl_geometry_shader;
		else if (ext == "frag") return shaderc_glsl_fragment_shader;
		else if (ext == "comp") return shaderc_glsl_compute_shader;
	}

	throw std::runtime_error("Error compiling shader, invalid filename: '" + path + "'");
}

VkShaderStageFlagBits shadercKindToVkFlag(const shaderc_shader_kind kind) {
	if (kind == shaderc_glsl_vertex_shader) return VK_SHADER_STAGE_VERTEX_BIT;
	else if (kind == shaderc_glsl_tess_control_shader) return VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
	else if (kind == shaderc_glsl_tess_evaluation_shader) return VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
	else if (kind == shaderc_glsl_geometry_shader) return VK_SHADER_STAGE_GEOMETRY_BIT;
	else if (kind == shaderc_glsl_fragment_shader) return VK_SHADER_STAGE_FRAGMENT_BIT;
	else if (kind == shaderc_glsl_compute_shader) return VK_SHADER_STAGE_COMPUTE_BIT;
	return {};
}

time_t getModTime(const std::string& filePath) {
	struct stat statResult;
	if (stat(filePath.c_str(), &statResult) == 0) {
		// glsl file exists
		return statResult.st_mtime;
	}

	throw std::runtime_error(std::string("Shader source file not found: '") + filePath + "'");
}

bool areShaderIncludesModified(time_t compiledTime, const std::string& filePath, time_t *mostRecentChange) {
	std::ifstream fileRead(filePath);

	int i = 0;
	while(fileRead.fail()) {
		std::cerr << "Error: " << strerror(errno) << std::endl;
		return false;
	}

	std::string fileFolder = std::filesystem::path(filePath).parent_path().string() + "/";

	std::string line;
	while (std::getline(fileRead, line)) {
		std::istringstream iss(line);
		std::string a;
		if (!(iss >> a)) { continue; }
		if (a == "#include") {
			std::string b;
			if (!(iss >> b)) { continue; }
			if (b[0] != '"' || b[b.size() - 1] != '"') {
				throw std::runtime_error("Invalid include directive in " + filePath + ": " + b);
			}

			std::string includedFilePath = fileFolder + b.substr(1, b.size() - 2);

			time_t modTime = getModTime(includedFilePath);
			if (compiledTime < modTime) {
				*mostRecentChange = modTime;
				return true;
			}

			if (areShaderIncludesModified(compiledTime, includedFilePath, mostRecentChange)) {
				return true;
			}
		}
	}

	return false;
}

// will set mostRecentChange if the shader or its dependencies were modified
bool isShaderModified(const std::string& shaderPath, const std::string& spvPath, time_t *mostRecentChange) {
	time_t modTime, spvModTime;

	modTime = getModTime(shaderPath);
	try {
		spvModTime = getModTime(spvPath);
	}
	catch (const std::exception& e) {
		*mostRecentChange = modTime;
		// could not read spv file. Definitely out of date
		return true;
	}

	// spv is out of date with shader
	if (spvModTime < modTime) {
		*mostRecentChange = modTime;
		return true;
	}

	return areShaderIncludesModified(spvModTime, shaderPath, mostRecentChange);
}

std::vector<uint32_t> recompileShader(const std::string& sourcePath, const std::map<std::string, std::string> macros, const std::string& spvPath) {
	std::cout << "Compiling '" << sourcePath << "'" << std::endl;

	std::vector<char> glslSourceVec = readFileCharVec(sourcePath);
	std::string glslSource(glslSourceVec.begin(), glslSourceVec.end());

	shaderc_shader_kind shaderType = getShaderKind(sourcePath);

	shaderc::Compiler compiler{};
	shaderc::CompileOptions options{};

	// hook #include api
	ShadercIncluder *includer = new ShadercIncluder();
	shaderc::CompileOptions::IncluderInterface *includerPtr = includer;
	options.SetIncluder(std::unique_ptr<shaderc::CompileOptions::IncluderInterface>(includerPtr));

	for (auto macro : macros) {
		options.AddMacroDefinition(macro.first, macro.second);
	}
	options.SetWarningsAsErrors();
	shaderc::SpvCompilationResult result = compiler.CompileGlslToSpv(glslSource, shaderType, sourcePath.c_str(), options);
	if (result.GetCompilationStatus() != shaderc_compilation_status_success) {
		std::cout << result.GetErrorMessage() << std::endl;
		throw std::runtime_error("Error compiling '" + sourcePath + "'");
	}

	std::vector<uint32_t> spirvData(result.cbegin(), result.cend());

	std::ofstream outStream(spvPath, std::ios::binary);
	outStream.write((char*)spirvData.data(), spirvData.size() * sizeof(uint32_t));
	outStream.close();

	return spirvData;
}

namespace vku {

	ShaderCache::ShaderCache(VulkanDevice* device) {
		this->device = device;
	}

	void ShaderCache::setSourceDirectory(const std::string& newShaderDirectory) {
		this->shaderDirectory = newShaderDirectory;
	}

	ShaderModule* ShaderCache::get(const std::string& name) {
		ShaderVariant variant{};
		variant.name = std::string(name);
		variant.getHashcode();
		return get(variant);
	}

	ShaderModule* ShaderCache::get(const ShaderVariant& variant) {
		size_t hash = variant.getHashcode();

		// If it's in the cache, use it
		if (runtimeShaderCache.find(hash) != runtimeShaderCache.end()) {
			return runtimeShaderCache[hash];
		}

		// convert hashcode to hex
		std::stringstream stream;
		stream << std::hex << variant.getHashcode();
		std::string hexHash(stream.str());

		// get file paths
		std::string shaderPath = shaderDirectory + variant.name;
		std::string spvPath = shaderPath + "." + hexHash + ".spv";

		std::vector<uint32_t> spirv;

		time_t dummy;
		if (isShaderModified(shaderPath, spvPath, &dummy)) {
			spirv = recompileShader(shaderPath, variant.macros, spvPath);
		}
		else {
			spirv = readFileIntVec(spvPath);
		}

		shaderc_shader_kind kind = getShaderKind(shaderPath);
		VkShaderStageFlagBits shaderStage = shadercKindToVkFlag(kind);
		ShaderModule* shader = new ShaderModule(device, spirv, shaderStage);
		shader->info = variant;
		this->runtimeShaderCache[hash] = shader;
		return shader;
	}

	void ShaderCache::hotReloadCheck() {
		std::unordered_set<ShaderCacheHotReloadCallback> callbacks{};
		std::vector<ShaderModule*> condemnedToDeletion{};
		for (auto& cached : runtimeShaderCache) {
			size_t hash = cached.first;

			// convert hashcode to hex
			std::stringstream stream;
			stream << std::hex << hash;
			std::string hexHash(stream.str());

			// get file paths
			std::string shaderPath = shaderDirectory + cached.second->info.name;
			std::string spvPath = shaderPath + "." + hexHash + ".spv";

			time_t mostRecentChange;
			bool shaderModified;
			try {
				shaderModified = isShaderModified(shaderPath, spvPath, &mostRecentChange);
			}
			catch (const std::runtime_error& e) {
				continue;
			}
			if (shaderModified) {
				time_t lastFailure = 0;
				auto failRecord = lastFailedHotCompilation.find(hash);
				if (failRecord != lastFailedHotCompilation.end()) {
					lastFailure = failRecord->second;
				}

				if (mostRecentChange > lastFailure) {
					std::vector<uint32_t> spirv;
					try {
						spirv = recompileShader(shaderPath, cached.second->info.macros, spvPath);
					}
					catch (const std::runtime_error& e) {
						lastFailedHotCompilation[hash] = mostRecentChange;
						continue;
					}
					shaderc_shader_kind kind = getShaderKind(shaderPath);
					VkShaderStageFlagBits shaderStage = shadercKindToVkFlag(kind);

					// delete later when it's fully replaced in dependencies
					condemnedToDeletion.push_back(cached.second);

					ShaderModule* shader = new ShaderModule(device, spirv, shaderStage);
					shader->info = cached.second->info;

					for (auto& callback : cached.second->hotReloadCallbacks) {
						callbacks.insert(callback);
					}

					this->runtimeShaderCache[hash] = shader;
				}
			}
		}

		if (callbacks.size() > 0) {
			vkQueueWaitIdle(device->graphicsQueue);
			vkQueueWaitIdle(device->presentQueue);
			for (ShaderCacheHotReloadCallback callback : callbacks) {
				callback.fun();
			}
		}

		for (auto* shaderModule : condemnedToDeletion) {
			delete shaderModule;
		}
	}

	ShaderCache::~ShaderCache() {
		for (auto& cached : runtimeShaderCache) {
			delete cached.second;
		}
	}
}