#include <PCH.h>
#include <random>

#include "Water.h"
#include "../Engine/Rendering/Common/QbVkUtils.h"
#include "../Engine/Global/ImGuiState.h"

void Water::Init() {
	InitializeCompute();

	// Initialize vertices and indices for the water mesh
	int meshBounds = WATER_RESOLUTION + 1;
	waterVertices_.resize(static_cast<std::size_t>(meshBounds) * static_cast<std::size_t>(meshBounds) + 1);
	waterIndices_.resize((static_cast<std::size_t>(meshBounds) - 1) * (static_cast<std::size_t>(meshBounds) - 1) * 6);
	int vertexCount = 0;
	int indexCount = 0;
	for (int x = 0; x < meshBounds; x++) {
		for (int z = 0; z < meshBounds; z++) {
			waterVertices_[vertexCount] = {x, 0, z};
			if (x < meshBounds - 1 && z < meshBounds - 1) {
				waterIndices_[indexCount++] = vertexCount;
				waterIndices_[indexCount++] = vertexCount + meshBounds;
				waterIndices_[indexCount++] = vertexCount + meshBounds + 1;
				waterIndices_[indexCount++] = vertexCount;
				waterIndices_[indexCount++] = vertexCount + meshBounds + 1;
				waterIndices_[indexCount++] = vertexCount + 1;
			}
			vertexCount++;
		}
	}

	// Create UBO
	renderer_->CreateGPUBuffer(togglesUBO_, sizeof(TogglesUBO), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, Quadbit::QBVK_MEMORY_USAGE_CPU_TO_GPU);
	TogglesUBO* ubo = reinterpret_cast<TogglesUBO*>(togglesUBO_.alloc.data);
	ubo->useNormalMap = 0;

	Quadbit::QbVkRenderMeshInstance* rMeshInstance = renderer_->CreateRenderMeshInstance(
		{
			Quadbit::VkUtils::CreateRenderDescriptor(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, displacementResources_.displacementMap.descriptor, VK_SHADER_STAGE_VERTEX_BIT),
			Quadbit::VkUtils::CreateRenderDescriptor(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, displacementResources_.normalMap.descriptor, VK_SHADER_STAGE_FRAGMENT_BIT),
			Quadbit::VkUtils::CreateRenderDescriptor(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, togglesUBO_.descriptor, VK_SHADER_STAGE_FRAGMENT_BIT)}, 
		{ 
			Quadbit::QbVkVertexInputAttribute::QBVK_VERTEX_ATTRIBUTE_POSITION 
		},
		"Resources/Shaders/Compiled/water_vert.spv", "main", "Resources/Shaders/Compiled/water_frag.spv", "main"
	);


	// Setup water plane entity
	auto entityManager = Quadbit::EntityManager::GetOrCreate();
	auto entity = entityManager->Create();
	entity.AddComponent<Quadbit::RenderMeshComponent>(renderer_->CreateMesh(waterVertices_, waterIndices_, rMeshInstance));
	entity.AddComponent<Quadbit::RenderTransformComponent>(Quadbit::RenderTransformComponent(1.0f, { 0.0f, 0.0f, 0.0f }, { 0, 0, 0, 1 }));

	entity = entityManager->Create();
	entity.AddComponent<Quadbit::RenderMeshComponent>(renderer_->CreateMesh(waterVertices_, waterIndices_, rMeshInstance));
	entity.AddComponent<Quadbit::RenderTransformComponent>(Quadbit::RenderTransformComponent(1.0f, { WATER_RESOLUTION, 0.0f, 0.0f }, { 0, 0, 0, 1 }));

	entity = entityManager->Create();
	entity.AddComponent<Quadbit::RenderMeshComponent>(renderer_->CreateMesh(waterVertices_, waterIndices_, rMeshInstance));
	entity.AddComponent<Quadbit::RenderTransformComponent>(Quadbit::RenderTransformComponent(1.0f, { 0.0f, 0.0f, WATER_RESOLUTION }, { 0, 0, 0, 1 }));

	entity = entityManager->Create();
	entity.AddComponent<Quadbit::RenderMeshComponent>(renderer_->CreateMesh(waterVertices_, waterIndices_, rMeshInstance));
	entity.AddComponent<Quadbit::RenderTransformComponent>(Quadbit::RenderTransformComponent(1.0f, { WATER_RESOLUTION, 0.0f, WATER_RESOLUTION }, { 0, 0, 0, 1 }));


	// Some ImGui debug stuff 
	// (Can be extended to modify wind and amplitudes, but will need to recreate the initial precalcs every time that happens)
	step_ = 1.0f;
	repeat_ = 200.0f;
	Quadbit::ImGuiState::Inject([]() {
		ImGui::SetNextWindowSize(ImVec2(300, 100), ImGuiCond_FirstUseEver);
		ImGui::Begin("Water Debug", nullptr);
		ImGui::SliderFloat("Step size", &step_, 0.1f, 10.0f, "%.3f");
		ImGui::SliderFloat("Cycle length", &repeat_, 10.0f, 500.0f, "%.3f");
		ImGui::Checkbox("Use Normal Map?", &useNormalMap_);
		ImGui::End();
	});
}

