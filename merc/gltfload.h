#pragma once

#include <string>
#include <glm/gtc/type_ptr.hpp>

#include "mikktspace.h"

#include "Vku.hpp"


using namespace vku::mesh;

/*
	Major thanks to Sascha Willem's GLTF example, I took a lot of inspiration from it:
	https://github.com/SaschaWillems/Vulkan/blob/master/examples/gltfloading/gltfloading.cpp
*/

namespace vku::gltf {
	struct MikktCalculator {
		void generateTangentSpace(MeshData *model) {
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
		static int  getNumFaces(const SMikkTSpaceContext *context) {
			MeshData *mesh = static_cast<MeshData*>(context->m_pUserData);

			return mesh->indices.size() / 3;
		}

		// Return number of vertices in the primitive given by index.
		static int  getNumVerticesOfFace(const SMikkTSpaceContext *context, const int primnum) {
			return 3;
		}

		// Write 3-float position of the vertex's point.
		static void getPosition(const SMikkTSpaceContext *context, float pos[], const int primnum, const int vtxnum) {
			MeshData *mesh = static_cast<MeshData*>(context->m_pUserData);

			glm::vec3 &v = mesh->vertices[mesh->indices[primnum * 3 + vtxnum]].pos;
			pos[0] = v[0];
			pos[1] = v[1];
			pos[2] = v[2];
		}

		// Write 3-float vertex normal.
		static void getNormal(const SMikkTSpaceContext *context, float normal[], const int primnum, const int vtxnum) {
			MeshData *mesh = static_cast<MeshData*>(context->m_pUserData);

			glm::vec3 &n = mesh->vertices[mesh->indices[primnum * 3 + vtxnum]].normal;
			normal[0] = n[0];
			normal[1] = n[1];
			normal[2] = n[2];
		}

		// Write 2-float vertex uv.
		static void getTexCoord(const SMikkTSpaceContext *context, float uv[], const int primnum, const int vtxnum) {
			MeshData *mesh = static_cast<MeshData*>(context->m_pUserData);

			glm::vec2 &tc = mesh->vertices[mesh->indices[primnum * 3 + vtxnum]].texCoord;
			uv[0] = tc[0];
			uv[1] = tc[1];
		}

		// Compute and set attributes on the geometry vertex.
		static void setTSpaceBasic(const SMikkTSpaceContext *context, const float tangentu[], const float sign, const int primnum, const int vtxnum) {
			MeshData *mesh = static_cast<MeshData*>(context->m_pUserData);
			
			Vertex &v = mesh->vertices[mesh->indices[primnum * 3 + vtxnum]];
			v.tangent = glm::vec4(tangentu[0], tangentu[1], tangentu[2], sign);
		}

		static void setTSpace(const SMikkTSpaceContext *context, const float tangentu[], const float tangentv[], const float magu, const float magv, const tbool keep, const int primnum, const int vtxnum);

	};
	struct GltfModel {
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

		MeshBuffer meshBuf{};

		std::vector<Texture> textures;
		std::vector<Material> materials;
		std::vector<MaterialInstance> materialInstances;
		std::vector<Node> nodes;

		GltfModel() {}

		GltfModel(const std::string& filename, VkDescriptorSetLayout globalDSetLayout, RNode *pass, VkDescriptorPool descriptorPool) {
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

			loadTextures(model);
			loadMaterials(globalDSetLayout, pass, descriptorPool, model);


			MeshData meshData;

			
			for (uint32_t i = 0; i < model.nodes.size(); i++) {
				tinygltf::Node &gNode = model.nodes[i];
				loadNode(gNode, model, nullptr, meshData.indices, meshData.vertices);
				Node &node = nodes[i];
			}

			MikktCalculator mikkt;
			mikkt.generateTangentSpace(&meshData);

			meshBuf.load(meshData);
		}

		Image loadImage(uint32_t i, tinygltf::Model &model) {
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
			} else {
				buffer = &gImage.image[0];
				bufferSize = gImage.image.size();
			}

			// Load texture from image buffer
			Image image{};
			{
				VkBuffer stagingBuffer;
				VkDeviceMemory stagingBufferMemory;
				vku::buffer::createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
					VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
					stagingBuffer, stagingBufferMemory);

				void* data;
				vkMapMemory(state::device, stagingBufferMemory, 0, bufferSize, 0, &data);
				memcpy(data, buffer, static_cast<size_t>(bufferSize));
				vkUnmapMemory(state::device, stagingBufferMemory);

				vku::image::loadFromBuffer(stagingBuffer, gImage.width, gImage.height, image);

				vkDestroyBuffer(vku::state::device, stagingBuffer, nullptr);
				vkFreeMemory(vku::state::device, stagingBufferMemory, nullptr);

				image.view = vku::image::createImageView(image.handle, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT, image.mipLevels);
			}
				
