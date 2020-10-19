#include "VulkanMesh.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>
#include <mikktspace.h>
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

	void MikktCalculator::generateTangentSpace(VulkanMeshData* model) {
		SMikkTSpaceInterface iMikkt;
		iMikkt.m_getNumFaces = getNumFaces;
		iMikkt.m_getNumVerticesOfFace = getNumVerticesOfFace;
		iMikkt.m_getPosition = getPosition;
		iMikkt.m_getNormal = getNormal;
		iMikkt.m_getTexCoord = getTexCoord;
		iMikkt.m_setTSpaceBasic = setTSpaceBasic;
		iMikkt.m_setTSpace = nullptr;

		SMikkTSpaceContext context;
		context.m_pInterface = &iMikkt;
		context.m_pUserData = model;

		genTangSpaceDefault(&context);
	}

	// Return primitive count
	 int  MikktCalculator::getNumFaces(const SMikkTSpaceContext* context) {
		 VulkanMeshData* mesh = static_cast<VulkanMeshData*>(context->m_pUserData);

		return mesh->indices.size() / 3;
	}

	// Return number of vertices in the primitive given by index.
	 int  MikktCalculator::getNumVerticesOfFace(const SMikkTSpaceContext* context, const int primnum) {
		return 3;
	}

	// Write 3-float position of the vertex's point.
	 void MikktCalculator::getPosition(const SMikkTSpaceContext* context, float pos[], const int primnum, const int vtxnum) {
		 VulkanMeshData* mesh = static_cast<VulkanMeshData*>(context->m_pUserData);

		glm::vec3& v = mesh->vertices[mesh->indices[primnum * 3 + vtxnum]].pos;
		pos[0] = v[0];
		pos[1] = v[1];
		pos[2] = v[2];
	}

	// Write 3-float vertex normal.
	 void MikktCalculator::getNormal(const SMikkTSpaceContext* context, float normal[], const int primnum, const int vtxnum) {
		 VulkanMeshData* mesh = static_cast<VulkanMeshData*>(context->m_pUserData);

		glm::vec3& n = mesh->vertices[mesh->indices[primnum * 3 + vtxnum]].normal;
		normal[0] = n[0];
		normal[1] = n[1];
		normal[2] = n[2];
	}

	// Write 2-float vertex uv.
	 void MikktCalculator::getTexCoord(const SMikkTSpaceContext* context, float uv[], const int primnum, const int vtxnum) {
		 VulkanMeshData* mesh = static_cast<VulkanMeshData*>(context->m_pUserData);

		glm::vec2& tc = mesh->vertices[mesh->indices[primnum * 3 + vtxnum]].texCoord;
		uv[0] = tc[0];
		uv[1] = tc[1];
	}

	// Compute and set attributes on the geometry vertex.
	 void MikktCalculator::setTSpaceBasic(const SMikkTSpaceContext* context, const float tangentu[], const float sign, const int primnum, const int vtxnum) {
		 VulkanMeshData* mesh = static_cast<VulkanMeshData*>(context->m_pUserData);

		Vertex& v = mesh->vertices[mesh->indices[primnum * 3 + vtxnum]];
		v.tangent = glm::vec4(tangentu[0], tangentu[1], tangentu[2], sign);
	}
}