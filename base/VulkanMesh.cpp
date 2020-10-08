#include "VulkanMesh.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>
#include <vulkan/vulkan.h>

#include "VulkanDevice.h"

namespace vku {
	VkVertexInputBindingDescription Vertex::getBindingDescription() {
		VkVertexInputBindingDescription bindingDescription{};

		bindingDescription.binding = 0;
		bindingDescription.stride = sizeof(Vertex);
		bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

		return bindingDescription;
	}
	std::vector<VkVertexInputAttributeDescription> Vertex::getAttributeDescriptions() {
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
	bool Vertex::operator==(const Vertex& other) const {
		return pos == other.pos && color == other.color && texCoord == other.texCoord && normal == other.normal && tangent == other.tangent;
	}

	VulkanMeshBuffer::VulkanMeshBuffer(VulkanDevice* device, const VulkanMeshData& mesh) {
		this->device = device;
		device->initDeviceLocalBuffer<uint32_t>(mesh.indices, iBuffer, iMemory, VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
		device->initDeviceLocalBuffer<Vertex>(mesh.vertices, vBuffer, vMemory, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
		indicesSize = mesh.indices.size();
	}
	void VulkanMeshBuffer::draw(VkCommandBuffer cmdbuf) {
		VkDeviceSize offsets[] = { 0 };
		vkCmdBindVertexBuffers(cmdbuf, 0, 1, &vBuffer, offsets);
		vkCmdBindIndexBuffer(cmdbuf, iBuffer, 0, VK_INDEX_TYPE_UINT32);
		vkCmdDrawIndexed(cmdbuf, indicesSize, 1, 0, 0, 0);
	}
	VulkanMeshBuffer::~VulkanMeshBuffer() {
		vkDestroyBuffer(*device, vBuffer, nullptr);
		vkDestroyBuffer(*device, iBuffer, nullptr);
		vkFreeMemory(*device, vMemory, nullptr);
		vkFreeMemory(*device, iMemory, nullptr);
	}
}

namespace std {
	template<> struct hash<vku::Vertex> {
		size_t operator()(vku::Vertex const& vertex) const {
			return ((hash<glm::vec3>()(vertex.pos) ^ (hash<glm::vec3>()(vertex.color) << 1)) >> 1) ^ (hash<glm::vec2>()(vertex.texCoord) << 1);
		}
	};
}