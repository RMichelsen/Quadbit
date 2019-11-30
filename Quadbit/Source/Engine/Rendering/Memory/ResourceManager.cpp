#include "ResourceManager.h"

#include <EASTL/algorithm.h>

#include <stb/stb_image.h>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

#include "Engine/Rendering/VulkanUtils.h"
#include "Engine/Rendering/Memory/Allocator.h"
#include "Engine/Core/Logging.h"

namespace Quadbit {
	PerFrameTransfers::PerFrameTransfers(const QbVkContext& context) : count(0) {
		commandBuffer = VkUtils::CreatePersistentCommandBuffer(context);
	}

	QbVkResourceManager::QbVkResourceManager(QbVkContext& context) : context_(context), transferQueue_(PerFrameTransfers(context)) {}

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
		// Destroy descriptor set allocators
		for (uint16_t i = 0; i < descriptorAllocators_.resourceIndex; i++) {
			if (eastl::find(descriptorAllocators_.freeList.begin(), descriptorAllocators_.freeList.end(), i) == descriptorAllocators_.freeList.end()) {
				DestroyResource<QbVkDescriptorAllocator>(descriptorAllocators_.GetHandle(i));
			}
		}

		vkFreeCommandBuffers(context_.device, context_.commandPool, 1, &transferQueue_.commandBuffer);
	}

	QbVkPipelineHandle QbVkResourceManager::CreateGraphicsPipeline(const char* vertexPath, const char* vertexEntry, 
		const char* fragmentPath, const char* fragmentEntry, const QbVkPipelineDescription pipelineDescription, 
		const VkRenderPass renderPass, const uint32_t maxInstances, const eastl::vector<eastl::tuple<VkFormat, uint32_t>>& vertexAttributeOverride) {
		
		auto handle = pipelines_.GetNextHandle();
		pipelines_[handle] = eastl::make_unique<QbVkPipeline>(context_, vertexPath, vertexEntry, fragmentPath, fragmentEntry,
			pipelineDescription, renderPass, maxInstances, vertexAttributeOverride);
		return handle;
	}

	QbVkPipelineHandle QbVkResourceManager::CreateComputePipeline(const char* computePath, const char* computeEntry, 
		const void* specConstants, const uint32_t maxInstances) {
		
		auto handle = pipelines_.GetNextHandle();
		pipelines_[handle] = eastl::make_unique<QbVkPipeline>(context_, computePath, computeEntry, specConstants, maxInstances);
		return handle;
	}

	void QbVkResourceManager::RebuildPipelines() {
		// Rebuild all active pipelines
		for (uint16_t i = 0; i < pipelines_.resourceIndex; i++) {
			if (eastl::find(pipelines_.freeList.begin(), pipelines_.freeList.end(), i) == pipelines_.freeList.end()) {
				pipelines_[pipelines_.GetHandle(i)]->Rebuild();
			}
		}
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

	QbVkBufferHandle QbVkResourceManager::CreateGPUBuffer(VkDeviceSize size, VkBufferUsageFlags bufferUsage, QbVkMemoryUsage memoryUsage) {
		auto bufferInfo = VkUtils::Init::BufferCreateInfo(size, bufferUsage);
		auto handle = buffers_.GetNextHandle();

		context_.allocator->CreateBuffer(buffers_[handle], bufferInfo, memoryUsage);
		buffers_[handle].descriptor = { buffers_[handle].buf, 0, VK_WHOLE_SIZE };
		return handle;
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

	QbVkTextureHandle QbVkResourceManager::CreateTexture(uint32_t width, uint32_t height, VkSamplerCreateInfo* samplerInfo) {
		auto handle = textures_.GetNextHandle();
		auto& texture = textures_[handle];

		auto imageInfo = VkUtils::Init::ImageCreateInfo(width, height, VK_FORMAT_R8G8B8A8_UNORM, 
			VK_IMAGE_TILING_LINEAR, VK_IMAGE_USAGE_SAMPLED_BIT, VK_SAMPLE_COUNT_1_BIT);
		context_.allocator->CreateImage(texture.image, imageInfo, QbVkMemoryUsage::QBVK_MEMORY_USAGE_GPU_ONLY);
		texture.descriptor.imageView = VkUtils::CreateImageView(context_, texture.image.imgHandle, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT);
		texture.descriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		if (samplerInfo != nullptr) VK_CHECK(vkCreateSampler(context_.device, samplerInfo, nullptr, &texture.descriptor.sampler));

		// Transition the image layout to the desired layout
		VkUtils::TransitionImageLayout(context_, texture.image.imgHandle, VK_IMAGE_ASPECT_COLOR_BIT, 
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_HOST_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

		return handle;
	}

	QbVkTextureHandle QbVkResourceManager::CreateTexture(VkImageCreateInfo* imageInfo, VkImageAspectFlags aspectFlags, 
		VkImageLayout finalLayout, VkSamplerCreateInfo* samplerInfo) {

		auto handle = textures_.GetNextHandle();
		auto& texture = textures_[handle];

		context_.allocator->CreateImage(texture.image, *imageInfo, QbVkMemoryUsage::QBVK_MEMORY_USAGE_GPU_ONLY);
		texture.descriptor.imageView = VkUtils::CreateImageView(context_, texture.image.imgHandle, imageInfo->format, aspectFlags);
		texture.descriptor.imageLayout = finalLayout;
		if (samplerInfo != nullptr) VK_CHECK(vkCreateSampler(context_.device, samplerInfo, nullptr, &texture.descriptor.sampler));

		// Transition the image layout to the desired layout
		VkUtils::TransitionImageLayout(context_, texture.image.imgHandle, aspectFlags,
			VK_IMAGE_LAYOUT_UNDEFINED, finalLayout, VK_PIPELINE_STAGE_HOST_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

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
		QB_ASSERT(pixels != nullptr);

		auto handle = LoadTexture(width, height, pixels, samplerInfo);

		stbi_image_free(pixels);
		return handle;
	}

	QbVkTextureHandle QbVkResourceManager::GetEmptyTexture() {
		if (emptyTexture_ != QBVK_TEXTURE_NULL_HANDLE) return emptyTexture_;

		// Create an empty texture for use when a material has empty slots
		auto samplerInfo = VkUtils::Init::SamplerCreateInfo(VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_TRUE, 16.0f, VK_COMPARE_OP_ALWAYS, VK_SAMPLER_MIPMAP_MODE_LINEAR);
		emptyTexture_ = CreateTexture(1, 1, &samplerInfo);
		return emptyTexture_;
	}

	// Max instances here refers to the maximum number shader resource instances
	QbVkDescriptorAllocatorHandle QbVkResourceManager::CreateDescriptorAllocator(const eastl::vector<VkDescriptorSetLayout>& setLayouts,
		const eastl::vector<VkDescriptorPoolSize>& poolSizes, const uint32_t maxInstances) {

		auto allocatorHandle = descriptorAllocators_.GetNextHandle();
		auto& allocator = descriptorAllocators_[allocatorHandle];

		auto setCount = static_cast<uint32_t>(setLayouts.size());
		auto poolInfo = VkUtils::Init::DescriptorPoolCreateInfo(poolSizes, setCount * MAX_FRAMES_IN_FLIGHT * maxInstances);
		VK_CHECK(vkCreateDescriptorPool(context_.device, &poolInfo, nullptr, &allocator.pool));

		// We will now allocate the maximum number of descriptor sets up front
		// so when the user starts requesting descriptor sets to be written we 
		// hand out handles of pre-allocated descriptor sets
		for (uint32_t i = 0; i < maxInstances; i++) {
			auto& sets = allocator.setInstances.elements[i];
			eastl::vector<VkDescriptorSetLayout> layouts;

			for (const auto& setLayout : setLayouts) {
				layouts.push_back(setLayout);
			}

			sets.resize(setCount);
			VkDescriptorSetAllocateInfo descSetallocInfo = VkUtils::Init::DescriptorSetAllocateInfo();
			descSetallocInfo.descriptorPool = allocator.pool;
			descSetallocInfo.descriptorSetCount = setCount;
			descSetallocInfo.pSetLayouts = layouts.data();
			VK_CHECK(vkAllocateDescriptorSets(context_.device, &descSetallocInfo, sets.data()));

			//for (int j = 0; j < MAX_FRAMES_IN_FLIGHT; j++) {
			//	sets[j].resize(setCount);
			//	VkDescriptorSetAllocateInfo descSetallocInfo = VkUtils::Init::DescriptorSetAllocateInfo();
			//	descSetallocInfo.descriptorPool = allocator.pool;
			//	descSetallocInfo.descriptorSetCount = setCount;
			//	descSetallocInfo.pSetLayouts = layouts.data();
			//	VK_CHECK(vkAllocateDescriptorSets(context_.device, &descSetallocInfo, sets[j].data()));
			//}
		}

		return allocatorHandle;
	}

	QbVkDescriptorSetsHandle QbVkResourceManager::GetNextDescriptorSetsHandle(const QbVkDescriptorAllocatorHandle allocatorHandle) {
		auto& allocator = descriptorAllocators_[allocatorHandle];
		return allocator.setInstances.GetNextHandle();
	}

	const eastl::vector<VkDescriptorSet>& QbVkResourceManager::GetDescriptorSets(
		const QbVkDescriptorAllocatorHandle allocatorHandle, const QbVkDescriptorSetsHandle setsHandle) {
		return descriptorAllocators_[allocatorHandle].setInstances[setsHandle];
	}

	void QbVkResourceManager::WriteDescriptor(QbVkDescriptorAllocatorHandle allocatorHandle, QbVkDescriptorSetsHandle descriptorSetsHandle, 
		QbVkBufferHandle bufferHandle, VkDescriptorType descriptorType, uint32_t set, uint32_t binding, uint32_t descriptorCount) {

		auto& allocator = descriptorAllocators_[allocatorHandle];
		auto& descriptorSets = allocator.setInstances[descriptorSetsHandle];

		eastl::array<VkWriteDescriptorSet, 1> writeDescSets = {
			VkUtils::Init::WriteDescriptorSet(descriptorSets[set], binding, descriptorType,
			&buffers_[bufferHandle].descriptor, descriptorCount)
		};
		vkUpdateDescriptorSets(context_.device, static_cast<uint32_t>(writeDescSets.size()), writeDescSets.data(), 0, nullptr);
	}

	void QbVkResourceManager::WriteDescriptor(QbVkDescriptorAllocatorHandle allocatorHandle, QbVkDescriptorSetsHandle descriptorSetsHandle, 
		QbVkTextureHandle textureHandle, VkDescriptorType descriptorType, uint32_t set, uint32_t binding, uint32_t descriptorCount) {

		auto& allocator = descriptorAllocators_[allocatorHandle];
		auto& descriptorSets = allocator.setInstances[descriptorSetsHandle];

		eastl::array<VkWriteDescriptorSet, 1> writeDescSets = {
			VkUtils::Init::WriteDescriptorSet(descriptorSets[set], binding, descriptorType,
			&textures_[textureHandle].descriptor, descriptorCount)
		};
		vkUpdateDescriptorSets(context_.device, static_cast<uint32_t>(writeDescSets.size()), writeDescSets.data(), 0, nullptr);
	}
	uint32_t QbVkResourceManager::GetUniformBufferAlignment(uint32_t structSize) {
		return VkUtils::GetDynamicUBOAlignment(context_, structSize);
	}

	QbVkBufferHandle QbVkResourceManager::CreateUniformBuffer(uint32_t alignedSize) {
		return CreateGPUBuffer(alignedSize * MAX_FRAMES_IN_FLIGHT, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, QbVkMemoryUsage::QBVK_MEMORY_USAGE_CPU_TO_GPU);
	}
	void* QbVkResourceManager::GetMappedGPUData(QbVkBufferHandle handle) {
		QB_ASSERT(buffers_[handle].alloc.data != nullptr && "GPUBuffer data not mapped!");
		return buffers_[handle].alloc.data;
	}
}