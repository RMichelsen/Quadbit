#include <PCH.h>
#include <random>

#include "Water.h"
#include "../Engine/Rendering/Common/QbVkUtils.h"
#include "../Engine/Global/ImGuiState.h"

void Water::Init() {
	InitializeCompute();

	// Initialize vertices and indices for the water mesh
	waterVertices_.resize((WATER_RESOLUTION + 1) * (WATER_RESOLUTION + 1));
	waterIndices_.resize(WATER_RESOLUTION * WATER_RESOLUTION * 6);
	int vertexCount = 0;
	int indexCount = 0;
	for (int x = 0; x <= WATER_RESOLUTION; x++) {
		for (int z = 0; z <= WATER_RESOLUTION; z++) {
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

	VkDescriptorImageInfo displacementImageDescInfo{};
	displacementImageDescInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	displacementImageDescInfo.imageView = Quadbit::VkUtils::CreateImageView(renderer_->RequestRenderContext(), 
		displacementResources_.displacement.img, IMAGE_FORMAT, VK_IMAGE_ASPECT_COLOR_BIT);

	Quadbit::QbVkRenderMeshInstance* rMeshInstance = renderer_->CreateRenderMeshInstance({
			std::make_tuple(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &displacementImageDescInfo, VK_SHADER_STAGE_FRAGMENT_BIT)
		}, 
		"Resources/Shaders/Compiled/water_vert.spv", "main", "Resources/Shaders/Compiled/water_frag.spv", "main"
	);

	entity.AddComponent<Quadbit::RenderMeshComponent>(renderer_->CreateMesh(waterVertices_, waterIndices_, rMeshInstance));
	entity.AddComponent<Quadbit::RenderTransformComponent>(Quadbit::RenderTransformComponent(1.0f, { 0.0f, 0.0f, 0.0f }, { 0, 0, 0, 1 }));

	step_ = 1.0f;
	repeat_ = 200.0f;
	Quadbit::ImGuiState::Inject([]() {
		ImGui::SetNextWindowSize(ImVec2(300, 100), ImGuiCond_FirstUseEver);
		ImGui::Begin("Water Debug", nullptr);
		ImGui::SliderFloat("Step size", &step_, 0.1f, 10.0f, "%.3f");
		ImGui::SliderFloat("Cycle length", &repeat_, 10.0f, 500.0f, "%.3f");
		ImGui::End();
	});
}

void Water::InitializeCompute() {

	// Initialize precalc and waveheight compute instances
	InitPrecalcComputeInstance();
	InitWaveheightComputeInstance();
	InitInverseFFTComputeInstances();
	InitDisplacementInstance();

	RecordComputeCommands();
	renderer_->ComputeDispatch(precalcInstance_);
}

void Water::RecordComputeCommands() {
	auto context = renderer_->RequestRenderContext();

	vkQueueWaitIdle(context->computeQueue);

	// Record for the precalc compute instance
	VkCommandBufferBeginInfo cmdBufInfo = Quadbit::VkUtils::Init::CommandBufferBeginInfo();
	VK_CHECK(vkBeginCommandBuffer(precalcInstance_.commandBuffer, &cmdBufInfo));
	vkCmdBindPipeline(precalcInstance_.commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, precalcInstance_.pipeline);
	vkCmdBindDescriptorSets(precalcInstance_.commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, precalcInstance_.pipelineLayout, 0, 1, &precalcInstance_.descriptorSet, 0, 0);
	vkCmdDispatch(precalcInstance_.commandBuffer, WATER_RESOLUTION / 32, WATER_RESOLUTION / 32, 1);
	VK_CHECK(vkEndCommandBuffer(precalcInstance_.commandBuffer));

	// Record for the waveheight compute instance
	cmdBufInfo = Quadbit::VkUtils::Init::CommandBufferBeginInfo();
	VK_CHECK(vkBeginCommandBuffer(waveheightInstance_.commandBuffer, &cmdBufInfo));
	vkCmdBindPipeline(waveheightInstance_.commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, waveheightInstance_.pipeline);
	vkCmdBindDescriptorSets(waveheightInstance_.commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, waveheightInstance_.pipelineLayout, 0, 1, &waveheightInstance_.descriptorSet, 0, 0);
	vkCmdDispatch(waveheightInstance_.commandBuffer, WATER_RESOLUTION / 32, WATER_RESOLUTION / 32, 1);

	VkImageMemoryBarrier h0TildeTxBarrier = Quadbit::VkUtils::Init::ImageMemoryBarrier();
	h0TildeTxBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
	h0TildeTxBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
	h0TildeTxBarrier.image = waveheightResources_.h0TildeTx.img;
	h0TildeTxBarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
	h0TildeTxBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	h0TildeTxBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

	VkImageMemoryBarrier h0TildeTyBarrier = Quadbit::VkUtils::Init::ImageMemoryBarrier();
	h0TildeTyBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
	h0TildeTyBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
	h0TildeTyBarrier.image = waveheightResources_.h0TildeTy.img;
	h0TildeTyBarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
	h0TildeTyBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	h0TildeTyBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

	VkImageMemoryBarrier h0TildeTzBarrier = Quadbit::VkUtils::Init::ImageMemoryBarrier();
	h0TildeTzBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
	h0TildeTzBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
	h0TildeTzBarrier.image = waveheightResources_.h0TildeTz.img;
	h0TildeTzBarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
	h0TildeTzBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	h0TildeTzBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

	std::array<VkImageMemoryBarrier, 3> tBarriers { h0TildeTxBarrier, h0TildeTyBarrier, h0TildeTzBarrier };

	vkCmdPipelineBarrier(waveheightInstance_.commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		0, 0, nullptr, 0, nullptr, 3, tBarriers.data());

	VK_CHECK(vkEndCommandBuffer(waveheightInstance_.commandBuffer));

	// Record for the IFFT compute instances
	cmdBufInfo = Quadbit::VkUtils::Init::CommandBufferBeginInfo();
	VK_CHECK(vkBeginCommandBuffer(horizontalIFFTInstance_.commandBuffer, &cmdBufInfo));
	vkCmdBindPipeline(horizontalIFFTInstance_.commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, horizontalIFFTInstance_.pipeline);
	vkCmdBindDescriptorSets(horizontalIFFTInstance_.commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, horizontalIFFTInstance_.pipelineLayout, 0, 1, &horizontalIFFTInstance_.descriptorSet, 0, 0);
	vkCmdDispatch(horizontalIFFTInstance_.commandBuffer, 1, WATER_RESOLUTION, 1);

	VkImageMemoryBarrier hDxBarrier = Quadbit::VkUtils::Init::ImageMemoryBarrier();
	hDxBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
	hDxBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
	hDxBarrier.image = horizontalIFFTResources_.Dx.img;
	hDxBarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
	hDxBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	hDxBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

	VkImageMemoryBarrier hDyBarrier = Quadbit::VkUtils::Init::ImageMemoryBarrier();
	hDyBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
	hDyBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
	hDyBarrier.image = horizontalIFFTResources_.Dy.img;
	hDyBarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
	hDyBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	hDyBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

	VkImageMemoryBarrier hDzBarrier = Quadbit::VkUtils::Init::ImageMemoryBarrier();
	hDzBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
	hDzBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
	hDzBarrier.image = horizontalIFFTResources_.Dz.img;
	hDzBarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
	hDzBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	hDzBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

	std::array<VkImageMemoryBarrier, 3> hBarriers { hDxBarrier, hDyBarrier, hDzBarrier };

	vkCmdPipelineBarrier(horizontalIFFTInstance_.commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		0, 0, nullptr, 0, nullptr, 3, hBarriers.data());

	VK_CHECK(vkEndCommandBuffer(horizontalIFFTInstance_.commandBuffer));

	cmdBufInfo = Quadbit::VkUtils::Init::CommandBufferBeginInfo();
	VK_CHECK(vkBeginCommandBuffer(verticalIFFTInstance_.commandBuffer, &cmdBufInfo));
	vkCmdBindPipeline(verticalIFFTInstance_.commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, verticalIFFTInstance_.pipeline);
	vkCmdBindDescriptorSets(verticalIFFTInstance_.commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, verticalIFFTInstance_.pipelineLayout, 0, 1, &verticalIFFTInstance_.descriptorSet, 0, 0);
	vkCmdDispatch(verticalIFFTInstance_.commandBuffer, 1, WATER_RESOLUTION, 1);

	VkImageMemoryBarrier vDxBarrier = Quadbit::VkUtils::Init::ImageMemoryBarrier();
	vDxBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
	vDxBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
	vDxBarrier.image = verticalIFFTResources_.Dx.img;
	vDxBarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
	vDxBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	vDxBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

	VkImageMemoryBarrier vDyBarrier = Quadbit::VkUtils::Init::ImageMemoryBarrier();
	vDyBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
	vDyBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
	vDyBarrier.image = verticalIFFTResources_.Dy.img;
	vDyBarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
	vDyBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	vDyBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

	VkImageMemoryBarrier vDzBarrier = Quadbit::VkUtils::Init::ImageMemoryBarrier();
	vDzBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
	vDzBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
	vDzBarrier.image = verticalIFFTResources_.Dz.img;
	vDzBarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
	vDzBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	vDzBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

	std::array<VkImageMemoryBarrier, 3> vBarriers{ vDxBarrier, vDyBarrier, vDzBarrier };

	vkCmdPipelineBarrier(verticalIFFTInstance_.commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		0, 0, nullptr, 0, nullptr, 3, vBarriers.data());

	VK_CHECK(vkEndCommandBuffer(verticalIFFTInstance_.commandBuffer));

	VK_CHECK(vkBeginCommandBuffer(displacementInstance_.commandBuffer, &cmdBufInfo));
	vkCmdBindPipeline(displacementInstance_.commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, displacementInstance_.pipeline);
	vkCmdBindDescriptorSets(displacementInstance_.commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, displacementInstance_.pipelineLayout, 0, 1, &displacementInstance_.descriptorSet, 0, 0);
	vkCmdDispatch(displacementInstance_.commandBuffer, WATER_RESOLUTION / 32, WATER_RESOLUTION / 32, 1);

	vDxBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
	vDxBarrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	vDxBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
	vDxBarrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	vDxBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
	vDxBarrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;

	vkCmdPipelineBarrier(displacementInstance_.commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		0, 0, nullptr, 0, nullptr, 3, vBarriers.data());

	VkImageMemoryBarrier dispBarrier = Quadbit::VkUtils::Init::ImageMemoryBarrier();
	dispBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
	dispBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
	dispBarrier.image = displacementResources_.displacement.img;
	dispBarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
	dispBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	dispBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

	vkCmdPipelineBarrier(displacementInstance_.commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		0, 0, nullptr, 0, nullptr, 1, &dispBarrier);

	VK_CHECK(vkEndCommandBuffer(displacementInstance_.commandBuffer));
}

void Water::Simulate(float deltaTime) {
	UpdateWaveheightUBO(deltaTime);
	renderer_->ComputeDispatch(waveheightInstance_);

	renderer_->ComputeDispatch(horizontalIFFTInstance_);
	renderer_->ComputeDispatch(verticalIFFTInstance_);

	renderer_->ComputeDispatch(displacementInstance_);
}

void Water::DrawFrame() {
	renderer_->DrawFrame();
}

void Water::InitPrecalcComputeInstance() {
	// Get render context
	auto context = renderer_->RequestRenderContext();

	// Allocate uniform buffer for the compute shader
	VkBufferCreateInfo bufferCreateInfo = Quadbit::VkUtils::Init::BufferCreateInfo(sizeof(PrecalcUBO), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
	context->allocator->CreateBuffer(precalcResources_.ubo, bufferCreateInfo, Quadbit::QBVK_MEMORY_USAGE_CPU_TO_GPU);

	PrecalcUBO* ubo = reinterpret_cast<PrecalcUBO*>(precalcResources_.ubo.alloc.data);
	ubo->N = WATER_RESOLUTION;
	ubo->A = 20.0f;
	ubo->L = 1000;
	ubo->W = glm::float2(16.0f, 16.0f);

	// Create images
	VkImageCreateInfo imageCreateInfo = Quadbit::VkUtils::Init::ImageCreateInfo(WATER_RESOLUTION, WATER_RESOLUTION, IMAGE_FORMAT, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_STORAGE_BIT);
	context->allocator->CreateImage(precalcResources_.h0Tilde, imageCreateInfo, Quadbit::QBVK_MEMORY_USAGE_GPU_ONLY);
	context->allocator->CreateImage(precalcResources_.h0TildeConj, imageCreateInfo, Quadbit::QBVK_MEMORY_USAGE_GPU_ONLY);


	// Precalculate uniform randoms used for the generation of the initial frequency heightmaps
	precalcResources_.precalcUniformRandoms.resize(WATER_RESOLUTION * WATER_RESOLUTION * 4);
	std::minstd_rand engine(std::random_device{}());
	std::uniform_real_distribution<double> dist(0.0, 1.0);
	for (int i = 0; i < WATER_RESOLUTION * WATER_RESOLUTION; i++) {
		precalcResources_.precalcUniformRandoms[i] = {
			static_cast<float>(dist(engine)),
			static_cast<float>(dist(engine)),
			static_cast<float>(dist(engine)),
			static_cast<float>(dist(engine))
		};
	}
	VkDeviceSize uniformRandomsSize = WATER_RESOLUTION * WATER_RESOLUTION * sizeof(glm::float4);
	VkBufferCreateInfo uniformRandomsStorageBufferCreateInfo =
		Quadbit::VkUtils::Init::BufferCreateInfo(uniformRandomsSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	context->allocator->CreateBuffer(precalcResources_.uniformRandomsStorageBuffer, uniformRandomsStorageBufferCreateInfo, Quadbit::QBVK_MEMORY_USAGE_GPU_ONLY);

	Quadbit::QbVkBuffer uniformRandomsStagingBuffer;
	context->allocator->CreateStagingBuffer(uniformRandomsStagingBuffer, uniformRandomsSize, precalcResources_.precalcUniformRandoms.data());
	Quadbit::VkUtils::CopyBuffer(context, uniformRandomsStagingBuffer.buf, precalcResources_.uniformRandomsStorageBuffer.buf, uniformRandomsSize);
	context->allocator->DestroyBuffer(uniformRandomsStagingBuffer);


	// Precalculate twiddle indices used in the IFFT
	precalcResources_.precalcBitRevIndices.resize(WATER_RESOLUTION);
	for (auto i = 0; i < WATER_RESOLUTION; i++) {
		uint32_t x = i;
		uint32_t res = 0;
		for (auto j = 0; j < log(WATER_RESOLUTION) / log(2); j++) {
			res = (res << 1) + (x & 1);
			x >>= 1;
		}
		precalcResources_.precalcBitRevIndices[i] = res;
	}
	VkDeviceSize bitRevIndicesSize = WATER_RESOLUTION;
	VkBufferCreateInfo bitRevIndicesStorageBufferCreateInfo =
		Quadbit::VkUtils::Init::BufferCreateInfo(bitRevIndicesSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	context->allocator->CreateBuffer(precalcResources_.bitRevIndicesStorageBuffer, bitRevIndicesStorageBufferCreateInfo, Quadbit::QBVK_MEMORY_USAGE_GPU_ONLY);

	Quadbit::QbVkBuffer bitRevIndicesStagingBuffer;
	context->allocator->CreateStagingBuffer(bitRevIndicesStagingBuffer, bitRevIndicesSize, precalcResources_.precalcBitRevIndices.data());
	Quadbit::VkUtils::CopyBuffer(context, bitRevIndicesStagingBuffer.buf, precalcResources_.bitRevIndicesStorageBuffer.buf, bitRevIndicesSize);
	context->allocator->DestroyBuffer(bitRevIndicesStagingBuffer);


	VkDescriptorBufferInfo descBufferInfo{};
	descBufferInfo.buffer = precalcResources_.ubo.buf;
	descBufferInfo.range = sizeof(PrecalcUBO);

	VkDescriptorImageInfo h0TildeDescImageInfo{};
	h0TildeDescImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	h0TildeDescImageInfo.imageView = Quadbit::VkUtils::CreateImageView(context, precalcResources_.h0Tilde.img, IMAGE_FORMAT, VK_IMAGE_ASPECT_COLOR_BIT);

	VkDescriptorImageInfo h0TildeConjDescImageInfo{};
	h0TildeConjDescImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	h0TildeConjDescImageInfo.imageView = Quadbit::VkUtils::CreateImageView(context, precalcResources_.h0TildeConj.img, IMAGE_FORMAT, VK_IMAGE_ASPECT_COLOR_BIT);

	VkDescriptorBufferInfo descUniformRandomsStorageBufferInfo{};
	descUniformRandomsStorageBufferInfo.buffer = precalcResources_.uniformRandomsStorageBuffer.buf;
	descUniformRandomsStorageBufferInfo.range = uniformRandomsSize;

	VkDescriptorBufferInfo descBitRevIndicesStorageBufferInfo{};
	descBitRevIndicesStorageBufferInfo.buffer = precalcResources_.bitRevIndicesStorageBuffer.buf;
	descBitRevIndicesStorageBufferInfo.range = bitRevIndicesSize;

	// Transition images from undefined layout and also copy the uniform randoms into the buffer on the GPU
	VkCommandBuffer tempBuffer = Quadbit::VkUtils::InitSingleTimeCommandBuffer(context);

	Quadbit::VkUtils::TransitionImageLayout(context, tempBuffer, precalcResources_.h0Tilde.img, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
	Quadbit::VkUtils::TransitionImageLayout(context, tempBuffer, precalcResources_.h0TildeConj.img, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

	Quadbit::VkUtils::FlushCommandBuffer(context, tempBuffer);

	// Setup precalc compute shader
	std::vector<std::tuple<VkDescriptorType, void*>> computeDescriptors {
			std::make_tuple(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &descBufferInfo),
			std::make_tuple(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &h0TildeDescImageInfo),
			std::make_tuple(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &h0TildeConjDescImageInfo),
			std::make_tuple(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &descUniformRandomsStorageBufferInfo),
			std::make_tuple(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &descBitRevIndicesStorageBufferInfo),
	};
	precalcInstance_ = renderer_->CreateComputeInstance(computeDescriptors, "Resources/Shaders/Compiled/precalc_comp.spv", "main");
}

void Water::InitWaveheightComputeInstance() {
	// Get render context
	auto context = renderer_->RequestRenderContext();

	// Allocate uniform buffer for the compute shader
	VkBufferCreateInfo bufferCreateInfo = Quadbit::VkUtils::Init::BufferCreateInfo(sizeof(WaveheightUBO), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
	context->allocator->CreateBuffer(waveheightResources_.ubo, bufferCreateInfo, Quadbit::QBVK_MEMORY_USAGE_CPU_TO_GPU);

	// Create images
	VkImageCreateInfo imageCreateInfo = Quadbit::VkUtils::Init::ImageCreateInfo(WATER_RESOLUTION, WATER_RESOLUTION, IMAGE_FORMAT, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_STORAGE_BIT);
	context->allocator->CreateImage(waveheightResources_.h0TildeTx, imageCreateInfo, Quadbit::QBVK_MEMORY_USAGE_GPU_ONLY);
	context->allocator->CreateImage(waveheightResources_.h0TildeTy, imageCreateInfo, Quadbit::QBVK_MEMORY_USAGE_GPU_ONLY);
	context->allocator->CreateImage(waveheightResources_.h0TildeTz, imageCreateInfo, Quadbit::QBVK_MEMORY_USAGE_GPU_ONLY);

	// Fill descriptors
	VkDescriptorBufferInfo descBufferInfo{};
	descBufferInfo.buffer = waveheightResources_.ubo.buf;
	descBufferInfo.range = sizeof(WaveheightUBO);

	VkDescriptorImageInfo h0TildeDescImageInfo{};
	h0TildeDescImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	h0TildeDescImageInfo.imageView = Quadbit::VkUtils::CreateImageView(context, precalcResources_.h0Tilde.img, IMAGE_FORMAT, VK_IMAGE_ASPECT_COLOR_BIT);

	VkDescriptorImageInfo h0TildeConjDescImageInfo{};
	h0TildeConjDescImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	h0TildeConjDescImageInfo.imageView = Quadbit::VkUtils::CreateImageView(context, precalcResources_.h0TildeConj.img, IMAGE_FORMAT, VK_IMAGE_ASPECT_COLOR_BIT);

	VkDescriptorImageInfo h0TildeTxDescImageInfo{};
	h0TildeTxDescImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	h0TildeTxDescImageInfo.imageView = Quadbit::VkUtils::CreateImageView(context, waveheightResources_.h0TildeTx.img, IMAGE_FORMAT, VK_IMAGE_ASPECT_COLOR_BIT);
	VkDescriptorImageInfo h0TildeTyDescImageInfo{};
	h0TildeTyDescImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	h0TildeTyDescImageInfo.imageView = Quadbit::VkUtils::CreateImageView(context, waveheightResources_.h0TildeTy.img, IMAGE_FORMAT, VK_IMAGE_ASPECT_COLOR_BIT);
	VkDescriptorImageInfo h0TildeTzDescImageInfo{};
	h0TildeTzDescImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	h0TildeTzDescImageInfo.imageView = Quadbit::VkUtils::CreateImageView(context, waveheightResources_.h0TildeTz.img, IMAGE_FORMAT, VK_IMAGE_ASPECT_COLOR_BIT);

	// Transition images from undefined layout and also copy the uniform randoms into the buffer on the GPU
	VkCommandBuffer tempBuffer = Quadbit::VkUtils::InitSingleTimeCommandBuffer(context);

	Quadbit::VkUtils::TransitionImageLayout(context, tempBuffer, waveheightResources_.h0TildeTx.img, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
	Quadbit::VkUtils::TransitionImageLayout(context, tempBuffer, waveheightResources_.h0TildeTy.img, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
	Quadbit::VkUtils::TransitionImageLayout(context, tempBuffer, waveheightResources_.h0TildeTz.img, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

	Quadbit::VkUtils::FlushCommandBuffer(context, tempBuffer);

	// Setup waveheight shader
	std::vector<std::tuple<VkDescriptorType, void*>> computeDescriptors{
			std::make_tuple(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &descBufferInfo),
			std::make_tuple(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &h0TildeDescImageInfo),
			std::make_tuple(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &h0TildeConjDescImageInfo),
			std::make_tuple(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &h0TildeTxDescImageInfo),
			std::make_tuple(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &h0TildeTyDescImageInfo),
			std::make_tuple(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &h0TildeTzDescImageInfo)
	};
	waveheightInstance_ = renderer_->CreateComputeInstance(computeDescriptors, "Resources/Shaders/Compiled/waveheight_comp.spv", "main");
}

void Water::InitInverseFFTComputeInstances() {
	// Get render context
	auto context = renderer_->RequestRenderContext();

	VkSpecializationMapEntry xLocalSize		{ 0, 0, sizeof(int) };
	VkSpecializationMapEntry yLocalSize		{ 1, sizeof(int) * 1, sizeof(int) };
	VkSpecializationMapEntry zLocalSize		{ 2, sizeof(int) * 2, sizeof(int) };
	VkSpecializationMapEntry resolution		{ 3, sizeof(int) * 3, sizeof(int) };
	VkSpecializationMapEntry passCount		{ 4, sizeof(int) * 4, sizeof(int) };
	VkSpecializationMapEntry verticalPass	{ 5, sizeof(int) * 5, sizeof(int) };

	std::array<VkSpecializationMapEntry, 6> specMaps { xLocalSize, yLocalSize, zLocalSize, resolution, passCount, verticalPass };

	horizontalIFFTResources_.specData = { WATER_RESOLUTION, 1, 1, WATER_RESOLUTION, static_cast<int>(std::log2(WATER_RESOLUTION)), 0 };
	verticalIFFTResources_.specData = { WATER_RESOLUTION, 1, 1, WATER_RESOLUTION, static_cast<int>(std::log2(WATER_RESOLUTION)), 1 };

	VkSpecializationInfo horizontalSpecInfo{};
	horizontalSpecInfo.mapEntryCount = 6;
	horizontalSpecInfo.pMapEntries = specMaps.data();
	horizontalSpecInfo.dataSize = 6 * sizeof(int);
	horizontalSpecInfo.pData = horizontalIFFTResources_.specData.data();

	VkSpecializationInfo verticalSpecInfo{};
	verticalSpecInfo.mapEntryCount = 6;
	verticalSpecInfo.pMapEntries = specMaps.data();
	verticalSpecInfo.dataSize = 6 * sizeof(int);
	verticalSpecInfo.pData = verticalIFFTResources_.specData.data();

	// Create images	
	VkImageCreateInfo imageCreateInfo = Quadbit::VkUtils::Init::ImageCreateInfo(WATER_RESOLUTION, WATER_RESOLUTION, IMAGE_FORMAT, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_STORAGE_BIT);
	context->allocator->CreateImage(horizontalIFFTResources_.Dx, imageCreateInfo, Quadbit::QBVK_MEMORY_USAGE_GPU_ONLY);
	context->allocator->CreateImage(verticalIFFTResources_.Dx, imageCreateInfo, Quadbit::QBVK_MEMORY_USAGE_GPU_ONLY);
	context->allocator->CreateImage(horizontalIFFTResources_.Dy, imageCreateInfo, Quadbit::QBVK_MEMORY_USAGE_GPU_ONLY);
	context->allocator->CreateImage(verticalIFFTResources_.Dy, imageCreateInfo, Quadbit::QBVK_MEMORY_USAGE_GPU_ONLY);
	context->allocator->CreateImage(horizontalIFFTResources_.Dz, imageCreateInfo, Quadbit::QBVK_MEMORY_USAGE_GPU_ONLY);
	context->allocator->CreateImage(verticalIFFTResources_.Dz, imageCreateInfo, Quadbit::QBVK_MEMORY_USAGE_GPU_ONLY);

	VkDescriptorImageInfo h0TildeTxDescImageInfo{};
	h0TildeTxDescImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	h0TildeTxDescImageInfo.imageView = Quadbit::VkUtils::CreateImageView(context, waveheightResources_.h0TildeTx.img, IMAGE_FORMAT, VK_IMAGE_ASPECT_COLOR_BIT);

	VkDescriptorImageInfo h0TildeTyDescImageInfo{};
	h0TildeTyDescImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	h0TildeTyDescImageInfo.imageView = Quadbit::VkUtils::CreateImageView(context, waveheightResources_.h0TildeTy.img, IMAGE_FORMAT, VK_IMAGE_ASPECT_COLOR_BIT);

	VkDescriptorImageInfo h0TildeTzDescImageInfo{};
	h0TildeTzDescImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	h0TildeTzDescImageInfo.imageView = Quadbit::VkUtils::CreateImageView(context, waveheightResources_.h0TildeTz.img, IMAGE_FORMAT, VK_IMAGE_ASPECT_COLOR_BIT);

	VkDescriptorImageInfo horizontalDxDescImageInfo{};
	horizontalDxDescImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	horizontalDxDescImageInfo.imageView = Quadbit::VkUtils::CreateImageView(context, horizontalIFFTResources_.Dx.img, IMAGE_FORMAT, VK_IMAGE_ASPECT_COLOR_BIT);
	VkDescriptorImageInfo horizontalDyDescImageInfo{};
	horizontalDyDescImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	horizontalDyDescImageInfo.imageView = Quadbit::VkUtils::CreateImageView(context, horizontalIFFTResources_.Dy.img, IMAGE_FORMAT, VK_IMAGE_ASPECT_COLOR_BIT);
	VkDescriptorImageInfo horizontalDzDescImageInfo{};
	horizontalDzDescImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	horizontalDzDescImageInfo.imageView = Quadbit::VkUtils::CreateImageView(context, horizontalIFFTResources_.Dz.img, IMAGE_FORMAT, VK_IMAGE_ASPECT_COLOR_BIT);

	VkDescriptorImageInfo verticalDxDescImageInfo{};
	verticalDxDescImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	verticalDxDescImageInfo.imageView = Quadbit::VkUtils::CreateImageView(context, verticalIFFTResources_.Dx.img, IMAGE_FORMAT, VK_IMAGE_ASPECT_COLOR_BIT);
	VkDescriptorImageInfo verticalDyDescImageInfo{};
	verticalDyDescImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	verticalDyDescImageInfo.imageView = Quadbit::VkUtils::CreateImageView(context, verticalIFFTResources_.Dy.img, IMAGE_FORMAT, VK_IMAGE_ASPECT_COLOR_BIT);
	VkDescriptorImageInfo verticalDzDescImageInfo{};
	verticalDzDescImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	verticalDzDescImageInfo.imageView = Quadbit::VkUtils::CreateImageView(context, verticalIFFTResources_.Dz.img, IMAGE_FORMAT, VK_IMAGE_ASPECT_COLOR_BIT);

	// Transition images from undefined layout and also copy the uniform randoms into the buffer on the GPU
	VkCommandBuffer tempBuffer = Quadbit::VkUtils::InitSingleTimeCommandBuffer(context);
	Quadbit::VkUtils::TransitionImageLayout(context, tempBuffer, horizontalIFFTResources_.Dx.img, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
	Quadbit::VkUtils::TransitionImageLayout(context, tempBuffer, horizontalIFFTResources_.Dy.img, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
	Quadbit::VkUtils::TransitionImageLayout(context, tempBuffer, horizontalIFFTResources_.Dz.img, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
	Quadbit::VkUtils::TransitionImageLayout(context, tempBuffer, verticalIFFTResources_.Dx.img, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
	Quadbit::VkUtils::TransitionImageLayout(context, tempBuffer, verticalIFFTResources_.Dy.img, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
	Quadbit::VkUtils::TransitionImageLayout(context, tempBuffer, verticalIFFTResources_.Dz.img, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
	Quadbit::VkUtils::FlushCommandBuffer(context, tempBuffer);

	std::vector<std::tuple<VkDescriptorType, void*>> horizontalComputeDesc {
		std::make_tuple(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &h0TildeTxDescImageInfo),
		std::make_tuple(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &h0TildeTyDescImageInfo),
		std::make_tuple(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &h0TildeTzDescImageInfo),
		std::make_tuple(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &horizontalDxDescImageInfo),
		std::make_tuple(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &horizontalDyDescImageInfo),
		std::make_tuple(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &horizontalDzDescImageInfo)
	};
	
	std::vector<std::tuple<VkDescriptorType, void*>> verticalComputeDesc {
		std::make_tuple(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &horizontalDxDescImageInfo),
		std::make_tuple(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &horizontalDyDescImageInfo),
		std::make_tuple(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &horizontalDzDescImageInfo),
		std::make_tuple(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &verticalDxDescImageInfo),
		std::make_tuple(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &verticalDyDescImageInfo),
		std::make_tuple(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &verticalDzDescImageInfo)
	};
	
	horizontalIFFTInstance_ = renderer_->CreateComputeInstance(horizontalComputeDesc, "Resources/Shaders/Compiled/ifft_comp.spv", "main", &horizontalSpecInfo);
	verticalIFFTInstance_ = renderer_->CreateComputeInstance(verticalComputeDesc, "Resources/Shaders/Compiled/ifft_comp.spv", "main", &verticalSpecInfo);
}

void Water::InitDisplacementInstance() {
	// Get render context
	auto context = renderer_->RequestRenderContext();

	VkImageCreateInfo imageCreateInfo = Quadbit::VkUtils::Init::ImageCreateInfo(WATER_RESOLUTION, WATER_RESOLUTION, IMAGE_FORMAT, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_STORAGE_BIT);
	context->allocator->CreateImage(displacementResources_.displacement, imageCreateInfo, Quadbit::QBVK_MEMORY_USAGE_GPU_ONLY);

	VkDescriptorImageInfo verticalDxDescImageInfo{};
	verticalDxDescImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	verticalDxDescImageInfo.imageView = Quadbit::VkUtils::CreateImageView(context, verticalIFFTResources_.Dx.img, IMAGE_FORMAT, VK_IMAGE_ASPECT_COLOR_BIT);
	VkDescriptorImageInfo verticalDyDescImageInfo{};
	verticalDyDescImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	verticalDyDescImageInfo.imageView = Quadbit::VkUtils::CreateImageView(context, verticalIFFTResources_.Dy.img, IMAGE_FORMAT, VK_IMAGE_ASPECT_COLOR_BIT);
	VkDescriptorImageInfo verticalDzDescImageInfo{};
	verticalDzDescImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	verticalDzDescImageInfo.imageView = Quadbit::VkUtils::CreateImageView(context, verticalIFFTResources_.Dz.img, IMAGE_FORMAT, VK_IMAGE_ASPECT_COLOR_BIT);

	VkDescriptorImageInfo displacementDescImageInfo{};
	displacementDescImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	displacementDescImageInfo.imageView = Quadbit::VkUtils::CreateImageView(context, displacementResources_.displacement.img, IMAGE_FORMAT, VK_IMAGE_ASPECT_COLOR_BIT);

	// Transition images from undefined layout and also copy the uniform randoms into the buffer on the GPU
	VkCommandBuffer tempBuffer = Quadbit::VkUtils::InitSingleTimeCommandBuffer(context);
	Quadbit::VkUtils::TransitionImageLayout(context, tempBuffer, displacementResources_.displacement.img, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
	Quadbit::VkUtils::FlushCommandBuffer(context, tempBuffer);

	// Setup FFT compute shader
	std::vector<std::tuple<VkDescriptorType, void*>> computeDescriptors {
			std::make_tuple(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &verticalDxDescImageInfo),
			std::make_tuple(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &verticalDyDescImageInfo),
			std::make_tuple(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &verticalDzDescImageInfo),
			std::make_tuple(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &displacementDescImageInfo)
	};

	displacementInstance_ = renderer_->CreateComputeInstance(computeDescriptors, "Resources/Shaders/Compiled/displacement_comp.spv", "main");
}

void Water::UpdateWaveheightUBO(float deltaTime) {
	static float t = 0.0f;

	WaveheightUBO* ubo = reinterpret_cast<WaveheightUBO*>(waveheightResources_.ubo.alloc.data);
	ubo->N = WATER_RESOLUTION;
	ubo->L = 1000;
	ubo->RT = repeat_;
	ubo->T = t;

	t += deltaTime * step_;
}
