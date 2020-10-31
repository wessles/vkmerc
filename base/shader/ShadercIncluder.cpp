#include "ShadercIncluder.h"

#include <filesystem>
#include <fstream>

#include <shaderc/shaderc.hpp>


shaderc_include_result* ShadercIncluder::GetInclude(const char* requested_source, shaderc_include_type type, const char* requesting_source, size_t include_depth) {
	shaderc_include_result* result = new shaderc_include_result();
	IncluderResultData* data = new IncluderResultData();

	data->sourceName = std::filesystem::path(requesting_source).parent_path().string() + "/" + std::string(requested_source);
	std::ifstream fileStream(data->sourceName);
	data->content = std::string((std::istreambuf_iterator<char>(fileStream)), std::istreambuf_iterator<char>());

	result->content = data->content.c_str();
	result->content_length = data->content.size();
	result->source_name = data->sourceName.c_str();
	result->source_name_length = data->sourceName.size();
	result->user_data = data;

	return result;
}

void ShadercIncluder::ReleaseInclude(shaderc_include_result* data) {
	IncluderResultData* info = static_cast<IncluderResultData*>(data->user_data);
	delete info;
	delete data;
}