			if (deleteBuffer) {
				delete buffer;
			}

			return image;
		}
		VkFilter convertTinyGltfFilter(int tinyGltfFilter) {
			switch (tinyGltfFilter) {
			case TINYGLTF_TEXTURE_FILTER_LINEAR:
				return VK_FILTER_LINEAR;
			case TINYGLTF_TEXTURE_FILTER_NEAREST:
			default:
				return VK_FILTER_NEAREST;
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
		VkSampler loadSampler(uint32_t i, tinygltf::Model &model) {
			tinygltf::Sampler &gSampler = model.samplers[i];
			VkSampler sampler{};

			VkSamplerCreateInfo samplerCI = vku::create::createSamplerCI();

			// set up min/mag filters
			samplerCI.minFilter = convertTinyGltfFilter(gSampler.minFilter);
			samplerCI.magFilter = convertTinyGltfFilter(gSampler.magFilter);
				
			// set up wraps
			samplerCI.addressModeU = convertTinyGltfAddressMode(gSampler.wrapR);
			samplerCI.addressModeV = convertTinyGltfAddressMode(gSampler.wrapS);
			samplerCI.addressModeW = convertTinyGltfAddressMode(gSampler.wrapT);

			if (vkCreateSampler(vku::state::device, &samplerCI, nullptr, &sampler) != VK_SUCCESS) {
				throw std::runtime_error("Failed to create sampler.");
			}

			return sampler;
		}
		void loadTextures(tinygltf::Model &model) {
			textures.resize(model.textures.size());
			for (uint32_t i = 0; i < model.textures.size(); i++) {
				tinygltf::Texture &gTexture = model.textures[i];
				Texture &texture = textures[i];

				texture.image = loadImage(gTexture.source, model);
				texture.sampler = loadSampler(gTexture.sampler, model);
			}
		}

		void loadMaterials(VkDescriptorSetLayout globalDSetLayout, RNode *pass, VkDescriptorPool descriptorPool, tinygltf::Model &model) {
			materials.resize(model.materials.size());
			materialInstances.resize(model.materials.size());

			for (uint32_t i = 0; i < model.materials.size(); i++) {
				tinygltf::Material &gMaterial = model.materials[i];
				Material &material = materials[i];
				MaterialInstance &materialInstance = materialInstances[i];

				material = Material(globalDSetLayout, pass->pass, pass->inputLayout, {
					{"res/shaders/pbr/pbr.vert", VK_SHADER_STAGE_VERTEX_BIT},
					{"res/shaders/pbr/pbr.frag", VK_SHADER_STAGE_FRAGMENT_BIT}
					});
				material.createPipeline();

				materialInstance = MaterialInstance(&material, descriptorPool);
				Texture &colorTexture = textures[gMaterial.pbrMetallicRoughness.baseColorTexture.index];
				materialInstance.write(0, colorTexture);
				Texture &normalTexture = textures[gMaterial.normalTexture.index];
				materialInstance.write(1, normalTexture);
			}
		}

		void loadNode(tinygltf::Node &gNode, tinygltf::Model &model, Node *parent, std::vector<uint32_t>& indexBuffer, std::vector<Vertex>& vertexBuffer) {
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
				const tinygltf::Mesh &mesh = model.meshes[gNode.mesh];

				for (size_t i = 0; i < mesh.primitives.size(); i++) {
					const tinygltf::Primitive &gPrimitive = mesh.primitives[i];

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
			} else {
				nodes.push_back(node);
			}
		}

		void drawNode(VkCommandBuffer commandBuffer, uint32_t i, Node &node) {
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
						MaterialInstance &materialInstance = materialInstances[primitive.materialIndex];
						materialInstance.material->bind(commandBuffer);
						materialInstance.bind(commandBuffer, i);

						vkCmdPushConstants(commandBuffer, materialInstance.material->pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &nodeMatrix);
	
						vkCmdDrawIndexed(commandBuffer, primitive.indexCount, 1, primitive.firstIndex, 0, 0);
					}
				}
			}
			for (auto& child : node.children) {
				drawNode(commandBuffer, i, child);
			}
		}

		void draw(VkCommandBuffer commandBuffer, uint32_t i)
		{
			VkDeviceSize offsets[1] = { 0 };
			vkCmdBindVertexBuffers(commandBuffer, 0, 1, &meshBuf.vBuffer, offsets);
			vkCmdBindIndexBuffer(commandBuffer, meshBuf.iBuffer, 0, VK_INDEX_TYPE_UINT32);
			for (auto& node : nodes) {
				drawNode(commandBuffer, i, node);
			}
		}

		void destroy(VkDescriptorPool descriptorPool) {
			for (Texture &texture : textures) {
				vku::image::destroyTexture(vku::state::device, texture);
			}
			for (Material &material : materials) {
				material.destroy(descriptorPool);
			}
			meshBuf.destroy();
		}
	};
}