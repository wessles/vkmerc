#pragma once

#include <vector>

#include <glm/glm.hpp>
#include <mikktspace.h>
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

	struct MikktCalculator {
		void generateTangentSpace(VulkanMeshData* model);
		// Return primitive count
		static int  getNumFaces(const SMikkTSpaceContext* context);
		// Return number of vertices in the primitive given by index.
		static int  getNumVerticesOfFace(const SMikkTSpaceContext* context, const int primnum);
		// Write 3-float position of the vertex's point.
		static void getPosition(const SMikkTSpaceContext* context, float pos[], const int primnum, const int vtxnum);
		// Write 3-float vertex normal.
		static void getNormal(const SMikkTSpaceContext* context, float normal[], const int primnum, const int vtxnum);
		// Write 2-float vertex uv.
		static void getTexCoord(const SMikkTSpaceContext* context, float uv[], const int primnum, const int vtxnum);
		// Compute and set attributes on the geometry vertex.
		static void setTSpaceBasic(const SMikkTSpaceContext* context, const float tangentu[], const float sign, const int primnum, const int vtxnum);
		// We leave this one out since I haven't found a use for it yet
		//static void setTSpace(const SMikkTSpaceContext* context, const float tangentu[], const float tangentv[], const float magu, const float magv, const tbool keep, const int primnum, const int vtxnum);
	};
}

namespace std {
	template<> struct hash<vku::Vertex>;
}