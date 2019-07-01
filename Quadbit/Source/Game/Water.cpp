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

	VkDescriptorImageInfo descImageInfo{};
	descImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	descImageInfo.imageView = Quadbit::VkUtils::CreateImageView(renderer_->RequestRenderContext(), waveheightResources_.h0TildeTy.img, IMAGE_FORMAT, VK_IMAGE_ASPECT_COLOR_BIT);

	Quadbit::QbVkRenderMeshInstance* rMeshInstance = renderer_->CreateRenderMeshInstance({
			std::make_tuple(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &descImageInfo)
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
	vkCmdDispatch(precalcInstance_.commandBuffer, (WATER_RESOLUTION + 1) / 16, (WATER_RESOLUTION + 1) / 16, 1);
	VK_CHECK(vkEndCommandBuffer(precalcInstance_.commandBuffer));

	// Record for the waveheight compute instance
	cmdBufInfo = Quadbit::VkUtils::Init::CommandBufferBeginInfo();
	VK_CHECK(vkBeginCommandBuffer(waveheightInstance_.commandBuffer, &cmdBufInfo));

	VkImageMemoryBarrier imgBarrier{};
	imgBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	imgBarrier.image = waveheightResources_.h0TildeTy.img;
	imgBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
	imgBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
	imgBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
	imgBarrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	imgBarrier.srcQueueFamilyIndex = context->gpu->computeFamilyIdx;
	imgBarrier.dstQueueFamilyIndex = context->gpu->graphicsFamilyIdx;

	vkCmdPipelineBarrier(waveheightInstance_.commandBuffer, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imgBarrier);

	vkCmdBindPipeline(waveheightInstance_.commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, waveheightInstance_.pipeline);
	vkCmdBindDescriptorSets(waveheightInstance_.commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, waveheightInstance_.pipelineLayout, 0, 1, &waveheightInstance_.descriptorSet, 0, 0);
	vkCmdDispatch(waveheightInstance_.commandBuffer, WATER_RESOLUTION / 16, WATER_RESOLUTION / 16, 1);

	VK_CHECK(vkEndCommandBuffer(waveheightInstance_.commandBuffer));
}

void Water::Simulate(float deltaTime) {
	UpdateWaveheightUBO(deltaTime);
	renderer_->ComputeDispatch(waveheightInstance_);
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
	ubo->N = 512;
	ubo->A = 0.0005f;
	ubo->L = 1000;
	ubo->W = glm::float2(16.0f, 16.0f);

	// Create images
	VkImageCreateInfo imageCreateInfo = Quadbit::VkUtils::Init::ImageCreateInfo(WATER_RESOLUTION + 1, WATER_RESOLUTION + 1, IMAGE_FORMAT, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_STORAGE_BIT);
	context->allocator->CreateImage(precalcResources_.h0Tilde, imageCreateInfo, Quadbit::QBVK_MEMORY_USAGE_GPU_ONLY);
	context->allocator->CreateImage(precalcResources_.h0TildeConj, imageCreateInfo, Quadbit::QBVK_MEMORY_USAGE_GPU_ONLY);


	// Precalculate uniform randoms used for the generation of the initial frequency heightmaps
	precalcResources_.precalcUniformRandoms.resize((WATER_RESOLUTION + 1) * (WATER_RESOLUTION + 1) * 4);
	std::minstd_rand engine(std::random_device{}());
	std::uniform_real_distribution<double> dist(0.0, 1.0);
	for (int i = 0; i < (WATER_RESOLUTION + 1) * (WATER_RESOLUTION + 1); i++) {
		precalcResources_.precalcUniformRandoms[i] = {
			static_cast<float>(dist(engine)),
			static_cast<float>(dist(engine)),
			static_cast<float>(dist(engine)),
			static_cast<float>(dist(engine))
		};
	}
	VkDeviceSize uniformRandomsSize = (WATER_RESOLUTION + 1) * (WATER_RESOLUTION + 1) * sizeof(glm::float4);
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

void Water::UpdateWaveheightUBO(float deltaTime) {
	static float t = 0.0f;

	WaveheightUBO* ubo = reinterpret_cast<WaveheightUBO*>(waveheightResources_.ubo.alloc.data);
	ubo->N = 512;
	ubo->L = 1000;
	ubo->RT = repeat_;
	ubo->T = t;

	t += deltaTime * step_;
}