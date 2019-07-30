#include <PCH.h>
#include <random>

#include "Water.h"
#include "../Engine/Rendering/Common/QbVkUtils.h"
#include "../Engine/Global/ImGuiState.h"

void Water::Init() {
	InitializeCompute();


	// Initialize vertices and indices for the water mesh
	int resPlus1 = WATER_RESOLUTION + 1;
	waterVertices_.resize(static_cast<std::size_t>(resPlus1) * static_cast<std::size_t>(resPlus1) + 1);
	waterIndices_.resize((static_cast<std::size_t>(resPlus1) - 1) * (static_cast<std::size_t>(resPlus1) - 1) * 6);
	int vertexCount = 0;
	int indexCount = 0;
	for (int x = 0; x < resPlus1; x++) {
		for (int z = 0; z < resPlus1; z++) {
			waterVertices_[vertexCount] = { {x, 0, z}, {0.1f, 0.5f, 0.7f} };
			if (x < resPlus1 - 1 && z < resPlus1 - 1) {
				waterIndices_[indexCount++] = vertexCount;
				waterIndices_[indexCount++] = vertexCount + resPlus1;
				waterIndices_[indexCount++] = vertexCount + resPlus1 + 1;
				waterIndices_[indexCount++] = vertexCount;
				waterIndices_[indexCount++] = vertexCount + resPlus1 + 1;
				waterIndices_[indexCount++] = vertexCount + 1;
			}
			vertexCount++;
		}
	}


	// Our shader will be using the final displacement image calculated in the compute shaders to offset the vertex positions
	VkDescriptorImageInfo displacementMapImageDescInfo{};
	displacementMapImageDescInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	displacementMapImageDescInfo.imageView = Quadbit::VkUtils::CreateImageView(renderer_->RequestRenderContext(),
		displacementResources_.displacementMap.img, IMAGE_FORMAT, VK_IMAGE_ASPECT_COLOR_BIT);

	// Sampler to sample from the normal map image in the fragment shader
	VkSamplerCreateInfo samplerInfo{};
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfo.magFilter = VK_FILTER_LINEAR;
	samplerInfo.minFilter = VK_FILTER_LINEAR;
	samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.anisotropyEnable = VK_TRUE;
	samplerInfo.maxAnisotropy = 16;
	samplerInfo.unnormalizedCoordinates = VK_FALSE;
	samplerInfo.compareEnable = VK_FALSE;
	samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
	samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	VK_CHECK(vkCreateSampler(renderer_->RequestRenderContext()->device, &samplerInfo, nullptr, &displacementResources_.normalMapSampler));

	// And the normal map image itself
	VkDescriptorImageInfo normalMapTextureImageDescInfo{};
	normalMapTextureImageDescInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	normalMapTextureImageDescInfo.imageView = Quadbit::VkUtils::CreateImageView(renderer_->RequestRenderContext(),
		displacementResources_.normalMap.img, IMAGE_FORMAT, VK_IMAGE_ASPECT_COLOR_BIT);
	normalMapTextureImageDescInfo.sampler = displacementResources_.normalMapSampler;

	Quadbit::QbVkRenderMeshInstance* rMeshInstance = renderer_->CreateRenderMeshInstance({
			std::make_tuple(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &displacementMapImageDescInfo, VK_SHADER_STAGE_VERTEX_BIT),
			std::make_tuple(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &normalMapTextureImageDescInfo, VK_SHADER_STAGE_FRAGMENT_BIT)
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
	entity.AddComponent<Quadbit::RenderTransformComponent>(Quadbit::RenderTransformComponent(1.0f, { 1024.0f, 0.0f, 0.0f }, { 0, 0, 0, 1 }));

	entity = entityManager->Create();
	entity.AddComponent<Quadbit::RenderMeshComponent>(renderer_->CreateMesh(waterVertices_, waterIndices_, rMeshInstance));
	entity.AddComponent<Quadbit::RenderTransformComponent>(Quadbit::RenderTransformComponent(1.0f, { 0.0f, 0.0f, 1024.0f }, { 0, 0, 0, 1 }));

	entity = entityManager->Create();
	entity.AddComponent<Quadbit::RenderMeshComponent>(renderer_->CreateMesh(waterVertices_, waterIndices_, rMeshInstance));
	entity.AddComponent<Quadbit::RenderTransformComponent>(Quadbit::RenderTransformComponent(1.0f, { 1024.0f, 0.0f, 1024.0f }, { 0, 0, 0, 1 }));


	// Some ImGui debug stuff 
	// (Can be extended to modify wind and amplitudes, but will need to recreate the initial precalcs every time that happens)
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

	// Initialize the necessary compute instances
	// Precalc runs once.

	// Then for each frame:
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

	VkImageMemoryBarrier h0TildeSlopeXBarrier = Quadbit::VkUtils::Init::ImageMemoryBarrier();
	h0TildeSlopeXBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
	h0TildeSlopeXBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
	h0TildeSlopeXBarrier.image = waveheightResources_.h0TildeSlopeX.img;
	h0TildeSlopeXBarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
	h0TildeSlopeXBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	h0TildeSlopeXBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

	VkImageMemoryBarrier h0TildeSlopeZBarrier = Quadbit::VkUtils::Init::ImageMemoryBarrier();
	h0TildeSlopeZBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
	h0TildeSlopeZBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
	h0TildeSlopeZBarrier.image = waveheightResources_.h0TildeSlopeX.img;
	h0TildeSlopeZBarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
	h0TildeSlopeZBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	h0TildeSlopeZBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

	std::array<VkImageMemoryBarrier, 5> tBarriers { h0TildeTxBarrier, h0TildeTyBarrier, h0TildeTzBarrier, h0TildeSlopeXBarrier, h0TildeSlopeZBarrier };

	vkCmdPipelineBarrier(waveheightInstance_.commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		0, 0, nullptr, 0, nullptr, static_cast<uint32_t>(tBarriers.size()), tBarriers.data());

	VK_CHECK(vkEndCommandBuffer(waveheightInstance_.commandBuffer));

	// Record for the IFFT compute instances
	cmdBufInfo = Quadbit::VkUtils::Init::CommandBufferBeginInfo();
	VK_CHECK(vkBeginCommandBuffer(horizontalIFFTInstance_.commandBuffer, &cmdBufInfo));
	vkCmdBindPipeline(horizontalIFFTInstance_.commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, horizontalIFFTInstance_.pipeline);
	vkCmdBindDescriptorSets(horizontalIFFTInstance_.commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, horizontalIFFTInstance_.pipelineLayout, 0, 1, &horizontalIFFTInstance_.descriptorSet, 0, 0);

	for (int i = 0; i < 5; i++) {
		horizontalIFFTResources_.pushConstants.iteration = i;
		vkCmdPushConstants(horizontalIFFTInstance_.commandBuffer, horizontalIFFTInstance_.pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(IFFTPushConstants), &horizontalIFFTResources_.pushConstants);
		vkCmdDispatch(horizontalIFFTInstance_.commandBuffer, 1, WATER_RESOLUTION, 1);
	}

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

	VkImageMemoryBarrier hDSlopeXBarrier = Quadbit::VkUtils::Init::ImageMemoryBarrier();
	hDSlopeXBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
	hDSlopeXBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
	hDSlopeXBarrier.image = horizontalIFFTResources_.DSlopeX.img;
	hDSlopeXBarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
	hDSlopeXBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	hDSlopeXBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

	VkImageMemoryBarrier hDSlopeZBarrier = Quadbit::VkUtils::Init::ImageMemoryBarrier();
	hDSlopeZBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
	hDSlopeZBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
	hDSlopeZBarrier.image = horizontalIFFTResources_.DSlopeZ.img;
	hDSlopeZBarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
	hDSlopeZBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	hDSlopeZBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

	std::array<VkImageMemoryBarrier, 5> hBarriers { hDxBarrier, hDyBarrier, hDzBarrier, hDSlopeXBarrier, hDSlopeZBarrier };

	vkCmdPipelineBarrier(horizontalIFFTInstance_.commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		0, 0, nullptr, 0, nullptr, static_cast<uint32_t>(hBarriers.size()), hBarriers.data());

	VK_CHECK(vkEndCommandBuffer(horizontalIFFTInstance_.commandBuffer));

	cmdBufInfo = Quadbit::VkUtils::Init::CommandBufferBeginInfo();
	VK_CHECK(vkBeginCommandBuffer(verticalIFFTInstance_.commandBuffer, &cmdBufInfo));
	vkCmdBindPipeline(verticalIFFTInstance_.commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, verticalIFFTInstance_.pipeline);
	vkCmdBindDescriptorSets(verticalIFFTInstance_.commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, verticalIFFTInstance_.pipelineLayout, 0, 1, &verticalIFFTInstance_.descriptorSet, 0, 0);

	for (int i = 0; i < 5; i++) {
		verticalIFFTResources_.pushConstants.iteration = i;
		vkCmdPushConstants(verticalIFFTInstance_.commandBuffer, verticalIFFTInstance_.pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(IFFTPushConstants), &verticalIFFTResources_.pushConstants);
		vkCmdDispatch(verticalIFFTInstance_.commandBuffer, 1, WATER_RESOLUTION, 1);
	}

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

	VkImageMemoryBarrier vDSlopeXBarrier = Quadbit::VkUtils::Init::ImageMemoryBarrier();
	vDSlopeXBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
	vDSlopeXBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
	vDSlopeXBarrier.image = verticalIFFTResources_.DSlopeX.img;
	vDSlopeXBarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
	vDSlopeXBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	vDSlopeXBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

	VkImageMemoryBarrier vDSlopeZBarrier = Quadbit::VkUtils::Init::ImageMemoryBarrier();
	vDSlopeZBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
	vDSlopeZBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
	vDSlopeZBarrier.image = verticalIFFTResources_.DSlopeZ.img;
	vDSlopeZBarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
	vDSlopeZBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	vDSlopeZBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

	std::array<VkImageMemoryBarrier, 5> vBarriers{ vDxBarrier, vDyBarrier, vDzBarrier, vDSlopeXBarrier, vDSlopeZBarrier };

	vkCmdPipelineBarrier(verticalIFFTInstance_.commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		0, 0, nullptr, 0, nullptr, static_cast<uint32_t>(vBarriers.size()), vBarriers.data());

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
		0, 0, nullptr, 0, nullptr, static_cast<uint32_t>(vBarriers.size()), vBarriers.data());

	VkImageMemoryBarrier dispBarrier = Quadbit::VkUtils::Init::ImageMemoryBarrier();
	dispBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
	dispBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
	dispBarrier.image = displacementResources_.displacementMap.img;
	dispBarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
	dispBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	dispBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

	VkImageMemoryBarrier normBarrier = Quadbit::VkUtils::Init::ImageMemoryBarrier();
	normBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
	normBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
	normBarrier.image = displacementResources_.normalMap.img;
	normBarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
	normBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	normBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

	vkCmdPipelineBarrier(displacementInstance_.commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		0, 0, nullptr, 0, nullptr, 1, &normBarrier);

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
	// Get render context
	auto context = renderer_->RequestRenderContext();

	// Allocate uniform buffer for the compute shader
	VkBufferCreateInfo bufferCreateInfo = Quadbit::VkUtils::Init::BufferCreateInfo(sizeof(PrecalcUBO), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
	context->allocator->CreateBuffer(precalcResources_.ubo, bufferCreateInfo, Quadbit::QBVK_MEMORY_USAGE_CPU_TO_GPU);

	// Set initial values for water
	PrecalcUBO* ubo = reinterpret_cast<PrecalcUBO*>(precalcResources_.ubo.alloc.data);
	ubo->N = WATER_RESOLUTION;
	ubo->A = 150.3e-9;
	ubo->L = 1300;
	ubo->W = glm::float2(16.0f, 24.0f);

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

	// Utilize a staging buffer to transfer the data onto the GPU
	Quadbit::QbVkBuffer uniformRandomsStagingBuffer;
	context->allocator->CreateStagingBuffer(uniformRandomsStagingBuffer, uniformRandomsSize, precalcResources_.precalcUniformRandoms.data());
	Quadbit::VkUtils::CopyBuffer(context, uniformRandomsStagingBuffer.buf, precalcResources_.uniformRandomsStorageBuffer.buf, uniformRandomsSize);
	context->allocator->DestroyBuffer(uniformRandomsStagingBuffer);

	// Fill descriptors
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

	// Transition images from undefined layout and also copy the uniform randoms into the buffer on the GPU
	VkCommandBuffer tempBuffer = Quadbit::VkUtils::InitSingleTimeCommandBuffer(context);

	Quadbit::VkUtils::TransitionImageLayout(context, tempBuffer, precalcResources_.h0Tilde.img, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
	Quadbit::VkUtils::TransitionImageLayout(context, tempBuffer, precalcResources_.h0TildeConj.img, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

	Quadbit::VkUtils::FlushCommandBuffer(context, tempBuffer);

	// Setup precalc compute shader
	std::vector<std::tuple<VkDescriptorType, std::vector<void*>>> computeDescriptors {
			std::make_tuple(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, std::vector<void*> { &descBufferInfo }),
			std::make_tuple(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, std::vector<void*> { &h0TildeDescImageInfo }),
			std::make_tuple(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, std::vector<void*> { &h0TildeConjDescImageInfo }),
			std::make_tuple(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, std::vector<void*> { &descUniformRandomsStorageBufferInfo })
	};
	precalcInstance_ = renderer_->CreateComputeInstance(computeDescriptors, "Resources/Shaders/Compiled/precalc_comp.spv", "main");
}

void Water::InitWaveheightComputeInstance() {
	// Get render context
	auto context = renderer_->RequestRenderContext();

	// Allocate uniform buffer for the compute shader
	VkBufferCreateInfo bufferCreateInfo = Quadbit::VkUtils::Init::BufferCreateInfo(sizeof(WaveheightUBO), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
	context->allocator->CreateBuffer(waveheightResources_.ubo, bufferCreateInfo, Quadbit::QBVK_MEMORY_USAGE_CPU_TO_GPU);

	WaveheightUBO* ubo = reinterpret_cast<WaveheightUBO*>(waveheightResources_.ubo.alloc.data);
	ubo->N = WATER_RESOLUTION;
	ubo->L = 1300;
	ubo->RT = repeat_;
	ubo->T = 0.0f;

	// Create images
	VkImageCreateInfo imageCreateInfo = Quadbit::VkUtils::Init::ImageCreateInfo(WATER_RESOLUTION, WATER_RESOLUTION, IMAGE_FORMAT, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_STORAGE_BIT);
	context->allocator->CreateImage(waveheightResources_.h0TildeTx, imageCreateInfo, Quadbit::QBVK_MEMORY_USAGE_GPU_ONLY);
	context->allocator->CreateImage(waveheightResources_.h0TildeTy, imageCreateInfo, Quadbit::QBVK_MEMORY_USAGE_GPU_ONLY);
	context->allocator->CreateImage(waveheightResources_.h0TildeTz, imageCreateInfo, Quadbit::QBVK_MEMORY_USAGE_GPU_ONLY);
	context->allocator->CreateImage(waveheightResources_.h0TildeSlopeX, imageCreateInfo, Quadbit::QBVK_MEMORY_USAGE_GPU_ONLY);
	context->allocator->CreateImage(waveheightResources_.h0TildeSlopeZ, imageCreateInfo, Quadbit::QBVK_MEMORY_USAGE_GPU_ONLY);

	// Create imageviews
	waveheightResources_.h0TildeTxImgInfo.imageView = Quadbit::VkUtils::CreateImageView(context, waveheightResources_.h0TildeTx.img, IMAGE_FORMAT, VK_IMAGE_ASPECT_COLOR_BIT);
	waveheightResources_.h0TildeTyImgInfo.imageView = Quadbit::VkUtils::CreateImageView(context, waveheightResources_.h0TildeTy.img, IMAGE_FORMAT, VK_IMAGE_ASPECT_COLOR_BIT);
	waveheightResources_.h0TildeTzImgInfo.imageView = Quadbit::VkUtils::CreateImageView(context, waveheightResources_.h0TildeTz.img, IMAGE_FORMAT, VK_IMAGE_ASPECT_COLOR_BIT);
	waveheightResources_.h0TildeSlopeXImgInfo.imageView = Quadbit::VkUtils::CreateImageView(context, waveheightResources_.h0TildeSlopeX.img, IMAGE_FORMAT, VK_IMAGE_ASPECT_COLOR_BIT);
	waveheightResources_.h0TildeSlopeZImgInfo.imageView = Quadbit::VkUtils::CreateImageView(context, waveheightResources_.h0TildeSlopeZ.img, IMAGE_FORMAT, VK_IMAGE_ASPECT_COLOR_BIT);

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

	// Transition images from undefined layout and also copy the uniform randoms into the buffer on the GPU
	VkCommandBuffer tempBuffer = Quadbit::VkUtils::InitSingleTimeCommandBuffer(context);

	Quadbit::VkUtils::TransitionImageLayout(context, tempBuffer, waveheightResources_.h0TildeTx.img, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
	Quadbit::VkUtils::TransitionImageLayout(context, tempBuffer, waveheightResources_.h0TildeTy.img, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
	Quadbit::VkUtils::TransitionImageLayout(context, tempBuffer, waveheightResources_.h0TildeTz.img, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
	Quadbit::VkUtils::TransitionImageLayout(context, tempBuffer, waveheightResources_.h0TildeSlopeX.img, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
	Quadbit::VkUtils::TransitionImageLayout(context, tempBuffer, waveheightResources_.h0TildeSlopeZ.img, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

	Quadbit::VkUtils::FlushCommandBuffer(context, tempBuffer);

	// Setup waveheight shader
	std::vector<std::tuple<VkDescriptorType, std::vector<void*>>> computeDescriptors{
			std::make_tuple(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, std::vector<void*> { &descBufferInfo }),
			std::make_tuple(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,  std::vector<void*> { &h0TildeDescImageInfo }),
			std::make_tuple(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,  std::vector<void*> { &h0TildeConjDescImageInfo }),
			std::make_tuple(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,  std::vector<void*> { &waveheightResources_.h0TildeTxImgInfo }),
			std::make_tuple(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,  std::vector<void*> { &waveheightResources_.h0TildeTyImgInfo }),
			std::make_tuple(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,  std::vector<void*> { &waveheightResources_.h0TildeTzImgInfo }),
			std::make_tuple(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,  std::vector<void*> { &waveheightResources_.h0TildeSlopeXImgInfo }),
			std::make_tuple(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,  std::vector<void*> { &waveheightResources_.h0TildeSlopeZImgInfo })
	};
	waveheightInstance_ = renderer_->CreateComputeInstance(computeDescriptors, "Resources/Shaders/Compiled/waveheight_comp.spv", "main");
}

void Water::InitInverseFFTComputeInstances() {
	// Get render context
	auto context = renderer_->RequestRenderContext();

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

	// Create images	
	VkImageCreateInfo imageCreateInfo = Quadbit::VkUtils::Init::ImageCreateInfo(WATER_RESOLUTION, WATER_RESOLUTION, IMAGE_FORMAT, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_STORAGE_BIT);
	context->allocator->CreateImage(horizontalIFFTResources_.Dx, imageCreateInfo, Quadbit::QBVK_MEMORY_USAGE_GPU_ONLY);
	context->allocator->CreateImage(verticalIFFTResources_.Dx, imageCreateInfo, Quadbit::QBVK_MEMORY_USAGE_GPU_ONLY);
	context->allocator->CreateImage(horizontalIFFTResources_.Dy, imageCreateInfo, Quadbit::QBVK_MEMORY_USAGE_GPU_ONLY);
	context->allocator->CreateImage(verticalIFFTResources_.Dy, imageCreateInfo, Quadbit::QBVK_MEMORY_USAGE_GPU_ONLY);
	context->allocator->CreateImage(horizontalIFFTResources_.Dz, imageCreateInfo, Quadbit::QBVK_MEMORY_USAGE_GPU_ONLY);
	context->allocator->CreateImage(verticalIFFTResources_.Dz, imageCreateInfo, Quadbit::QBVK_MEMORY_USAGE_GPU_ONLY);
	context->allocator->CreateImage(horizontalIFFTResources_.DSlopeX, imageCreateInfo, Quadbit::QBVK_MEMORY_USAGE_GPU_ONLY);
	context->allocator->CreateImage(verticalIFFTResources_.DSlopeX, imageCreateInfo, Quadbit::QBVK_MEMORY_USAGE_GPU_ONLY);
	context->allocator->CreateImage(horizontalIFFTResources_.DSlopeZ, imageCreateInfo, Quadbit::QBVK_MEMORY_USAGE_GPU_ONLY);
	context->allocator->CreateImage(verticalIFFTResources_.DSlopeZ, imageCreateInfo, Quadbit::QBVK_MEMORY_USAGE_GPU_ONLY);

	// Create image views
	horizontalIFFTResources_.DxImgInfo.imageView = Quadbit::VkUtils::CreateImageView(context, horizontalIFFTResources_.Dx.img, IMAGE_FORMAT, VK_IMAGE_ASPECT_COLOR_BIT);
	horizontalIFFTResources_.DyImgInfo.imageView = Quadbit::VkUtils::CreateImageView(context, horizontalIFFTResources_.Dy.img, IMAGE_FORMAT, VK_IMAGE_ASPECT_COLOR_BIT);
	horizontalIFFTResources_.DzImgInfo.imageView = Quadbit::VkUtils::CreateImageView(context, horizontalIFFTResources_.Dz.img, IMAGE_FORMAT, VK_IMAGE_ASPECT_COLOR_BIT);
	horizontalIFFTResources_.DSlopeXImgInfo.imageView = Quadbit::VkUtils::CreateImageView(context, horizontalIFFTResources_.DSlopeX.img, IMAGE_FORMAT, VK_IMAGE_ASPECT_COLOR_BIT);
	horizontalIFFTResources_.DSlopeZImgInfo.imageView = Quadbit::VkUtils::CreateImageView(context, horizontalIFFTResources_.DSlopeZ.img, IMAGE_FORMAT, VK_IMAGE_ASPECT_COLOR_BIT);
	verticalIFFTResources_.DxImgInfo.imageView = Quadbit::VkUtils::CreateImageView(context, verticalIFFTResources_.Dx.img, IMAGE_FORMAT, VK_IMAGE_ASPECT_COLOR_BIT);
	verticalIFFTResources_.DyImgInfo.imageView = Quadbit::VkUtils::CreateImageView(context, verticalIFFTResources_.Dy.img, IMAGE_FORMAT, VK_IMAGE_ASPECT_COLOR_BIT);
	verticalIFFTResources_.DzImgInfo.imageView = Quadbit::VkUtils::CreateImageView(context, verticalIFFTResources_.Dz.img, IMAGE_FORMAT, VK_IMAGE_ASPECT_COLOR_BIT);
	verticalIFFTResources_.DSlopeXImgInfo.imageView = Quadbit::VkUtils::CreateImageView(context, verticalIFFTResources_.DSlopeX.img, IMAGE_FORMAT, VK_IMAGE_ASPECT_COLOR_BIT);
	verticalIFFTResources_.DSlopeZImgInfo.imageView = Quadbit::VkUtils::CreateImageView(context, verticalIFFTResources_.DSlopeZ.img, IMAGE_FORMAT, VK_IMAGE_ASPECT_COLOR_BIT);

	// Transition images to VK_IMAGE_LAYOUT_GENERAL
	VkCommandBuffer tempBuffer = Quadbit::VkUtils::InitSingleTimeCommandBuffer(context);
	Quadbit::VkUtils::TransitionImageLayout(context, tempBuffer, horizontalIFFTResources_.Dx.img, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
	Quadbit::VkUtils::TransitionImageLayout(context, tempBuffer, horizontalIFFTResources_.Dy.img, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
	Quadbit::VkUtils::TransitionImageLayout(context, tempBuffer, horizontalIFFTResources_.Dz.img, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
	Quadbit::VkUtils::TransitionImageLayout(context, tempBuffer, horizontalIFFTResources_.DSlopeX.img, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
	Quadbit::VkUtils::TransitionImageLayout(context, tempBuffer, horizontalIFFTResources_.DSlopeZ.img, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
	Quadbit::VkUtils::TransitionImageLayout(context, tempBuffer, verticalIFFTResources_.Dx.img, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
	Quadbit::VkUtils::TransitionImageLayout(context, tempBuffer, verticalIFFTResources_.Dy.img, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
	Quadbit::VkUtils::TransitionImageLayout(context, tempBuffer, verticalIFFTResources_.Dz.img, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
	Quadbit::VkUtils::TransitionImageLayout(context, tempBuffer, verticalIFFTResources_.DSlopeX.img, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
	Quadbit::VkUtils::TransitionImageLayout(context, tempBuffer, verticalIFFTResources_.DSlopeZ.img, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
	Quadbit::VkUtils::FlushCommandBuffer(context, tempBuffer);

	// Setup the vertical/horizontal IFFT compute shaders
	std::vector<std::tuple<VkDescriptorType, std::vector<void*>>> horizontalComputeDesc {
		std::make_tuple(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, std::vector<void*> { 
			&waveheightResources_.h0TildeTxImgInfo,	
			&waveheightResources_.h0TildeTyImgInfo,	
			&waveheightResources_.h0TildeTzImgInfo,	
			&waveheightResources_.h0TildeSlopeXImgInfo,
			&waveheightResources_.h0TildeSlopeZImgInfo,
		}),
		std::make_tuple(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, std::vector<void*> {
			&horizontalIFFTResources_.DxImgInfo,
			&horizontalIFFTResources_.DyImgInfo,
			&horizontalIFFTResources_.DzImgInfo,
			&horizontalIFFTResources_.DSlopeXImgInfo,
			&horizontalIFFTResources_.DSlopeZImgInfo
		})
	};
	
	std::vector<std::tuple<VkDescriptorType, std::vector<void*>>> verticalComputeDesc {
		std::make_tuple(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, std::vector<void*> {
			&horizontalIFFTResources_.DxImgInfo,		
			&horizontalIFFTResources_.DyImgInfo,		
			&horizontalIFFTResources_.DzImgInfo,		
			&horizontalIFFTResources_.DSlopeXImgInfo,	
			&horizontalIFFTResources_.DSlopeZImgInfo,	
		}), 
		std::make_tuple(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, std::vector<void*> {
			&verticalIFFTResources_.DxImgInfo,
			&verticalIFFTResources_.DyImgInfo,
			&verticalIFFTResources_.DzImgInfo,
			&verticalIFFTResources_.DSlopeXImgInfo,
			&verticalIFFTResources_.DSlopeZImgInfo
		})
	};
	
	horizontalIFFTInstance_ = renderer_->CreateComputeInstance(horizontalComputeDesc, "Resources/Shaders/Compiled/ifft_comp.spv", "main", &horizontalSpecInfo, sizeof(IFFTPushConstants));
	verticalIFFTInstance_ = renderer_->CreateComputeInstance(verticalComputeDesc, "Resources/Shaders/Compiled/ifft_comp.spv", "main", &verticalSpecInfo, sizeof(IFFTPushConstants));
}

void Water::InitDisplacementInstance() {
	// Get render context
	auto context = renderer_->RequestRenderContext();

	// Create the final displacement image and the normal map image
	VkImageCreateInfo imageCreateInfo = Quadbit::VkUtils::Init::ImageCreateInfo(WATER_RESOLUTION, WATER_RESOLUTION, IMAGE_FORMAT, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_STORAGE_BIT);
	VkImageCreateInfo normalMapImageCreateInfo = Quadbit::VkUtils::Init::ImageCreateInfo(WATER_RESOLUTION, WATER_RESOLUTION, IMAGE_FORMAT, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
	context->allocator->CreateImage(displacementResources_.displacementMap, imageCreateInfo, Quadbit::QBVK_MEMORY_USAGE_GPU_ONLY);
	context->allocator->CreateImage(displacementResources_.normalMap, normalMapImageCreateInfo, Quadbit::QBVK_MEMORY_USAGE_GPU_ONLY);

	// Fill descriptors
	VkDescriptorImageInfo verticalDxDescImageInfo{};
	verticalDxDescImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	verticalDxDescImageInfo.imageView = Quadbit::VkUtils::CreateImageView(context, verticalIFFTResources_.Dx.img, IMAGE_FORMAT, VK_IMAGE_ASPECT_COLOR_BIT);
	VkDescriptorImageInfo verticalDyDescImageInfo{};
	verticalDyDescImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	verticalDyDescImageInfo.imageView = Quadbit::VkUtils::CreateImageView(context, verticalIFFTResources_.Dy.img, IMAGE_FORMAT, VK_IMAGE_ASPECT_COLOR_BIT);
	VkDescriptorImageInfo verticalDzDescImageInfo{};
	verticalDzDescImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	verticalDzDescImageInfo.imageView = Quadbit::VkUtils::CreateImageView(context, verticalIFFTResources_.Dz.img, IMAGE_FORMAT, VK_IMAGE_ASPECT_COLOR_BIT);
	VkDescriptorImageInfo verticalDSlopeXDescImageInfo{};
	verticalDSlopeXDescImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	verticalDSlopeXDescImageInfo.imageView = Quadbit::VkUtils::CreateImageView(context, verticalIFFTResources_.DSlopeX.img, IMAGE_FORMAT, VK_IMAGE_ASPECT_COLOR_BIT);
	VkDescriptorImageInfo verticalDSlopeZDescImageInfo{};
	verticalDSlopeZDescImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	verticalDSlopeZDescImageInfo.imageView = Quadbit::VkUtils::CreateImageView(context, verticalIFFTResources_.DSlopeZ.img, IMAGE_FORMAT, VK_IMAGE_ASPECT_COLOR_BIT);

	VkDescriptorImageInfo displacementDescImageInfo{};
	displacementDescImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	displacementDescImageInfo.imageView = Quadbit::VkUtils::CreateImageView(context, displacementResources_.displacementMap.img, IMAGE_FORMAT, VK_IMAGE_ASPECT_COLOR_BIT);

	VkDescriptorImageInfo normalMapDescImageInfo{};
	normalMapDescImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	normalMapDescImageInfo.imageView = Quadbit::VkUtils::CreateImageView(context, displacementResources_.normalMap.img, IMAGE_FORMAT, VK_IMAGE_ASPECT_COLOR_BIT);


	// Transition images from undefined layout and also copy the uniform randoms into the buffer on the GPU
	VkCommandBuffer tempBuffer = Quadbit::VkUtils::InitSingleTimeCommandBuffer(context);
	Quadbit::VkUtils::TransitionImageLayout(context, tempBuffer, displacementResources_.displacementMap.img, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
	Quadbit::VkUtils::TransitionImageLayout(context, tempBuffer, displacementResources_.normalMap.img, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
	Quadbit::VkUtils::FlushCommandBuffer(context, tempBuffer);

	// Setup the displacement compute shader
	std::vector<std::tuple<VkDescriptorType, std::vector<void*>>> computeDescriptors {
			std::make_tuple(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, std::vector<void*> { &verticalDxDescImageInfo }),
			std::make_tuple(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, std::vector<void*> { &verticalDyDescImageInfo }),
			std::make_tuple(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, std::vector<void*> { &verticalDzDescImageInfo }),
			std::make_tuple(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, std::vector<void*> { &verticalDSlopeXDescImageInfo }),
			std::make_tuple(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, std::vector<void*> { &verticalDSlopeZDescImageInfo }),
			std::make_tuple(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, std::vector<void*> { &displacementDescImageInfo }),
			std::make_tuple(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, std::vector<void*> { &normalMapDescImageInfo })
	};

	displacementInstance_ = renderer_->CreateComputeInstance(computeDescriptors, "Resources/Shaders/Compiled/displacement_comp.spv", "main");
}

void Water::UpdateWaveheightUBO(float deltaTime) {
	static float t = 0.0f;

	WaveheightUBO* ubo = reinterpret_cast<WaveheightUBO*>(waveheightResources_.ubo.alloc.data);
	ubo->RT = repeat_;
	ubo->T = t;

	t += deltaTime * step_;
}
