#pragma once

#include <string>
#include <glm/gtc/type_ptr.hpp>

#include <tiny_gltf.h>

#include <mikktspace.h>

#include "VulkanMesh.h"
#include "VulkanTexture.h"
#include "VulkanDevice.h"
#include "scene/Scene.h"
#include "scene/Object.h"
#include "PbrMaterial.h"


/*
	Major thanks to Sascha Willem's GLTF example, I took a lot of inspiration from it:
	https://github.com/SaschaWillems/Vulkan/blob/master/examples/gltfloading/gltfloading.cpp
*/

namespace vku {
	struct VulkanGltfModel : public Object {
		// A primitive contains the data for a single draw call
		struct Primitive {
			uint32_t firstIndex;
			uint32_t indexCount;
			int32_t materialIndex;
		};

		// Contains the node's (optional) geometry and can be made up of an arbitrary number of primitives
		struct Mesh {
			std::vector<Primitive> primitives;
		};

		// A node represents an object in the glTF scene graph
		struct Node {
			Node* parent;
			std::vector<Node> children;
			Mesh mesh;
			glm::mat4 matrix;
		};

		VulkanMeshBuffer* meshBuf;

		std::vector<VulkanTexture*> textures;
		std::vector<PbrMaterial*> materials;
		std::vector<Node> nodes;

		VulkanGltfModel() {}

		VulkanGltfModel(const std::string& filename, Scene* scene, Pass* pass, VulkanTexture* brdf_lut, VulkanTexture* diffuse_ibl, VulkanTexture* specular_ibl) {
			tinygltf::Model model;

			tinygltf::TinyGLTF loader;
			std::string err;
			std::string warn;

			bool res = loader.LoadBinaryFromFile(&model, &err, &warn, filename);
			if (!warn.empty()) {
				throw std::runtime_error("TinyGLTF Warning: " + warn);
			}

			if (!err.empty()) {
				throw std::runtime_error("TinyGLTF Error: " + err);
			}

			if (!res)
				std::cout << "Failed to load glTF: " << filename << std::endl;
			else
				std::cout << "Loaded glTF: " << filename << std::endl;

			loadTextures(scene->device, model);
			loadMaterials(scene, pass, model, brdf_lut, diffuse_ibl, specular_ibl);

			VulkanMeshData meshData;

			for (uint32_t i = 0; i < model.nodes.size(); i++) {
				tinygltf::Node& gNode = model.nodes[i];
				loadNode(gNode, model, nullptr, meshData.indices, meshData.vertices);
				Node& node = nodes[i];
			}

			MikktCalculator mikkt;
			mikkt.generateTangentSpace(&meshData);

			meshBuf = new VulkanMeshBuffer(scene->device, meshData);
		}

		struct ImageWithView {
			VulkanImage* image;
			VulkanImageView* view;
		};
		ImageWithView loadImage(VulkanDevice* device, uint32_t i, tinygltf::Model& model) {
			tinygltf::Image gImage = model.images[i];

			// Get the image data from the glTF loader
			unsigned char* buffer = nullptr;
			VkDeviceSize bufferSize = 0;
			bool deleteBuffer = false;

			if (gImage.component == 3) {
				bufferSize = gImage.width * gImage.height * 4;
				buffer = new unsigned char[bufferSize];
				unsigned char* rgba = buffer;
				unsigned char* rgb = &gImage.image[0];
				for (size_t i = 0; i < gImage.width * gImage.height; ++i) {
					memcpy(rgba, rgb, sizeof(unsigned char) * 3);
					rgba += 4;
					rgb += 3;
				}
				deleteBuffer = true;
			}
			else {
				buffer = &gImage.image[0];
				bufferSize = gImage.image.size();
			}

			// Load texture from image buffer
			ImageWithView image{};
			{
				VkBuffer stagingBuffer;
				VkDeviceMemory stagingBufferMemory;
				device->createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
					VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
					stagingBuffer, stagingBufferMemory);

				void* data;
				vkMapMemory(*device, stagingBufferMemory, 0, bufferSize, 0, &data);
				memcpy(data, buffer, static_cast<size_t>(bufferSize));
				vkUnmapMemory(*device, stagingBufferMemory);

				VulkanImageInfo info{};
				info.width = gImage.width;
				info.height = gImage.height;
				info.format = VK_FORMAT_R8G8B8A8_UNORM;
				info.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
				image.image = new VulkanImage(device, info);
				image.image->loadFromBuffer(stagingBuffer);

				vkDestroyBuffer(*device, stagingBuffer, nullptr);
				vkFreeMemory(*device, stagingBufferMemory, nullptr);

				VulkanImageViewInfo viewInfo{};
				image.image->writeImageViewInfo(&viewInfo);
				image.view = new VulkanImageView(device, viewInfo);
			}

