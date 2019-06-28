#include <PCH.h>

#include "Water.h"
#include "../Engine/Rendering/Common/QbVkUtils.h"

void Water::Init() {
	// Initialize vertices and indices for the water mesh
	waterVertices_.resize(WATER_RESOLUTION * WATER_RESOLUTION);
	waterIndices_.resize((WATER_RESOLUTION - 1) * (WATER_RESOLUTION - 1) * 6);
	int vertexCount = 0;
	int indexCount = 0;
	for (int x = 0; x < WATER_RESOLUTION; x++) {
		for (int z = 0; z < WATER_RESOLUTION; z++) {
			waterVertices_[vertexCount] = { {x, 0, z}, {0.1f, 0.5f, 0.7f} };
			if (x < WATER_RESOLUTION - 1 && z < WATER_RESOLUTION - 1) {
				waterIndices_[indexCount++] = vertexCount;
				waterIndices_[indexCount++] = vertexCount + WATER_RESOLUTION;
				waterIndices_[indexCount++] = vertexCount + WATER_RESOLUTION + 1;
				waterIndices_[indexCount++] = vertexCount;
				waterIndices_[indexCount++] = vertexCount + WATER_RESOLUTION + 1;
				waterIndices_[indexCount++] = vertexCount + 1;
			}
			vertexCount++;
		}
	}

	// Setup entities
	auto entityManager = Quadbit::EntityManager::GetOrCreate();
	auto entity = entityManager->Create();
	entity.AddComponent<Quadbit::RenderMeshComponent>(renderer_->CreateMesh(waterVertices_, waterIndices_));
	entity.AddComponent<Quadbit::RenderTransformComponent>(Quadbit::RenderTransformComponent(1.0f, { 0.0f, 0.0f, 0.0f }, { 0, 0, 0, 1 }));

	InitializeCompute();
}

void Water::InitializeCompute() {
	// Get render context
	auto context = renderer_->RequestRenderContext();

	// Allocate buffers for the compute shader
	VkBufferCreateInfo bufferCreateInfo = Quadbit::VkUtils::Init::BufferCreateInfo(sizeof(WaveheightUBO), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
	context->allocator->CreateBuffer(waveheightResources_.ubo, bufferCreateInfo, Quadbit::QBVK_MEMORY_USAGE_CPU_TO_GPU);

	VkImageCreateInfo imageCreateInfo = Quadbit::VkUtils::Init::ImageCreateInfo(WATER_RESOLUTION, WATER_RESOLUTION, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_STORAGE_BIT);
	context->allocator->CreateImage(waveheightResources_.h0s, imageCreateInfo, Quadbit::QBVK_MEMORY_USAGE_GPU_ONLY);
	context->allocator->CreateImage(waveheightResources_.h0sConj, imageCreateInfo, Quadbit::QBVK_MEMORY_USAGE_GPU_ONLY);

	VkDescriptorBufferInfo descBufferInfo{};
	descBufferInfo.buffer = waveheightResources_.ubo.buf;
	descBufferInfo.range = sizeof(WaveheightUBO);

	VkDescriptorImageInfo h0sDescImageInfo{};
	h0sDescImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	h0sDescImageInfo.imageView = Quadbit::VkUtils::CreateImageView(context, waveheightResources_.h0s.img, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT);

	VkDescriptorImageInfo h0sConjDescImageInfo{};
	h0sConjDescImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	h0sConjDescImageInfo.imageView = Quadbit::VkUtils::CreateImageView(context, waveheightResources_.h0sConj.img, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT);

	// Setup compute shader
	std::vector<std::tuple<VkDescriptorType, void*>> computeDescriptors{
			std::make_tuple(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &descBufferInfo),
			std::make_tuple(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &h0sDescImageInfo),
			std::make_tuple(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &h0sConjDescImageInfo),
	};
	Quadbit::QbVkComputeInstance computeInstance = renderer_->CreateComputeInstance(computeDescriptors, "Resources/Shaders/Compiled/waveheights.comp.spv", "main");


	// Transition images from undefined layout
	VkCommandBuffer tempBuffer = Quadbit::VkUtils::InitSingleTimeCommandBuffer(context);

	Quadbit::VkUtils::TransitionImageLayout(context, tempBuffer, waveheightResources_.h0s.img, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
	Quadbit::VkUtils::TransitionImageLayout(context, tempBuffer, waveheightResources_.h0sConj.img, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

	Quadbit::VkUtils::FlushCommandBuffer(context, tempBuffer);

	RecordComputeCommands(computeInstance);
	renderer_->ComputeDispatch(computeInstance);

	VkDebugUtilsObjectNameInfoEXT nameInfo{};
	nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
	nameInfo.objectType = VK_OBJECT_TYPE_IMAGE;
	nameInfo.objectHandle = (uint64_t)waveheightResources_.h0s.img;
	nameInfo.pObjectName = "h0s";
	auto instance = renderer_->GetInstance();
	auto setDebugUtilsObjectName = (PFN_vkSetDebugUtilsObjectNameEXT)vkGetInstanceProcAddr(instance, "vkSetDebugUtilsObjectNameEXT");
	if (setDebugUtilsObjectName != nullptr) {
		setDebugUtilsObjectName(context->device, &nameInfo);
	}
	nameInfo.pObjectName = "h0sConj";
	nameInfo.objectHandle = (uint64_t)waveheightResources_.h0sConj.img;
	if (setDebugUtilsObjectName != nullptr) {
		setDebugUtilsObjectName(context->device, &nameInfo);
	}
}

void Water::RecordComputeCommands(Quadbit::QbVkComputeInstance& computeInstance) {
	VkCommandBufferBeginInfo cmdBufInfo = Quadbit::VkUtils::Init::CommandBufferBeginInfo();
	VK_CHECK(vkBeginCommandBuffer(computeInstance.commandBuffer, &cmdBufInfo));

	vkCmdBindPipeline(computeInstance.commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computeInstance.pipeline);
	vkCmdBindDescriptorSets(computeInstance.commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computeInstance.pipelineLayout, 0, 1, &computeInstance.descriptorSet, 0, 0);
	vkCmdDispatch(computeInstance.commandBuffer, 512 / 16, 512 / 16, 1);

	vkEndCommandBuffer(computeInstance.commandBuffer);
}

void Water::Simulate(float deltaTime) {
}

void Water::DrawFrame() {
	renderer_->DrawFrame();
}