void Water::InitializeCompute() {
	// Initialize the necessary compute instances
	// Precalc runs once. Then for each frame:
	// Waveheight (Frequency Domain) --> Inverse Fast Fourier Transform -> Displacement Map (Spatial Domain)
	InitPrecalcComputeInstance();
	InitWaveheightComputeInstance();
	InitInverseFFTComputeInstances();
	InitDisplacementInstance();

	RecordComputeCommands();
	renderer_->ComputeDispatch(precalcInstance_);
}

// This function records all the compute commands that will be run by each individual compute shader
// The barriers are there to make sure images are only written/read from when they are available
void Water::RecordComputeCommands() {
	// Global memory barrier used throughout
	VkMemoryBarrier memoryBarrier = Quadbit::VkUtils::Init::MemoryBarrierVk(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);

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
	vkCmdPipelineBarrier(waveheightInstance_.commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memoryBarrier, 0, nullptr, 0, nullptr);
	VK_CHECK(vkEndCommandBuffer(waveheightInstance_.commandBuffer));


	// Record for horizontal IFFT
	cmdBufInfo = Quadbit::VkUtils::Init::CommandBufferBeginInfo();
	VK_CHECK(vkBeginCommandBuffer(horizontalIFFTInstance_.commandBuffer, &cmdBufInfo));
	vkCmdBindPipeline(horizontalIFFTInstance_.commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, horizontalIFFTInstance_.pipeline);
	vkCmdBindDescriptorSets(horizontalIFFTInstance_.commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, horizontalIFFTInstance_.pipelineLayout, 0, 1, &horizontalIFFTInstance_.descriptorSet, 0, 0);

	for (int i = 0; i < 5; i++) {
		horizontalIFFTResources_.pushConstants.iteration = i;
		vkCmdPushConstants(horizontalIFFTInstance_.commandBuffer, horizontalIFFTInstance_.pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(IFFTPushConstants), &horizontalIFFTResources_.pushConstants);
		vkCmdDispatch(horizontalIFFTInstance_.commandBuffer, 1, WATER_RESOLUTION, 1);
	}
	vkCmdPipelineBarrier(horizontalIFFTInstance_.commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memoryBarrier, 0, nullptr, 0, nullptr);
	VK_CHECK(vkEndCommandBuffer(horizontalIFFTInstance_.commandBuffer));

	// Record for vertical IFFT
	cmdBufInfo = Quadbit::VkUtils::Init::CommandBufferBeginInfo();
	VK_CHECK(vkBeginCommandBuffer(verticalIFFTInstance_.commandBuffer, &cmdBufInfo));
	vkCmdBindPipeline(verticalIFFTInstance_.commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, verticalIFFTInstance_.pipeline);
	vkCmdBindDescriptorSets(verticalIFFTInstance_.commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, verticalIFFTInstance_.pipelineLayout, 0, 1, &verticalIFFTInstance_.descriptorSet, 0, 0);
	for (int i = 0; i < 5; i++) {
		verticalIFFTResources_.pushConstants.iteration = i;
		vkCmdPushConstants(verticalIFFTInstance_.commandBuffer, verticalIFFTInstance_.pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(IFFTPushConstants), &verticalIFFTResources_.pushConstants);
		vkCmdDispatch(verticalIFFTInstance_.commandBuffer, 1, WATER_RESOLUTION, 1);
	}
	vkCmdPipelineBarrier(verticalIFFTInstance_.commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memoryBarrier, 0, nullptr, 0, nullptr);
	VK_CHECK(vkEndCommandBuffer(verticalIFFTInstance_.commandBuffer));


	// Record for the displacement compute instance
	VK_CHECK(vkBeginCommandBuffer(displacementInstance_.commandBuffer, &cmdBufInfo));
	vkCmdBindPipeline(displacementInstance_.commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, displacementInstance_.pipeline);
	vkCmdBindDescriptorSets(displacementInstance_.commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, displacementInstance_.pipelineLayout, 0, 1, &displacementInstance_.descriptorSet, 0, 0);
	vkCmdDispatch(displacementInstance_.commandBuffer, WATER_RESOLUTION / 32, WATER_RESOLUTION / 32, 1);
	vkCmdPipelineBarrier(displacementInstance_.commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memoryBarrier, 0, nullptr, 0, nullptr);
	VK_CHECK(vkEndCommandBuffer(displacementInstance_.commandBuffer));

}

