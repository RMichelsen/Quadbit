#include "ResourceManager.h"

#include <EASTL/algorithm.h>

#include <filesystem>
#include <glm/gtc/type_ptr.hpp>

#include "Engine/Rendering/VulkanUtils.h"

namespace Quadbit {
	PerFrameTransfers::PerFrameTransfers(const QbVkContext& context) : count(0) {
		commandBuffer = VkUtils::CreatePersistentCommandBuffer(context);
	}

	QbVkResourceManager::QbVkResourceManager(QbVkContext& context) : context_(context), transferQueue_(PerFrameTransfers(context)) {
		// Create descriptor pool and layout
		VkDescriptorPoolSize poolSize = VkUtils::Init::DescriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, MAX_DESCRIPTORSET_COUNT);

		VkDescriptorPoolCreateInfo poolInfo{};
		poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.poolSizeCount = 1;
		poolInfo.pPoolSizes = &poolSize;
		poolInfo.maxSets = MAX_DESCRIPTORSET_COUNT;

		VK_CHECK(vkCreateDescriptorPool(context_.device, &poolInfo, nullptr, &materialDescriptorPool_));


		std::vector<VkDescriptorSetLayoutBinding> descSetLayoutBindings;
		descSetLayoutBindings.push_back(VkUtils::Init::DescriptorSetLayoutBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT));
		descSetLayoutBindings.push_back(VkUtils::Init::DescriptorSetLayoutBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT));
		descSetLayoutBindings.push_back(VkUtils::Init::DescriptorSetLayoutBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT));
		descSetLayoutBindings.push_back(VkUtils::Init::DescriptorSetLayoutBinding(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT));
		descSetLayoutBindings.push_back(VkUtils::Init::DescriptorSetLayoutBinding(4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT));

		VkDescriptorSetLayoutCreateInfo descSetLayoutCreateInfo = VkUtils::Init::DescriptorSetLayoutCreateInfo();
		descSetLayoutCreateInfo.pBindings = descSetLayoutBindings.data();
		descSetLayoutCreateInfo.bindingCount = static_cast<uint32_t>(descSetLayoutBindings.size());

		VK_CHECK(vkCreateDescriptorSetLayout(context_.device, &descSetLayoutCreateInfo, nullptr, &materialDescriptorSetLayout_));

		// Create an empty texture for use when a material has empty slots
		auto samplerInfo = VkUtils::Init::SamplerCreateInfo(VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_TRUE, 16.0f, VK_COMPARE_OP_ALWAYS, VK_SAMPLER_MIPMAP_MODE_LINEAR);
		emptyTexture_ = CreateTexture(1, 1, &samplerInfo);
	}

	QbVkResourceManager::~QbVkResourceManager() {
		// Destroy staging buffers...
		for (auto& stagingBuffer : transferQueue_.stagingBuffers) {
			// Simply break when we reach the end of the buffers
			if (stagingBuffer.buf == VK_NULL_HANDLE) break;
			context_.allocator->DestroyBuffer(stagingBuffer);
		}
		// Destroy regular GPU buffers
		for (uint16_t i = 0; i < buffers_.resourceIndex; i++) {
			if (eastl::find(buffers_.freeList.begin(), buffers_.freeList.end(), i) == buffers_.freeList.end()) {
				DestroyResource<QbVkBuffer>(buffers_.GetHandle(i));
			}
		}
		// Destroy textures...
		for (uint16_t i = 0; i < textures_.resourceIndex; i++) {
			if (eastl::find(textures_.freeList.begin(), textures_.freeList.end(), i) == textures_.freeList.end()) {
				DestroyResource<QbVkTexture>(textures_.GetHandle(i));
			}
		}
		// Destroy descriptor pool and layout
		vkDestroyDescriptorSetLayout(context_.device, materialDescriptorSetLayout_, nullptr);
		vkDestroyDescriptorPool(context_.device, materialDescriptorPool_, nullptr);
	}

	QbVkBufferHandle QbVkResourceManager::CreateGPUBuffer(VkDeviceSize size, VkBufferUsageFlags bufferUsage, QbVkMemoryUsage memoryUsage) {
		auto bufferInfo = VkUtils::Init::BufferCreateInfo(size, bufferUsage);
		auto handle = buffers_.GetNextHandle();

		context_.allocator->CreateBuffer(buffers_[handle], bufferInfo, memoryUsage);
		buffers_[handle].descriptor = { buffers_[handle].buf, 0, VK_WHOLE_SIZE };
		return handle;
	}

	void QbVkResourceManager::TransferDataToGPU(const void* data, VkDeviceSize size, QbVkBufferHandle destination) {
		// If its the first transfer of the frame, we destroy the staging buffers from the previous transfer frame
		if (transferQueue_.count == 0) {
			for (auto& stagingBuffer : transferQueue_.stagingBuffers) {
				// Simply break when we reach the end of the buffers
				if (stagingBuffer.buf == VK_NULL_HANDLE) break;
				context_.allocator->DestroyBuffer(stagingBuffer);
			}
		}

		context_.allocator->CreateStagingBuffer(transferQueue_.stagingBuffers[transferQueue_.count], size, data);
		transferQueue_.transfers[transferQueue_.count] = { size, destination };
		transferQueue_.count++;
	}

	QbVkBufferHandle QbVkResourceManager::CreateVertexBuffer(const void* vertices, uint32_t vertexStride, uint32_t vertexCount) {
		VkDeviceSize bufferSize = static_cast<uint64_t>(vertexCount)* vertexStride;

		auto handle = CreateGPUBuffer(bufferSize,
			VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, QbVkMemoryUsage::QBVK_MEMORY_USAGE_GPU_ONLY);
		TransferDataToGPU(vertices, bufferSize, handle);
		return handle;
	}

	QbVkBufferHandle QbVkResourceManager::CreateIndexBuffer(const eastl::vector<uint32_t>& indices) {
		VkDeviceSize bufferSize = static_cast<uint32_t>(indices.size()) * sizeof(uint32_t);

		auto handle = CreateGPUBuffer(bufferSize, 
			VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, QbVkMemoryUsage::QBVK_MEMORY_USAGE_GPU_ONLY);
		TransferDataToGPU(indices.data(), bufferSize, handle);
		return handle;
	}

	QbVkTextureHandle QbVkResourceManager::CreateStorageTexture(uint32_t width, uint32_t height, VkFormat format, VkSamplerCreateInfo* samplerInfo) {
		auto handle = textures_.GetNextHandle();
		auto& texture = textures_[handle];

		auto imageCreateInfo = VkUtils::Init::ImageCreateInfo(width, height, format, 
			VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_SAMPLE_COUNT_1_BIT);
		context_.allocator->CreateImage(texture.image, imageCreateInfo, QbVkMemoryUsage::QBVK_MEMORY_USAGE_GPU_ONLY);
		texture.descriptor.imageView = VkUtils::CreateImageView(context_, texture.image.imgHandle, format, VK_IMAGE_ASPECT_COLOR_BIT);
		texture.descriptor.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		if (samplerInfo != nullptr) VK_CHECK(vkCreateSampler(context_.device, samplerInfo, nullptr, &texture.descriptor.sampler));

		// Transition the image layout to the desired layout
		VkUtils::TransitionImageLayout(context_, texture.image.imgHandle, VK_IMAGE_ASPECT_COLOR_BIT, 
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

		return handle;
	}

	QbVkTextureHandle QbVkResourceManager::CreateTexture(uint32_t width, uint32_t height, VkSamplerCreateInfo* samplerInfo) {
		auto handle = textures_.GetNextHandle();
		auto& texture = textures_[handle];

		auto imageCreateInfo = VkUtils::Init::ImageCreateInfo(width, height, VK_FORMAT_R8G8B8A8_UNORM, 
			VK_IMAGE_TILING_LINEAR, VK_IMAGE_USAGE_SAMPLED_BIT, VK_SAMPLE_COUNT_1_BIT);
		context_.allocator->CreateImage(texture.image, imageCreateInfo, QbVkMemoryUsage::QBVK_MEMORY_USAGE_GPU_ONLY);
		texture.descriptor.imageView = VkUtils::CreateImageView(context_, texture.image.imgHandle, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT);
		texture.descriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		if (samplerInfo != nullptr) VK_CHECK(vkCreateSampler(context_.device, samplerInfo, nullptr, &texture.descriptor.sampler));

		// Transition the image layout to the desired layout
		VkUtils::TransitionImageLayout(context_, texture.image.imgHandle, VK_IMAGE_ASPECT_COLOR_BIT, 
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_HOST_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

		return handle;
	}

	QbVkTextureHandle QbVkResourceManager::LoadTexture(uint32_t width, uint32_t height, const void* data, VkSamplerCreateInfo* samplerInfo) {
		auto handle = textures_.GetNextHandle();
		auto& texture = textures_[handle];

		VkDeviceSize size = static_cast<uint64_t>(width)* static_cast<uint64_t>(height) * 4;

		auto imageCreateInfo = VkUtils::Init::ImageCreateInfo(width, height, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_SAMPLE_COUNT_1_BIT);
		context_.allocator->CreateImage(texture.image, imageCreateInfo, QbVkMemoryUsage::QBVK_MEMORY_USAGE_GPU_ONLY);

		// Create buffer and copy data
		QbVkBuffer buffer;
		context_.allocator->CreateStagingBuffer(buffer, size, data);

		// Transition the image layout to the desired layout
		VkUtils::TransitionImageLayout(context_, texture.image.imgHandle, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

		// Copy the pixel data to the image
		VkUtils::CopyBufferToImage(context_, buffer.buf, texture.image.imgHandle, width, height);

		// Transition the image to be read in shader
		VkUtils::TransitionImageLayout(context_, texture.image.imgHandle, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

		texture.descriptor.imageView = VkUtils::CreateImageView(context_, texture.image.imgHandle, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT);
		texture.descriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		if (samplerInfo != nullptr) VK_CHECK(vkCreateSampler(context_.device, samplerInfo, nullptr, &texture.descriptor.sampler));

		// Destroy the buffer and free the image
		context_.allocator->DestroyBuffer(buffer);

		return handle;
	}

	QbVkTextureHandle QbVkResourceManager::LoadTexture(const char* imagePath, VkSamplerCreateInfo* samplerInfo) {
		int width, height, channels;
		stbi_uc* pixels = stbi_load(imagePath, &width, &height, &channels, STBI_rgb_alpha);
		assert(pixels != nullptr);

		auto handle = LoadTexture(width, height, pixels, samplerInfo);

		stbi_image_free(pixels);
		return handle;
	}

	PBRModelComponent QbVkResourceManager::LoadModel(const char* path) {

		auto fsPath = std::filesystem::path(path);
		eastl::string extension = eastl::string(fsPath.extension().string().c_str());
		eastl::string directoryPath = eastl::string(fsPath.remove_filename().string().c_str());
		PBRModelComponent model;

		std::string err, warn;
		tinygltf::TinyGLTF loader;
		tinygltf::Model gltfModel;

		bool loaded = false;
		if (extension.compare(".gltf") == 0) {
			loaded = loader.LoadASCIIFromFile(&gltfModel, &err, &warn, path);
		}
		else if (extension.compare(".glb") == 0) {
			loaded = loader.LoadBinaryFromFile(&gltfModel, &err, &warn, path);
		}
		if (!loaded) {
			QB_LOG_ERROR("Failed to load model at %s\n", path);
		}

		// Parse materials
		eastl::vector<QbVkMaterial> materials;
		for (const auto& mat : gltfModel.materials) {
			QbVkMaterial material = ParseMaterial(gltfModel, mat);
			materials.push_back(material);
		}

		eastl::vector<QbVkVertex> vertices;
		eastl::vector<uint32_t> indices;

		// Parse buffers
		const auto& buffers = gltfModel.buffers;
		for (const auto& mesh : gltfModel.meshes) {
			for (const auto& primitive : mesh.primitives) {
				assert(primitive.attributes.find("POSITION") != primitive.attributes.end());
				assert(primitive.mode == TINYGLTF_MODE_TRIANGLES);

				const auto vertexCount = gltfModel.accessors[primitive.attributes.find("POSITION")->second].count;

				const auto vertexOffset = static_cast<uint32_t>(vertices.size());
				const auto indexOffset = static_cast<uint32_t>(indices.size());

				int positionStride = 0;
				int normalStride = 0;
				int uv0Stride = 0;
				int uv1Stride = 0;
				const float* positions = nullptr;
				const float* normals = nullptr;
				const float* uvs0 = nullptr;
				const float* uvs1 = nullptr;

				for (const auto& [name, index] : primitive.attributes) {
					const auto& accessor = gltfModel.accessors[index];
					const auto& bufferView = gltfModel.bufferViews[accessor.bufferView];
					const auto& buffer = gltfModel.buffers[bufferView.buffer];

					if (name.compare("POSITION") == 0) {
						positions = reinterpret_cast<const float*>(&buffer.data[accessor.byteOffset + bufferView.byteOffset]);
						positionStride = accessor.ByteStride(bufferView) / sizeof(float);
						assert(positions != nullptr && positionStride > 0);
					}
					else if (name.compare("NORMAL") == 0) {
						normals = reinterpret_cast<const float*>(&buffer.data[accessor.byteOffset + bufferView.byteOffset]);
						normalStride = accessor.ByteStride(bufferView) / sizeof(float);
						assert(normals != nullptr && normalStride > 0);
					}
					else if (name.compare("TEXCOORD_0") == 0) {
						uvs0 = reinterpret_cast<const float*>(&buffer.data[accessor.byteOffset + bufferView.byteOffset]);
						uv0Stride = accessor.ByteStride(bufferView) / sizeof(float);
						assert(uvs0 != nullptr && uv0Stride > 0);
					}
					else if (name.compare("TEXCOORD_1") == 0) {
						uvs1 = reinterpret_cast<const float*>(&buffer.data[accessor.byteOffset + bufferView.byteOffset]);
						uv1Stride = accessor.ByteStride(bufferView) / sizeof(float);
						assert(uvs1 != nullptr && uv1Stride > 0);
					}
				}

				for (int i = 0; i < vertexCount; i++) {
					QbVkVertex vertex{};
					vertex.position = glm::make_vec3(&positions[i * positionStride]);
					if (normals != nullptr) vertex.normal = glm::normalize(glm::make_vec3(&normals[i * normalStride]));
					if (uvs0 != nullptr) vertex.uv0 = glm::make_vec2(&uvs0[i * uv0Stride]);
					if (uvs1 != nullptr) vertex.uv1 = glm::make_vec2(&uvs1[i * uv1Stride]);
					vertices.push_back(vertex);
				}

				// TODO: Fallback to draw without indexes if not available
				assert(primitive.indices > -1);

				const auto& accessor = gltfModel.accessors[primitive.indices];
				const auto& bufferView = gltfModel.bufferViews[accessor.bufferView];
				const auto& buffer = gltfModel.buffers[bufferView.buffer];
				const auto indexCount = accessor.count;
				
				assert((accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE ||
					accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT ||
					accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)
					&& "Indexbuffer type not supported, supported types are uint8, uint16, uint32");

				const void* indexBuffer = &buffer.data[accessor.byteOffset + bufferView.byteOffset];
				if (accessor.componentType == TINYGLTF_PARAMETER_TYPE_UNSIGNED_BYTE) {
					const uint8_t* buf = static_cast<const uint8_t*>(indexBuffer);
					for (auto i = 0; i < indexCount; i++) {
						indices.push_back(buf[i]);
					}
				}
				else if (accessor.componentType == TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT) {
					const uint16_t* buf = static_cast<const uint16_t*>(indexBuffer);
					for (auto i = 0; i < indexCount; i++) {
						indices.push_back(buf[i]);
					}
				}
				else if (accessor.componentType == TINYGLTF_PARAMETER_TYPE_UNSIGNED_INT) {
					const uint32_t* buf = static_cast<const uint32_t*>(indexBuffer);
					for (auto i = 0; i < indexCount; i++) {
						indices.push_back(buf[i]);
					}
				}

				QbVkPBRMesh mesh;
				mesh.vertexOffset = vertexOffset;
				mesh.indexOffset = indexOffset;
				mesh.indexCount = static_cast<uint32_t>(indices.size()) - indexOffset;
				mesh.material = materials[primitive.material];
				model.meshes.push_back(mesh);
			}
		}

		model.vertexHandle = CreateVertexBuffer(vertices.data(), sizeof(QbVkVertex), static_cast<uint32_t>(vertices.size()));
		model.indexHandle = CreateIndexBuffer(indices);

		return model;
	}

	bool QbVkResourceManager::TransferQueuedDataToGPU(uint32_t resourceIndex) {
		if (transferQueue_.count == 0) return false;

		VkCommandBufferBeginInfo commandBufferInfo = VkUtils::Init::CommandBufferBeginInfo();
		commandBufferInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		VK_CHECK(vkBeginCommandBuffer(transferQueue_.commandBuffer, &commandBufferInfo));

		for (uint32_t i = 0; i < transferQueue_.count; i++) {
			// Issue copy command
			VkBufferCopy copyRegion{};
			copyRegion.srcOffset = 0;
			copyRegion.dstOffset = 0;
			copyRegion.size = transferQueue_[i].size;
			vkCmdCopyBuffer(transferQueue_.commandBuffer, transferQueue_.stagingBuffers[i].buf, buffers_[transferQueue_[i].destinationBuffer].buf, 1, &copyRegion);
		}

		// End recording
		VK_CHECK(vkEndCommandBuffer(transferQueue_.commandBuffer));

		VkSubmitInfo submitInfo = VkUtils::Init::SubmitInfo();
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &transferQueue_.commandBuffer;
		submitInfo.signalSemaphoreCount = 1;
		eastl::array<VkSemaphore, 1> transferSemaphore{ context_.renderingResources[resourceIndex].transferSemaphore };
		submitInfo.pSignalSemaphores = transferSemaphore.data();

		// Submit to queue
		VK_CHECK(vkQueueSubmit(context_.graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE));

		transferQueue_.Reset();
		return true;
	}
	
	VkSamplerCreateInfo QbVkResourceManager::GetSamplerInfo(const tinygltf::Model& model, int samplerIndex) {
		VkSamplerCreateInfo samplerCreateInfo{};
		samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
		samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
		samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerCreateInfo.anisotropyEnable = VK_TRUE;
		samplerCreateInfo.maxAnisotropy = 16.0f;
		samplerCreateInfo.unnormalizedCoordinates = VK_FALSE;
		samplerCreateInfo.compareEnable = VK_FALSE;
		samplerCreateInfo.compareOp = VK_COMPARE_OP_ALWAYS;
		samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerCreateInfo.maxLod = 0.0f;

		if (samplerIndex > -1) {
			auto& sampler = model.samplers[samplerIndex];
			// Hardcoded conversiosn from OpenGL constants...
			samplerCreateInfo.magFilter = (sampler.magFilter == 9728 || sampler.magFilter == 9984 || sampler.magFilter == 9986) ? VK_FILTER_NEAREST : VK_FILTER_LINEAR;
			samplerCreateInfo.minFilter = (sampler.minFilter == 9728 || sampler.minFilter == 9984 || sampler.minFilter == 9986) ? VK_FILTER_NEAREST : VK_FILTER_LINEAR;
			switch (sampler.wrapS) {
			case 10497: { samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT; break; }
			case 33071: { samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE; break; }
			case 33648: { samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT; break; }
			}
			switch (sampler.wrapT) {
			case 10497: { samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT; break; }
			case 33071: { samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE; break; }
			case 33648: { samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT; break; }
			}
			samplerCreateInfo.addressModeW = samplerCreateInfo.addressModeV;
		}

		return samplerCreateInfo;
	}

	QbVkDescriptorSetHandle QbVkResourceManager::GetDescriptorSetHandle(QbVkMaterial& material) {
		std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, materialDescriptorSetLayout_);
		VkDescriptorSetAllocateInfo descSetallocInfo = VkUtils::Init::DescriptorSetAllocateInfo();
		descSetallocInfo.descriptorPool = materialDescriptorPool_;
		descSetallocInfo.descriptorSetCount = MAX_FRAMES_IN_FLIGHT;
		descSetallocInfo.pSetLayouts = layouts.data();

		auto descriptorHandle = materialDescriptorSets_.GetNextHandle();
		auto& descriptorSet = materialDescriptorSets_[descriptorHandle];

		VK_CHECK(vkAllocateDescriptorSets(context_.device, &descSetallocInfo, &descriptorSet));

		std::vector<VkWriteDescriptorSet> writeDescSets;

		auto& baseColorDescriptor = (material.textureIndices.baseColorTextureIndex > -1) ?
			textures_[material.baseColorTexture].descriptor :
			textures_[emptyTexture_].descriptor;

		auto& metallicRoughnessDescriptor = (material.textureIndices.metallicRoughnessTextureIndex > -1) ?
			textures_[material.metallicRoughnessTexture].descriptor : 
			textures_[emptyTexture_].descriptor;

		auto& normalDescriptor = (material.textureIndices.normalTextureIndex > -1) ?
			textures_[material.normalTexture].descriptor :
			textures_[emptyTexture_].descriptor;

		auto& occlusionDescriptor = (material.textureIndices.occlusionTextureIndex > -1) ?
			textures_[material.occlusionTexture].descriptor :
			textures_[emptyTexture_].descriptor;

		auto& emissiveDescriptor = (material.textureIndices.emissiveTextureIndex > -1) ?
			textures_[material.emissiveTexture].descriptor :
			textures_[emptyTexture_].descriptor;

		writeDescSets.push_back(VkUtils::Init::WriteDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, &baseColorDescriptor));
		writeDescSets.push_back(VkUtils::Init::WriteDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &metallicRoughnessDescriptor));
		writeDescSets.push_back(VkUtils::Init::WriteDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2, &normalDescriptor));
		writeDescSets.push_back(VkUtils::Init::WriteDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3, &occlusionDescriptor));
		writeDescSets.push_back(VkUtils::Init::WriteDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4, &emissiveDescriptor));
		vkUpdateDescriptorSets(context_.device, static_cast<uint32_t>(writeDescSets.size()), writeDescSets.data(), 0, nullptr);

		return descriptorHandle;
	}

	QbVkTextureHandle QbVkResourceManager::CreateTextureFromResource(const tinygltf::Model& model, const tinygltf::Material& material, const char* textureName) {
		auto& texture = model.textures[material.values.at(textureName).TextureIndex()];
		auto& image = model.images[texture.source];

		auto samplerInfo = GetSamplerInfo(model, texture.sampler);
		return LoadTexture(image.width, image.height, image.image.data(), &samplerInfo);
	}

	QbVkMaterial QbVkResourceManager::ParseMaterial(const tinygltf::Model& model, const tinygltf::Material& material) {
		QbVkMaterial mat{};
		if (material.values.find("baseColorTexture") != material.values.end()) {
			mat.baseColorTexture = CreateTextureFromResource(model, material, "baseColorTexture");
			mat.textureIndices.baseColorTextureIndex = material.values.at("baseColorTexture").TextureTexCoord();
		}
		if (material.values.find("metallicRoughnessTexture") != material.values.end()) {
			mat.metallicRoughnessTexture = CreateTextureFromResource(model, material, "metallicRoughnessTexture");
			mat.textureIndices.metallicRoughnessTextureIndex = material.values.at("metallicRoughnessTexture").TextureTexCoord();
		}
		if (material.values.find("normalTexture") != material.values.end()) {
			mat.normalTexture = CreateTextureFromResource(model, material, "normalTexture");
			mat.textureIndices.normalTextureIndex = material.values.at("normalTexture").TextureTexCoord();
		}
		if (material.values.find("occlusionTexture") != material.values.end()) {
			mat.occlusionTexture = CreateTextureFromResource(model, material, "occlusionTexture");
			mat.textureIndices.occlusionTextureIndex = material.values.at("occlusionTexture").TextureTexCoord();
		}
		if (material.values.find("emissiveTexture") != material.values.end()) {
			mat.occlusionTexture = CreateTextureFromResource(model, material, "emissiveTexture");
			mat.textureIndices.occlusionTextureIndex = material.values.at("emissiveTexture").TextureTexCoord();
		}
		if (material.values.find("baseColorFactor") != material.values.end()) {
			mat.baseColorFactor = glm::make_vec4(material.values.at("baseColorFactor").ColorFactor().data());
		}
		if (material.values.find("emissiveFactor") != material.values.end()) {
			mat.emissiveFactor = glm::vec4(glm::make_vec3(material.values.at("emissiveFactor").ColorFactor().data()), 1.0f);
		}
		if (material.values.find("metallicFactor") != material.values.end()) {
			mat.metallicFactor = static_cast<float>(material.values.at("metallicFactor").Factor());
		}
		if (material.values.find("roughnessFactor") != material.values.end()) {
			mat.roughnessFactor = static_cast<float>(material.values.at("roughnessFactor").Factor());
		}
		mat.descriptorHandle = GetDescriptorSetHandle(mat);
		return mat;
	}
}