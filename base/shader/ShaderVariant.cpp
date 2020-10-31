#include "ShaderVariant.h"

#include <string>

namespace vku {
	size_t ShaderVariant::getHashcode() const {
		auto hasher = std::hash<std::string>();
		size_t hash = hasher(name);
		for (auto& x : macros) {
			std::string combined = x.first + "=" + x.second;
			hash = hash ^ hasher(combined);
		}
		return hash;
	}
}