#include "Water.h"

#include <random>
#include <EASTL/sort.h>
#include <imgui/imgui.h>

#include "Engine/Entities/EntityTypes.h"

void Water::Init() {
	// Create skybox
	//graphics_->LoadSkyGradient(glm::vec3(1.0f, 1.0f, 1.0f), glm::vec3(0.1f, 0.4f, 0.8f));

	InitializeCompute();

	// Initialize vertices and indices for the water mesh
	int meshBounds = WATER_RESOLUTION + 1;
	waterVertices_.resize(static_cast<std::size_t>(meshBounds) * static_cast<std::size_t>(meshBounds) + 1);
	waterIndices_.resize((static_cast<std::size_t>(meshBounds) - 1) * (static_cast<std::size_t>(meshBounds) - 1) * 6);
	int vertexCount = 0;
	int indexCount = 0;
	for (int x = 0; x < meshBounds; x++) {
		for (int z = 0; z < meshBounds; z++) {
			waterVertices_[vertexCount] = { x, 0, z };
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
	togglesUBO_ = graphics_->CreateMappedGPUBuffer<TogglesUBO>(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
	togglesUBO_->useNormalMap = 0;

	Quadbit::QbVkPipelineDescription pipelineDescription;
	pipelineDescription.colorBlending = Quadbit::QbVkPipelineColorBlending::QBVK_COLORBLENDING_DISABLE;
	pipelineDescription.depth = Quadbit::QbVkPipelineDepth::QBVK_PIPELINE_DEPTH_ENABLE;
	pipelineDescription.dynamicState = Quadbit::QbVkPipelineDynamicState::QBVK_DYNAMICSTATE_VIEWPORTSCISSOR;
	pipelineDescription.enableMSAA = true;
	pipelineDescription.rasterization = Quadbit::QbVkPipelineRasterization::QBVK_PIPELINE_RASTERIZATION_DEFAULT;
	pipeline_ = graphics_->CreatePipeline("Resources/Shaders/Compiled/water_vert.spv", "Resources/Shaders/Compiled/water_frag.spv", pipelineDescription);
	graphics_->BindResource(pipeline_, "normal_map", displacementResources_.normalMap);
	graphics_->BindResource(pipeline_, "displacement_map", displacementResources_.displacementMap);
	graphics_->BindResource(pipeline_, "UBO", togglesUBO_.handle);
	
	//eastl::vector<Quadbit::QbVkRenderDescriptor> renderDescriptors {
	//	graphics_->CreateRenderDescriptor(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, graphics_->GetDescriptorPtr(displacementResources_.displacementMap), VK_SHADER_STAGE_VERTEX_BIT),
	//	graphics_->CreateRenderDescriptor(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, graphics_->GetDescriptorPtr(displacementResources_.normalMap), VK_SHADER_STAGE_FRAGMENT_BIT),
	//	graphics_->CreateRenderDescriptor(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, graphics_->GetDescriptorPtr(togglesUBO_), VK_SHADER_STAGE_FRAGMENT_BIT),
	//};

	//Quadbit::QbVkShaderInstance shaderInstance = graphics_->CreateShaderInstance();
	//shaderInstance.AddShader("Resources/Shaders/Compiled/water_vert.spv", "main", VK_SHADER_STAGE_VERTEX_BIT);
	//shaderInstance.AddShader("Resources/Shaders/Compiled/water_frag.spv", "main", VK_SHADER_STAGE_FRAGMENT_BIT);

	//const Quadbit::QbVkRenderMeshInstance* rMeshInstance = graphics_->CreateRenderMeshInstance(renderDescriptors,
	//	{
	//		Quadbit::QbVkVertexInputAttribute::QBVK_VERTEX_ATTRIBUTE_POSITION 
	//	},
	//	shaderInstance
	//);

	for(auto i = 0; i < WATER_RESOLUTION * 2; i += WATER_RESOLUTION) {
		for(auto j = 0; j < WATER_RESOLUTION * 2; j += WATER_RESOLUTION) {
			auto entity = entityManager_->Create();
			entityManager_->AddComponent<Quadbit::CustomMeshComponent>(entity, graphics_->CreateMesh(waterVertices_, sizeof(glm::float3), waterIndices_, pipeline_));
			entityManager_->AddComponent<Quadbit::RenderTransformComponent>(entity, Quadbit::RenderTransformComponent(1.0f, { i, 0.0f, j }, { 0, 0, 0, 1 }));
		}
	}
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
	compute_->ComputeDispatch(precalcInstance_);
}

// This function records all the compute commands that will be run by each individual compute shader
// The barriers are there to make sure images are only written/read from when they are available
void Water::RecordComputeCommands() {
	// Global memory barrier used throughout
	VkMemoryBarrier memoryBarrier = graphics_->CreateMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);

	compute_->ComputeRecord(precalcInstance_, [&]() {
		vkCmdBindPipeline(precalcInstance_->commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, precalcInstance_->pipeline);
		vkCmdBindDescriptorSets(precalcInstance_->commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, precalcInstance_->pipelineLayout, 0, 1, &precalcInstance_->descriptorSet, 0, 0);
		vkCmdDispatch(precalcInstance_->commandBuffer, WATER_RESOLUTION / 32, WATER_RESOLUTION / 32, 1);
		});

	compute_->ComputeRecord(waveheightInstance_, [&]() {
		vkCmdBindPipeline(waveheightInstance_->commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, waveheightInstance_->pipeline);
		vkCmdBindDescriptorSets(waveheightInstance_->commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, waveheightInstance_->pipelineLayout, 0, 1, &waveheightInstance_->descriptorSet, 0, 0);
		vkCmdDispatch(waveheightInstance_->commandBuffer, WATER_RESOLUTION / 32, WATER_RESOLUTION / 32, 1);
		vkCmdPipelineBarrier(waveheightInstance_->commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memoryBarrier, 0, nullptr, 0, nullptr);
		});

	compute_->ComputeRecord(horizontalIFFTInstance_, [&]() {
		vkCmdBindPipeline(horizontalIFFTInstance_->commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, horizontalIFFTInstance_->pipeline);
		vkCmdBindDescriptorSets(horizontalIFFTInstance_->commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, horizontalIFFTInstance_->pipelineLayout, 0, 1, &horizontalIFFTInstance_->descriptorSet, 0, 0);
		for (auto i = 0; i < 5; i++) {
			horizontalIFFTResources_.pushConstants.iteration = i;
			vkCmdPushConstants(horizontalIFFTInstance_->commandBuffer, horizontalIFFTInstance_->pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(IFFTPushConstants), &horizontalIFFTResources_.pushConstants);
			vkCmdDispatch(horizontalIFFTInstance_->commandBuffer, 1, WATER_RESOLUTION, 1);
		}
		vkCmdPipelineBarrier(horizontalIFFTInstance_->commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memoryBarrier, 0, nullptr, 0, nullptr);
		});

	compute_->ComputeRecord(verticalIFFTInstance_, [&]() {
		vkCmdBindPipeline(verticalIFFTInstance_->commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, verticalIFFTInstance_->pipeline);
		vkCmdBindDescriptorSets(verticalIFFTInstance_->commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, verticalIFFTInstance_->pipelineLayout, 0, 1, &verticalIFFTInstance_->descriptorSet, 0, 0);
		for (auto i = 0; i < 5; i++) {
			verticalIFFTResources_.pushConstants.iteration = i;
			vkCmdPushConstants(verticalIFFTInstance_->commandBuffer, verticalIFFTInstance_->pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(IFFTPushConstants), &verticalIFFTResources_.pushConstants);
			vkCmdDispatch(verticalIFFTInstance_->commandBuffer, 1, WATER_RESOLUTION, 1);
		}
		vkCmdPipelineBarrier(verticalIFFTInstance_->commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memoryBarrier, 0, nullptr, 0, nullptr);
		});

	compute_->ComputeRecord(displacementInstance_, [&]() {
		vkCmdBindPipeline(displacementInstance_->commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, displacementInstance_->pipeline);
		vkCmdBindDescriptorSets(displacementInstance_->commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, displacementInstance_->pipelineLayout, 0, 1, &displacementInstance_->descriptorSet, 0, 0);
		vkCmdDispatch(displacementInstance_->commandBuffer, WATER_RESOLUTION / 32, WATER_RESOLUTION / 32, 1);
		vkCmdPipelineBarrier(displacementInstance_->commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memoryBarrier, 0, nullptr, 0, nullptr);
	});
}

void Water::Simulate(float deltaTime) {
	// Update toggles
	UpdateTogglesUBO(deltaTime);

	// Increment the time in the wave functions 
	UpdateWaveheightUBO(deltaTime);

	// Compute waveheights
	compute_->ComputeDispatch(waveheightInstance_);

	// IFFT, first a horizontal pass then a vertical pass
	compute_->ComputeDispatch(horizontalIFFTInstance_);
	compute_->ComputeDispatch(verticalIFFTInstance_);

	// Finally assemble the displacement map to be used in the vertex shader each frame
	compute_->ComputeDispatch(displacementInstance_);

	DrawImGui();
}

void Water::InitPrecalcComputeInstance() {
	// Create UBO
	precalcResources_.ubo = graphics_->CreateMappedGPUBuffer<PrecalcUBO>(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
	// Set initial values for water
	precalcResources_.ubo->N = WATER_RESOLUTION;
	precalcResources_.ubo->A = 12000;
	precalcResources_.ubo->L = 1000;
	precalcResources_.ubo->W = glm::float2(18.0f, 24.0f);
	// Create unif randoms storage buffer 
	VkDeviceSize uniformRandomsSize = WATER_RESOLUTION * WATER_RESOLUTION * sizeof(glm::float4);
	precalcResources_.uniformRandomsStorageBuffer = graphics_->CreateGPUBuffer(uniformRandomsSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		Quadbit::QbVkMemoryUsage::QBVK_MEMORY_USAGE_GPU_ONLY);
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
	graphics_->TransferDataToGPUBuffer(precalcResources_.precalcUniformRandoms.data(), uniformRandomsSize, precalcResources_.uniformRandomsStorageBuffer);

	// Create textures
	precalcResources_.h0Tilde = graphics_->CreateStorageTexture(WATER_RESOLUTION, WATER_RESOLUTION, IMAGE_FORMAT);
	precalcResources_.h0TildeConj = graphics_->CreateStorageTexture(WATER_RESOLUTION, WATER_RESOLUTION, IMAGE_FORMAT);

	// Setup precalc compute shader
	eastl::vector<Quadbit::QbVkComputeDescriptor> computeDescriptors = {
			compute_->CreateComputeDescriptor(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, graphics_->GetDescriptorPtr(precalcResources_.ubo)),
			compute_->CreateComputeDescriptor(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,  graphics_->GetDescriptorPtr(precalcResources_.h0Tilde)),
			compute_->CreateComputeDescriptor(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,  graphics_->GetDescriptorPtr(precalcResources_.h0TildeConj)),
			compute_->CreateComputeDescriptor(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, graphics_->GetDescriptorPtr(precalcResources_.uniformRandomsStorageBuffer))
	};
	precalcInstance_ = compute_->CreateComputeInstance(computeDescriptors, "Resources/Shaders/Compiled/precalc_comp.spv", "main");
}

void Water::InitWaveheightComputeInstance() {
	// Create UBO
	waveheightResources_.ubo = graphics_->CreateMappedGPUBuffer<WaveheightUBO>(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
	// Fill with initial data
	waveheightResources_.ubo->N = WATER_RESOLUTION;
	waveheightResources_.ubo->L = 1000;
	waveheightResources_.ubo->RT = repeat_;
	waveheightResources_.ubo->T = 0.0f;

	waveheightResources_.h0TildeTx = graphics_->CreateStorageTexture(WATER_RESOLUTION, WATER_RESOLUTION, IMAGE_FORMAT);
	waveheightResources_.h0TildeTy = graphics_->CreateStorageTexture(WATER_RESOLUTION, WATER_RESOLUTION, IMAGE_FORMAT);
	waveheightResources_.h0TildeTz = graphics_->CreateStorageTexture(WATER_RESOLUTION, WATER_RESOLUTION, IMAGE_FORMAT);
	waveheightResources_.h0TildeSlopeX = graphics_->CreateStorageTexture(WATER_RESOLUTION, WATER_RESOLUTION, IMAGE_FORMAT);
	waveheightResources_.h0TildeSlopeZ = graphics_->CreateStorageTexture(WATER_RESOLUTION, WATER_RESOLUTION, IMAGE_FORMAT);

	eastl::vector<Quadbit::QbVkComputeDescriptor> computeDescriptors = {
		compute_->CreateComputeDescriptor(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, graphics_->GetDescriptorPtr(waveheightResources_.ubo)),
		compute_->CreateComputeDescriptor(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,  graphics_->GetDescriptorPtr(precalcResources_.h0Tilde)),
		compute_->CreateComputeDescriptor(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,  graphics_->GetDescriptorPtr(precalcResources_.h0TildeConj)),
		compute_->CreateComputeDescriptor(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,  graphics_->GetDescriptorPtr(waveheightResources_.h0TildeTx)),
		compute_->CreateComputeDescriptor(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,  graphics_->GetDescriptorPtr(waveheightResources_.h0TildeTy)),
		compute_->CreateComputeDescriptor(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,  graphics_->GetDescriptorPtr(waveheightResources_.h0TildeTz)),
		compute_->CreateComputeDescriptor(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,  graphics_->GetDescriptorPtr(waveheightResources_.h0TildeSlopeX)),
		compute_->CreateComputeDescriptor(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,  graphics_->GetDescriptorPtr(waveheightResources_.h0TildeSlopeZ))
	};

	waveheightInstance_ = compute_->CreateComputeInstance(computeDescriptors, "Resources/Shaders/Compiled/waveheight_comp.spv", "main");
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

	eastl::array<VkSpecializationMapEntry, 6> specMaps { xLocalSize, yLocalSize, zLocalSize, resolution, passCount, verticalPass };

	const int WATER_RESOLUTION_LOG2 = eastl::Internal::Log2(WATER_RESOLUTION);
	horizontalIFFTResources_.specData = { WATER_RESOLUTION, 1, 1, WATER_RESOLUTION, WATER_RESOLUTION_LOG2, 0 };
	verticalIFFTResources_.specData = { WATER_RESOLUTION, 1, 1, WATER_RESOLUTION, WATER_RESOLUTION_LOG2, 1 };

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

	horizontalIFFTResources_.dX = graphics_->CreateStorageTexture(WATER_RESOLUTION, WATER_RESOLUTION, IMAGE_FORMAT);
	horizontalIFFTResources_.dY = graphics_->CreateStorageTexture(WATER_RESOLUTION, WATER_RESOLUTION, IMAGE_FORMAT);
	horizontalIFFTResources_.dZ = graphics_->CreateStorageTexture(WATER_RESOLUTION, WATER_RESOLUTION, IMAGE_FORMAT);
	horizontalIFFTResources_.dSlopeX = graphics_->CreateStorageTexture(WATER_RESOLUTION, WATER_RESOLUTION, IMAGE_FORMAT);
	horizontalIFFTResources_.dSlopeZ = graphics_->CreateStorageTexture(WATER_RESOLUTION, WATER_RESOLUTION, IMAGE_FORMAT);

	verticalIFFTResources_.dX = graphics_->CreateStorageTexture(WATER_RESOLUTION, WATER_RESOLUTION, IMAGE_FORMAT);
	verticalIFFTResources_.dY = graphics_->CreateStorageTexture(WATER_RESOLUTION, WATER_RESOLUTION, IMAGE_FORMAT);
	verticalIFFTResources_.dZ = graphics_->CreateStorageTexture(WATER_RESOLUTION, WATER_RESOLUTION, IMAGE_FORMAT);
	verticalIFFTResources_.dSlopeX = graphics_->CreateStorageTexture(WATER_RESOLUTION, WATER_RESOLUTION, IMAGE_FORMAT);
	verticalIFFTResources_.dSlopeZ = graphics_->CreateStorageTexture(WATER_RESOLUTION, WATER_RESOLUTION, IMAGE_FORMAT);

	// Setup the vertical/horizontal IFFT compute shaders
	eastl::vector waveheightImageArray = {
		*graphics_->GetDescriptorPtr(waveheightResources_.h0TildeTx),
		*graphics_->GetDescriptorPtr(waveheightResources_.h0TildeTy),
		*graphics_->GetDescriptorPtr(waveheightResources_.h0TildeTz),
		*graphics_->GetDescriptorPtr(waveheightResources_.h0TildeSlopeX),
		*graphics_->GetDescriptorPtr(waveheightResources_.h0TildeSlopeZ)
	};
	eastl::vector horizontalImageArray = {
		*graphics_->GetDescriptorPtr(horizontalIFFTResources_.dX),
		*graphics_->GetDescriptorPtr(horizontalIFFTResources_.dY),
		*graphics_->GetDescriptorPtr(horizontalIFFTResources_.dZ),
		*graphics_->GetDescriptorPtr(horizontalIFFTResources_.dSlopeX),
		*graphics_->GetDescriptorPtr(horizontalIFFTResources_.dSlopeZ)
	};
	eastl::vector verticalImageArray = {
		*graphics_->GetDescriptorPtr(verticalIFFTResources_.dX),
		*graphics_->GetDescriptorPtr(verticalIFFTResources_.dY),
		*graphics_->GetDescriptorPtr(verticalIFFTResources_.dZ),
		*graphics_->GetDescriptorPtr(verticalIFFTResources_.dSlopeX),
		*graphics_->GetDescriptorPtr(verticalIFFTResources_.dSlopeZ)
	};

	eastl::vector<Quadbit::QbVkComputeDescriptor> horizontalComputeDesc = {
		compute_->CreateComputeDescriptor(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, waveheightImageArray),
		compute_->CreateComputeDescriptor(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, horizontalImageArray)
	};
	eastl::vector<Quadbit::QbVkComputeDescriptor> verticalComputeDesc = {
		compute_->CreateComputeDescriptor(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, horizontalImageArray),
		compute_->CreateComputeDescriptor(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, verticalImageArray)
	};
	
	horizontalIFFTInstance_ = compute_->CreateComputeInstance(horizontalComputeDesc, "Resources/Shaders/Compiled/ifft_comp.spv", "main", &horizontalSpecInfo, sizeof(IFFTPushConstants));
	verticalIFFTInstance_ = compute_->CreateComputeInstance(verticalComputeDesc, "Resources/Shaders/Compiled/ifft_comp.spv", "main", &verticalSpecInfo, sizeof(IFFTPushConstants));
}

void Water::InitDisplacementInstance() {
	displacementResources_.displacementMap = graphics_->CreateStorageTexture(WATER_RESOLUTION, WATER_RESOLUTION, IMAGE_FORMAT);
	auto samplerInfo = graphics_->CreateImageSamplerInfo(VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_TRUE, 16.0f, VK_COMPARE_OP_ALWAYS, VK_SAMPLER_MIPMAP_MODE_LINEAR);
	displacementResources_.normalMap = graphics_->CreateStorageTexture(WATER_RESOLUTION, WATER_RESOLUTION, IMAGE_FORMAT, &samplerInfo);

	// Setup the displacement compute shader
	eastl::vector<Quadbit::QbVkComputeDescriptor> computeDescriptors = {
		compute_->CreateComputeDescriptor(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, graphics_->GetDescriptorPtr(verticalIFFTResources_.dX)),
		compute_->CreateComputeDescriptor(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, graphics_->GetDescriptorPtr(verticalIFFTResources_.dY)),
		compute_->CreateComputeDescriptor(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, graphics_->GetDescriptorPtr(verticalIFFTResources_.dZ)),
		compute_->CreateComputeDescriptor(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, graphics_->GetDescriptorPtr(verticalIFFTResources_.dSlopeX)),
		compute_->CreateComputeDescriptor(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, graphics_->GetDescriptorPtr(verticalIFFTResources_.dSlopeZ)),
		compute_->CreateComputeDescriptor(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, graphics_->GetDescriptorPtr(displacementResources_.displacementMap)),
		compute_->CreateComputeDescriptor(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, graphics_->GetDescriptorPtr(displacementResources_.normalMap))
	};
	displacementInstance_ = compute_->CreateComputeInstance(computeDescriptors, "Resources/Shaders/Compiled/displacement_comp.spv", "main");
}

void Water::UpdateWaveheightUBO(float deltaTime) {
	static float t = 0.0f;

	waveheightResources_.ubo->RT = repeat_;
	waveheightResources_.ubo->T = t;

	t += deltaTime * step_;
}

void Water::UpdateTogglesUBO(float deltaTime) {
	Quadbit::Entity cameraEntity = graphics_->GetActiveCamera();
	Quadbit::RenderCamera* camera = entityManager_->GetComponentPtr<Quadbit::RenderCamera>(cameraEntity);
	togglesUBO_->colourIntensity = colourIntensity_;
	togglesUBO_->cameraPos = camera->position;
	togglesUBO_->useNormalMap = useNormalMap_ ? 1 : 0;
	togglesUBO_->topColour = topColour_;
	togglesUBO_->botColour = botColour_;
}

void Water::DrawImGui() {
	ImGui::SetNextWindowSize(ImVec2(300, 100), ImGuiCond_FirstUseEver);
	ImGui::Begin("Water Debug", nullptr);
	ImGui::Checkbox("Use Normal Map?", &useNormalMap_);
	ImGui::SliderFloat("Step size", &step_, 0.1f, 10.0f, "%.3f");
	ImGui::SliderFloat("Cycle length", &repeat_, 10.0f, 500.0f, "%.3f");
	ImGui::SliderFloat("Colour Intensity", &colourIntensity_, 0.01f, 1.0f, "%.3f");
	ImGui::ColorPicker4("Top colour", reinterpret_cast<float*>(&topColour_));
	ImGui::ColorPicker4("Bot colour", reinterpret_cast<float*>(&botColour_));
	ImGui::End();
}