			if (deleteBuffer) {
				delete buffer;
			}

			return image;
		}
		VkFilter convertTinyGltfFilter(int tinyGltfFilter) {
			switch (tinyGltfFilter) {
			case TINYGLTF_TEXTURE_FILTER_NEAREST:
				return VK_FILTER_NEAREST;
			case TINYGLTF_TEXTURE_FILTER_LINEAR:
			default:
				return VK_FILTER_LINEAR;
			}
		}
		VkSamplerAddressMode convertTinyGltfAddressMode(int tinyGltfAddressMode) {
			switch (tinyGltfAddressMode) {
			case TINYGLTF_TEXTURE_WRAP_REPEAT:
				return VK_SAMPLER_ADDRESS_MODE_REPEAT;
			case TINYGLTF_TEXTURE_WRAP_MIRRORED_REPEAT:
				return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
			case TINYGLTF_TEXTURE_WRAP_CLAMP_TO_EDGE:
			default:
				return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
			}
		}
		VulkanSampler* loadSampler(VulkanDevice* device, uint32_t i, tinygltf::Model& model) {
			tinygltf::Sampler& gSampler = model.samplers[i];

			VulkanSamplerInfo info{};

			// set up min/mag filters
			info.minFilter = convertTinyGltfFilter(gSampler.minFilter);
			info.magFilter = convertTinyGltfFilter(gSampler.magFilter);

			// set up wraps
			info.addressModeU = convertTinyGltfAddressMode(gSampler.wrapR);
			info.addressModeV = convertTinyGltfAddressMode(gSampler.wrapS);
			info.addressModeW = convertTinyGltfAddressMode(gSampler.wrapT);

			VulkanSampler* sampler = new VulkanSampler(device, info);

			return sampler;
		}
		void loadTextures(VulkanDevice* device, tinygltf::Model& model) {
			textures.resize(model.textures.size());
			for (uint32_t i = 0; i < model.textures.size(); i++) {
				tinygltf::Texture& gTexture = model.textures[i];
				textures[i] = new VulkanTexture;

				ImageWithView imageAndView = loadImage(device, gTexture.source, model);
				textures[i]->image = imageAndView.image;
				textures[i]->view = imageAndView.view;
				textures[i]->sampler = loadSampler(device, gTexture.sampler, model);
			}
		}

		void loadMaterials(Scene* scene, Pass* pass, tinygltf::Model& model, VulkanTexture* brdf_lut, VulkanTexture* diffuse_ibl, VulkanTexture* specular_ibl) {
			materials.resize(model.materials.size());

			for (uint32_t i = 0; i < model.materials.size(); i++) {
				tinygltf::Material& gMaterial = model.materials[i];
				PbrMaterial*& material = materials[i];

				int colorTexIdx = gMaterial.pbrMetallicRoughness.baseColorTexture.index;
				int normalTexIdx = gMaterial.normalTexture.index;
				int metallicTexIdx = gMaterial.pbrMetallicRoughness.metallicRoughnessTexture.index;
				int emissiveTexIdx = gMaterial.emissiveTexture.index;
				int aoTexIdx = gMaterial.occlusionTexture.index;

				VulkanTexture* colorTexture = textures[colorTexIdx];
				VulkanTexture* normalTexture = textures[normalTexIdx];
				VulkanTexture* metallicTexture = textures[metallicTexIdx];
				VulkanTexture* emissiveTexture = textures[emissiveTexIdx];
				VulkanTexture* aoTexture = textures[aoTexIdx];

				material = new PbrMaterial(colorTexture, normalTexture, metallicTexture, emissiveTexture, aoTexture, specular_ibl, diffuse_ibl, brdf_lut, scene, pass);
			}
		}