void Water::Simulate(float deltaTime) {
	// Update the deltatime 
	UpdateWaveheightUBO(deltaTime);

	// Compute waveheights
	renderer_->ComputeDispatch(waveheightInstance_);

	// IFFT, first a horizontal pass then a vertical pass
	renderer_->ComputeDispatch(horizontalIFFTInstance_);
	renderer_->ComputeDispatch(verticalIFFTInstance_);

	// Finally assemble the displacement map to be used in the vertex shader each frame
	renderer_->ComputeDispatch(displacementInstance_);
}

void Water::DrawFrame() {
	renderer_->DrawFrame();
}

void Water::InitPrecalcComputeInstance() {
	// Create UBO
	renderer_->CreateGPUBuffer(precalcResources_.ubo, sizeof(PrecalcUBO), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, Quadbit::QBVK_MEMORY_USAGE_CPU_TO_GPU);
	// Set initial values for water
	PrecalcUBO* ubo = reinterpret_cast<PrecalcUBO*>(precalcResources_.ubo.alloc.data);
	ubo->N = WATER_RESOLUTION;
	ubo->A = 8000;
	ubo->L = 1000;
	ubo->W = glm::float2(20.0f, 26.0f);

	// Create unif randoms storage buffer 
	VkDeviceSize uniformRandomsSize = WATER_RESOLUTION * WATER_RESOLUTION * sizeof(glm::float4);
	renderer_->CreateGPUBuffer(precalcResources_.uniformRandomsStorageBuffer, uniformRandomsSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, Quadbit::QBVK_MEMORY_USAGE_GPU_ONLY);
	// Precalculate uniform randoms used for the generation of the initial frequency heightmaps
	precalcResources_.precalcUniformRandoms.resize(WATER_RESOLUTION * WATER_RESOLUTION);
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
	// Transfer to the GPU buffer
	renderer_->TransferDataToGPUBuffer(precalcResources_.uniformRandomsStorageBuffer, uniformRandomsSize, precalcResources_.precalcUniformRandoms.data());

	// Create textures
	renderer_->CreateTexture(precalcResources_.h0Tilde, WATER_RESOLUTION, WATER_RESOLUTION, IMAGE_FORMAT, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_STORAGE_BIT,
		VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, Quadbit::QBVK_MEMORY_USAGE_GPU_ONLY);
	renderer_->CreateTexture(precalcResources_.h0TildeConj, WATER_RESOLUTION, WATER_RESOLUTION, IMAGE_FORMAT, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_STORAGE_BIT,
		VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, Quadbit::QBVK_MEMORY_USAGE_GPU_ONLY);

	// Setup precalc compute shader
	std::vector<Quadbit::QbComputeDescriptor> computeDescriptors = {
			Quadbit::VkUtils::CreateComputeDescriptor(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, precalcResources_.ubo.descriptor),
			Quadbit::VkUtils::CreateComputeDescriptor(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, precalcResources_.h0Tilde.descriptor),
			Quadbit::VkUtils::CreateComputeDescriptor(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, precalcResources_.h0TildeConj.descriptor),
			Quadbit::VkUtils::CreateComputeDescriptor(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, precalcResources_.uniformRandomsStorageBuffer.descriptor)
	};
	precalcInstance_ = renderer_->CreateComputeInstance(computeDescriptors, "Resources/Shaders/Compiled/precalc_comp.spv", "main");
}

