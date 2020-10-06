#include <glm/glm.hpp>
#include <vulkan/vulkan.h>


namespace vku::mesh {
	struct Vertex {
		glm::vec3 pos;
		glm::vec3 color;
		glm::vec2 texCoord;
		glm::vec3 normal;
		glm::vec4 tangent;
		static VkVertexInputBindingDescription getBindingDescription() {
			VkVertexInputBindingDescription bindingDescription{};

			bindingDescription.binding = 0;
			bindingDescription.stride = sizeof(Vertex);
			bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

			return bindingDescription;
		}
		static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions() {
			std::vector<VkVertexInputAttributeDescription> attributeDescriptions{};
			attributeDescriptions.resize(5);

			// vertex position attribute (vec3)
			attributeDescriptions[0].binding = 0;
			attributeDescriptions[0].location = 0;
			attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
			attributeDescriptions[0].offset = offsetof(Vertex, pos);

			// vertex color attribute (vec3)
			attributeDescriptions[1].binding = 0;
			attributeDescriptions[1].location = 1;
			attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
			attributeDescriptions[1].offset = offsetof(Vertex, color);

			attributeDescriptions[2].binding = 0;
			attributeDescriptions[2].location = 2;
			attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
			attributeDescriptions[2].offset = offsetof(Vertex, texCoord);

			attributeDescriptions[3].binding = 0;
			attributeDescriptions[3].location = 3;
			attributeDescriptions[3].format = VK_FORMAT_R32G32B32_SFLOAT;
			attributeDescriptions[3].offset = offsetof(Vertex, normal);

			attributeDescriptions[4].binding = 0;
			attributeDescriptions[4].location = 4;
			attributeDescriptions[4].format = VK_FORMAT_R32G32B32A32_SFLOAT;
			attributeDescriptions[4].offset = offsetof(Vertex, tangent);

			return attributeDescriptions;
		}
		bool operator==(const Vertex& other) const {
			return pos == other.pos && color == other.color && texCoord == other.texCoord && normal == other.normal && tangent == other.tangent;
		}
	};

	struct MeshData {
		std::vector<Vertex> vertices;
		std::vector<uint32_t> indices;
	};

	//struct MeshBuffer {
	//	VkBuffer vBuffer;
	//	VkDeviceMemory vMemory;
	//	VkBuffer iBuffer;
	//	VkDeviceMemory iMemory;
	//	uint32_t indicesSize;

	//	void load(MeshData& mesh) {
	//		vku::buffer::initDeviceLocalBuffer<uint32_t>(mesh.indices, iBuffer, iMemory, VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
	//		vku::buffer::initDeviceLocalBuffer<Vertex>(mesh.vertices, vBuffer, vMemory, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
	//		indicesSize = mesh.indices.size();
	//	}
	//	void destroy() {
	//		vkDestroyBuffer(vku::state::device, vBuffer, nullptr);
	//		vkDestroyBuffer(vku::state::device, iBuffer, nullptr);
	//		vkFreeMemory(vku::state::device, vMemory, nullptr);
	//		vkFreeMemory(vku::state::device, iMemory, nullptr);
	//	}
	//};

	MeshData box = {
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
	MeshData blit = {
		{
			{{-1,-1,0}, {},{0,0},{}},
			{{-1,3,0}, {},{0,2},{}},
			{{3,-1,0}, {},{2,0},{}},
		},
		{0,1,2}
	};
}
//namespace std {
//	template<> struct hash<vku::mesh::Vertex> {
//		size_t operator()(vku::mesh::Vertex const& vertex) const {
//			return ((hash<glm::vec3>()(vertex.pos) ^ (hash<glm::vec3>()(vertex.color) << 1)) >> 1) ^ (hash<glm::vec2>()(vertex.texCoord) << 1);
//		}
//	};
//}