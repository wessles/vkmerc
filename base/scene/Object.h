#pragma once

#include <vector>

#include <glm/glm.hpp>


namespace vku {
	struct Scene;

	struct Object {
		Scene* scene;

		glm::mat4 transform;

		Object* parent;
	};
}