void Water::InitWaveheightComputeInstance() {
	// Create UBO
	renderer_->CreateGPUBuffer(waveheightResources_.ubo, sizeof(WaveheightUBO), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, Quadbit::QBVK_MEMORY_USAGE_CPU_TO_GPU);
	// Fill with initial data
	WaveheightUBO* ubo = reinterpret_cast<WaveheightUBO*>(waveheightResources_.ubo.alloc.data);
	ubo->N = WATER_RESOLUTION;
	ubo->L = 1000;
	ubo->RT = repeat_;
	ubo->T = 0.0f;

	renderer_->CreateTexture(waveheightResources_.h0TildeTx, WATER_RESOLUTION, WATER_RESOLUTION, IMAGE_FORMAT, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_STORAGE_BIT,
		VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, Quadbit::QBVK_MEMORY_USAGE_GPU_ONLY);
	renderer_->CreateTexture(waveheightResources_.h0TildeTy, WATER_RESOLUTION, WATER_RESOLUTION, IMAGE_FORMAT, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_STORAGE_BIT,
		VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, Quadbit::QBVK_MEMORY_USAGE_GPU_ONLY);
	renderer_->CreateTexture(waveheightResources_.h0TildeTz, WATER_RESOLUTION, WATER_RESOLUTION, IMAGE_FORMAT, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_STORAGE_BIT,
		VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, Quadbit::QBVK_MEMORY_USAGE_GPU_ONLY);
	renderer_->CreateTexture(waveheightResources_.h0TildeSlopeX, WATER_RESOLUTION, WATER_RESOLUTION, IMAGE_FORMAT, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_STORAGE_BIT,
		VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, Quadbit::QBVK_MEMORY_USAGE_GPU_ONLY);
	renderer_->CreateTexture(waveheightResources_.h0TildeSlopeZ, WATER_RESOLUTION, WATER_RESOLUTION, IMAGE_FORMAT, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_STORAGE_BIT,
		VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, Quadbit::QBVK_MEMORY_USAGE_GPU_ONLY);

	std::vector<Quadbit::QbComputeDescriptor> computeDescriptors = {
		Quadbit::VkUtils::CreateComputeDescriptor(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, waveheightResources_.ubo.descriptor),
		Quadbit::VkUtils::CreateComputeDescriptor(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, precalcResources_.h0Tilde.descriptor),
		Quadbit::VkUtils::CreateComputeDescriptor(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, precalcResources_.h0TildeConj.descriptor),
		Quadbit::VkUtils::CreateComputeDescriptor(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, waveheightResources_.h0TildeTx.descriptor),
		Quadbit::VkUtils::CreateComputeDescriptor(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, waveheightResources_.h0TildeTy.descriptor),
		Quadbit::VkUtils::CreateComputeDescriptor(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, waveheightResources_.h0TildeTz.descriptor),
		Quadbit::VkUtils::CreateComputeDescriptor(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, waveheightResources_.h0TildeSlopeX.descriptor),
		Quadbit::VkUtils::CreateComputeDescriptor(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, waveheightResources_.h0TildeSlopeZ.descriptor)
	};

	waveheightInstance_ = renderer_->CreateComputeInstance(computeDescriptors, "Resources/Shaders/Compiled/waveheight_comp.spv", "main");
}

