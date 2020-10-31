#pragma once

#include <string>

#include <shaderc/shaderc.hpp>

struct IncluderResultData {
	std::string content;
	std::string sourceName;
};

struct ShadercIncluder : shaderc::CompileOptions::IncluderInterface {
	virtual shaderc_include_result* GetInclude(const char* requested_source, shaderc_include_type type, const char* requesting_source, size_t include_depth) override;
	virtual void ReleaseInclude(shaderc_include_result* data) override;
};