#pragma once

#include <iostream>
#include <fstream>
#include <string>

#include <shaderc/shaderc.hpp>
#include <vulkan/vulkan.h>

#include "Vku.hpp"

// include stat command, independent of win32 or unix based
#include <sys/types.h>
#include <sys/stat.h>
#ifndef _WIN32 
#include <unistd.h>
#else
#define stat _stat
#endif

namespace vku::shadercomp {
	namespace {
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
		} else {
			throw std::runtime_error(std::string("Shader source file not found: '") + path + "'");
		}

		std::vector<char> spirvData;

		// if spv not created (spvModTime = 0 & shaderModTime != 0 --> spvModTime < shaderModTime)
		// OR if spv is out of date (spvModTime < shaderModTime)
		// then compile to spv / cache next to the source file
		if (spvModTime < shaderModTime) {
			std::cout << "Compiling '" << path << "'" << std::endl;

			std::vector<char> glslSourceVec = vku::io::readFileCharVec(path);
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
		} else {
			std::cout << "Loading from cached spv '" << path << "'" << std::endl;
			return vku::io::readFileIntVec(spvPath);
		}
	}
}