void Water::InitInverseFFTComputeInstances() {
	// Here we will utilize Vulkan specialization maps to dynamically change the size of the IFFT incase resolution changes.
	// The vertical pass property is also set here. This way we avoid having to use a Uniform Buffer.
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

	renderer_->CreateTexture(horizontalIFFTResources_.dX, WATER_RESOLUTION, WATER_RESOLUTION, IMAGE_FORMAT, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_STORAGE_BIT,
		VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, Quadbit::QBVK_MEMORY_USAGE_GPU_ONLY);
	renderer_->CreateTexture(horizontalIFFTResources_.dY, WATER_RESOLUTION, WATER_RESOLUTION, IMAGE_FORMAT, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_STORAGE_BIT,
		VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, Quadbit::QBVK_MEMORY_USAGE_GPU_ONLY);
	renderer_->CreateTexture(horizontalIFFTResources_.dZ, WATER_RESOLUTION, WATER_RESOLUTION, IMAGE_FORMAT, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_STORAGE_BIT,
		VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, Quadbit::QBVK_MEMORY_USAGE_GPU_ONLY);
	renderer_->CreateTexture(horizontalIFFTResources_.dSlopeX, WATER_RESOLUTION, WATER_RESOLUTION, IMAGE_FORMAT, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_STORAGE_BIT,
		VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, Quadbit::QBVK_MEMORY_USAGE_GPU_ONLY);
	renderer_->CreateTexture(horizontalIFFTResources_.dSlopeZ, WATER_RESOLUTION, WATER_RESOLUTION, IMAGE_FORMAT, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_STORAGE_BIT,
		VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, Quadbit::QBVK_MEMORY_USAGE_GPU_ONLY);

	renderer_->CreateTexture(verticalIFFTResources_.dX, WATER_RESOLUTION, WATER_RESOLUTION, IMAGE_FORMAT, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_STORAGE_BIT,
		VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, Quadbit::QBVK_MEMORY_USAGE_GPU_ONLY);
	renderer_->CreateTexture(verticalIFFTResources_.dY, WATER_RESOLUTION, WATER_RESOLUTION, IMAGE_FORMAT, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_STORAGE_BIT,
		VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, Quadbit::QBVK_MEMORY_USAGE_GPU_ONLY);
	renderer_->CreateTexture(verticalIFFTResources_.dZ, WATER_RESOLUTION, WATER_RESOLUTION, IMAGE_FORMAT, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_STORAGE_BIT,
		VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, Quadbit::QBVK_MEMORY_USAGE_GPU_ONLY);
	renderer_->CreateTexture(verticalIFFTResources_.dSlopeX, WATER_RESOLUTION, WATER_RESOLUTION, IMAGE_FORMAT, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_STORAGE_BIT,
		VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, Quadbit::QBVK_MEMORY_USAGE_GPU_ONLY);
	renderer_->CreateTexture(verticalIFFTResources_.dSlopeZ, WATER_RESOLUTION, WATER_RESOLUTION, IMAGE_FORMAT, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_STORAGE_BIT,
		VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, Quadbit::QBVK_MEMORY_USAGE_GPU_ONLY);

	// Setup the vertical/horizontal IFFT compute shaders
	std::vector waveheightImageArray = {
		waveheightResources_.h0TildeTx.descriptor,
		waveheightResources_.h0TildeTy.descriptor,
		waveheightResources_.h0TildeTz.descriptor,
		waveheightResources_.h0TildeSlopeX.descriptor,
		waveheightResources_.h0TildeSlopeZ.descriptor,
	};
	std::vector horizontalImageArray = {
		horizontalIFFTResources_.dX.descriptor,
		horizontalIFFTResources_.dY.descriptor,
		horizontalIFFTResources_.dZ.descriptor,
		horizontalIFFTResources_.dSlopeX.descriptor,
		horizontalIFFTResources_.dSlopeZ.descriptor
	};
	std::vector verticalImageArray = {
		verticalIFFTResources_.dX.descriptor,
		verticalIFFTResources_.dY.descriptor,
		verticalIFFTResources_.dZ.descriptor,
		verticalIFFTResources_.dSlopeX.descriptor,
		verticalIFFTResources_.dSlopeZ.descriptor
	};

	std::vector<Quadbit::QbComputeDescriptor> horizontalComputeDesc = {
		Quadbit::VkUtils::CreateComputeDescriptor(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, waveheightImageArray),
		Quadbit::VkUtils::CreateComputeDescriptor(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, horizontalImageArray)
	};
	std::vector<Quadbit::QbComputeDescriptor> verticalComputeDesc = {
		Quadbit::VkUtils::CreateComputeDescriptor(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, horizontalImageArray),
		Quadbit::VkUtils::CreateComputeDescriptor(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, verticalImageArray)
	};
	
	horizontalIFFTInstance_ = renderer_->CreateComputeInstance(horizontalComputeDesc, "Resources/Shaders/Compiled/ifft_comp.spv", "main", &horizontalSpecInfo, sizeof(IFFTPushConstants));
	verticalIFFTInstance_ = renderer_->CreateComputeInstance(verticalComputeDesc, "Resources/Shaders/Compiled/ifft_comp.spv", "main", &verticalSpecInfo, sizeof(IFFTPushConstants));
}