		void loadNode(tinygltf::Node& gNode, tinygltf::Model& model, Node* parent, std::vector<uint32_t>& indexBuffer, std::vector<Vertex>& vertexBuffer) {
			Node node{};
			node.matrix = glm::mat4(1.0f);

			if (gNode.translation.size() == 3) {
				node.matrix = glm::translate(node.matrix, glm::vec3(glm::make_vec3(gNode.translation.data())));
			}
			if (gNode.rotation.size() == 4) {
				node.matrix *= glm::mat4(glm::quat(glm::make_quat(gNode.rotation.data())));
			}
			if (gNode.scale.size() == 3) {
				node.matrix = glm::scale(node.matrix, glm::vec3(glm::make_vec3(gNode.scale.data())));
			}
			if (gNode.matrix.size() == 16) {
				node.matrix = glm::mat4(glm::make_mat4(gNode.matrix.data()));
			}

			for (uint32_t childIdx : gNode.children) {
				loadNode(model.nodes[childIdx], model, &node, indexBuffer, vertexBuffer);
			}

			if (gNode.mesh > -1) {
				const tinygltf::Mesh& mesh = model.meshes[gNode.mesh];

				for (size_t i = 0; i < mesh.primitives.size(); i++) {
					const tinygltf::Primitive& gPrimitive = mesh.primitives[i];

					uint32_t firstIndex = static_cast<uint32_t>(indexBuffer.size());
					uint32_t vertexStart = static_cast<uint32_t>(vertexBuffer.size());
					uint32_t indexCount = 0;

					// Vertex
					{
						const float* positionBuffer = nullptr;
						const float* normalsBuffer = nullptr;
						const float* tangentsBuffer = nullptr;
						const float* texCoordsBuffer = nullptr;
						size_t vertexCount = 0;

						if (gPrimitive.attributes.find("POSITION") != gPrimitive.attributes.end()) {
							const tinygltf::Accessor& accessor = model.accessors[gPrimitive.attributes.find("POSITION")->second];
							const tinygltf::BufferView& view = model.bufferViews[accessor.bufferView];
							positionBuffer = reinterpret_cast<const float*>(&(model.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
							vertexCount = accessor.count;
						}
						if (gPrimitive.attributes.find("NORMAL") != gPrimitive.attributes.end()) {
							const tinygltf::Accessor& accessor = model.accessors[gPrimitive.attributes.find("NORMAL")->second];
							const tinygltf::BufferView& view = model.bufferViews[accessor.bufferView];
							normalsBuffer = reinterpret_cast<const float*>(&(model.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
						}
						if (gPrimitive.attributes.find("TANGENT") != gPrimitive.attributes.end()) {
							const tinygltf::Accessor& accessor = model.accessors[gPrimitive.attributes.find("TANGENT")->second];
							const tinygltf::BufferView& view = model.bufferViews[accessor.bufferView];
							tangentsBuffer = reinterpret_cast<const float*>(&(model.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
						}
						if (gPrimitive.attributes.find("TEXCOORD_0") != gPrimitive.attributes.end()) {
							const tinygltf::Accessor& accessor = model.accessors[gPrimitive.attributes.find("TEXCOORD_0")->second];
							const tinygltf::BufferView& view = model.bufferViews[accessor.bufferView];
							texCoordsBuffer = reinterpret_cast<const float*>(&(model.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
						}


						for (size_t v = 0; v < vertexCount; v++) {
							Vertex vert{};
							vert.pos = glm::vec4(glm::make_vec3(&positionBuffer[v * 3]), 1.0f);
							vert.normal = glm::normalize(glm::vec3(normalsBuffer ? glm::make_vec3(&normalsBuffer[v * 3]) : glm::vec3(0.0f)));
							vert.tangent = tangentsBuffer ? glm::make_vec4(&tangentsBuffer[v * 4]) : glm::vec4(0.0f);
							vert.texCoord = texCoordsBuffer ? glm::make_vec2(&texCoordsBuffer[v * 2]) : glm::vec3(0.0f);
							vert.color = glm::vec3(1.0f);
							vertexBuffer.push_back(vert);
						}
					}
					// Indices
					{
						const tinygltf::Accessor& accessor = model.accessors[gPrimitive.indices];
						const tinygltf::BufferView& bufferView = model.bufferViews[accessor.bufferView];
						const tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];

						indexCount += static_cast<uint32_t>(accessor.count);

						switch (accessor.componentType) {
						case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT: {
							uint32_t* buf = new uint32_t[accessor.count];
							memcpy(buf, &buffer.data[accessor.byteOffset + bufferView.byteOffset], accessor.count * sizeof(uint32_t));
							for (size_t index = 0; index < accessor.count; index++) {
								indexBuffer.push_back(buf[index] + vertexStart);
							}
							break;
						}
						case TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT: {
							uint16_t* buf = new uint16_t[accessor.count];
							memcpy(buf, &buffer.data[accessor.byteOffset + bufferView.byteOffset], accessor.count * sizeof(uint16_t));
							for (size_t index = 0; index < accessor.count; index++) {
								indexBuffer.push_back(buf[index] + vertexStart);
							}
							break;
						}
						case TINYGLTF_PARAMETER_TYPE_UNSIGNED_BYTE: {
							uint8_t* buf = new uint8_t[accessor.count];
							memcpy(buf, &buffer.data[accessor.byteOffset + bufferView.byteOffset], accessor.count * sizeof(uint8_t));
							for (size_t index = 0; index < accessor.count; index++) {
								indexBuffer.push_back(buf[index] + vertexStart);
							}
							break;
						}
						default:
							throw std::runtime_error("Index component type " + std::to_string(accessor.componentType) + " not supported.");
						}
					}

					Primitive primitive{};
					primitive.firstIndex = firstIndex;
					primitive.indexCount = indexCount;
					primitive.materialIndex = gPrimitive.material;
					node.mesh.primitives.push_back(primitive);
				}
			}

			if (parent) {
				parent->children.push_back(node);
			}
			else {
				nodes.push_back(node);
			}
		}

		void drawNode(VkCommandBuffer commandBuffer, uint32_t i, Node& node, bool noMaterial) {
			if (node.mesh.primitives.size() > 0) {
				// Pass the node's matrix via push constants
				// Traverse the node hierarchy to the top-most parent to get the final matrix of the current node
				glm::mat4 nodeMatrix = node.matrix;
				Node* currentParent = node.parent;
				while (currentParent) {
					nodeMatrix = currentParent->matrix * nodeMatrix;
					currentParent = currentParent->parent;
				}

				for (Primitive& primitive : node.mesh.primitives) {
					if (primitive.indexCount > 0) {
						VulkanMaterialInstance* materialInstance = materials[primitive.materialIndex]->matInstance;
						if (!noMaterial) {
							materialInstance->material->bind(commandBuffer);
							materialInstance->bind(commandBuffer, i);
						}

						vkCmdPushConstants(commandBuffer, materialInstance->material->scene->globalPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(glm::mat4), &nodeMatrix);
						vkCmdDrawIndexed(commandBuffer, primitive.indexCount, 1, primitive.firstIndex, 0, 0);
					}
				}
			}
			for (auto& child : node.children) {
				drawNode(commandBuffer, i, child, noMaterial);
			}
		}

		virtual void render(VkCommandBuffer cmdBuf, uint32_t swapIdx, bool noMaterial) {
			VkDeviceSize offsets[1] = { 0 };
			vkCmdBindVertexBuffers(cmdBuf, 0, 1, &meshBuf->vBuffer, offsets);
			vkCmdBindIndexBuffer(cmdBuf, meshBuf->iBuffer, 0, VK_INDEX_TYPE_UINT32);
			for (auto& node : nodes) {
				drawNode(cmdBuf, swapIdx, node, noMaterial);
			}
		}

		~VulkanGltfModel() {

			for (VulkanTexture* texture : textures) {
				delete texture;
			}
			for (PbrMaterial* material : materials) {
				delete material;
			}
			delete meshBuf;
		}
	};
}