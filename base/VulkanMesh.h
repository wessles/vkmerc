#pragma once

#include <vector>

#include <glm/glm.hpp>
#include <vulkan/vulkan.h>

namespace vku {
	struct VulkanDevice;

	struct Vertex {
		glm::vec3 pos;
		glm::vec3 color;
		glm::vec2 texCoord;
		glm::vec3 normal;
		glm::vec4 tangent;

		static VkVertexInputBindingDescription getBindingDescription();
		static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions();
		bool operator==(const Vertex& other) const;
	};

	struct VulkanMeshData {
		std::vector<Vertex> vertices;
		std::vector<uint32_t> indices;
	};

	struct VulkanMeshBuffer {
		VulkanDevice* device;

		VkBuffer vBuffer;
		VkDeviceMemory vMemory;
		VkBuffer iBuffer;
		VkDeviceMemory iMemory;
		uint32_t indicesSize;

		VulkanMeshBuffer(VulkanDevice* device, const VulkanMeshData& mesh);
		void draw(VkCommandBuffer cmdbuf);
		~VulkanMeshBuffer();
	};

	const VulkanMeshData box = {
		{
			{{-1, -1, -1}, {}, {}, {1, 1, 1}},
			{{1, -1, -1}, {}, {}, {-1, 1, 1}},
			{{-1, 1, -1}, {}, {}, {1, -1, 1}},
			{{1, 1, -1}, {}, {}, {-1, -1, 1}},
			{{-1, -1, 1}, {}, {}, {1, 1, -1}},
			{{1, -1, 1}, {}, {}, {-1, 1, -1}},
			{{-1, 1, 1}, {}, {}, {1, -1, -1}},
			{{1, 1, 1}, {}, {}, {-1, -1, -1}},
		},
		{
			0, 2, 3,
			0, 3, 1,
			1, 7, 5,
			1, 3, 7,
			4, 5, 7,
			4, 7, 6,
			4, 6, 2,
			4, 2, 0,
			6, 7, 2,
			2, 7, 3,
			1, 5, 4,
			0, 1, 4
		}
	};
	const VulkanMeshData blit = {
		{
			{{-1,-1,0}, {},{0,0},{}},
			{{-1,3,0}, {},{0,2},{}},
			{{3,-1,0}, {},{2,0},{}},
		},
		{0,1,2}
	};
}

namespace std {
	template<> struct hash<vku::Vertex>;
}