#pragma once

#include <string>
#include <map>

namespace vku {
	struct ShaderVariant {
		std::string name;
		std::map<std::string, std::string> macros;
		size_t getHashcode() const;
	};
}