void Water::InitDisplacementInstance() {
	VkSamplerCreateInfo samplerCreateInfo = Quadbit::VkUtils::Init::SamplerCreateInfo(VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT, 
		VK_TRUE, 16.0f, VK_COMPARE_OP_ALWAYS, VK_SAMPLER_MIPMAP_MODE_LINEAR);

	renderer_->CreateTexture(displacementResources_.displacementMap, WATER_RESOLUTION, WATER_RESOLUTION, IMAGE_FORMAT, VK_IMAGE_TILING_OPTIMAL, 
		VK_IMAGE_USAGE_STORAGE_BIT, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 
		VK_PIPELINE_STAGE_TRANSFER_BIT, Quadbit::QBVK_MEMORY_USAGE_GPU_ONLY);

	renderer_->CreateTexture(displacementResources_.normalMap, WATER_RESOLUTION, WATER_RESOLUTION, IMAGE_FORMAT, VK_IMAGE_TILING_OPTIMAL, 
		VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 
		VK_PIPELINE_STAGE_TRANSFER_BIT, Quadbit::QBVK_MEMORY_USAGE_GPU_ONLY, &samplerCreateInfo);

	// Setup the displacement compute shader
	std::vector<Quadbit::QbComputeDescriptor> computeDescriptors = {
		Quadbit::VkUtils::CreateComputeDescriptor(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, verticalIFFTResources_.dX.descriptor),
		Quadbit::VkUtils::CreateComputeDescriptor(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, verticalIFFTResources_.dY.descriptor),
		Quadbit::VkUtils::CreateComputeDescriptor(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, verticalIFFTResources_.dZ.descriptor),
		Quadbit::VkUtils::CreateComputeDescriptor(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, verticalIFFTResources_.dSlopeX.descriptor),
		Quadbit::VkUtils::CreateComputeDescriptor(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, verticalIFFTResources_.dSlopeZ.descriptor),
		Quadbit::VkUtils::CreateComputeDescriptor(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, displacementResources_.displacementMap.descriptor),
		Quadbit::VkUtils::CreateComputeDescriptor(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, displacementResources_.normalMap.descriptor)
	};

	displacementInstance_ = renderer_->CreateComputeInstance(computeDescriptors, "Resources/Shaders/Compiled/displacement_comp.spv", "main");
}

void Water::UpdateWaveheightUBO(float deltaTime) {
	static float t = 0.0f;

	WaveheightUBO* ubo = reinterpret_cast<WaveheightUBO*>(waveheightResources_.ubo.alloc.data);
	ubo->RT = repeat_;
	ubo->T = t;

	TogglesUBO* togglesUBO = reinterpret_cast<TogglesUBO*>(togglesUBO_.alloc.data);
	togglesUBO->useNormalMap = useNormalMap_ ? 1 : 0;
	t += deltaTime * step_;
}
