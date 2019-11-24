#include "Water.h"

#include <random>
#include <EASTL/sort.h>
#include <imgui/imgui.h>

#include "Engine/Entities/EntityTypes.h"

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

	compute_->Dispatch(precalcPipeline_, WATER_RESOLUTION / 32, WATER_RESOLUTION / 32, 1);
}

void Water::Simulate(float deltaTime) {
	// Update toggles
	UpdateTogglesUBO(deltaTime);

	// Increment the time in the wave functions 
	UpdateWaveheightUBO(deltaTime);

	compute_->Dispatch(waveheightPipeline_, WATER_RESOLUTION / 32, WATER_RESOLUTION / 32, 1);

	// Here we pass the addresses of 5 instances of IFFTPushConstants
	// to the compute pipeline to be used as pushconstants for X = 5 
	// iterations of both the horizontal and vertical pipeline
	eastl::array<IFFTPushConstants, 5> iterations = { {
		{ 0 }, { 1 }, { 2 }, { 3 }, { 4 }
	} };
	eastl::vector<const void*> pushConstants{
		&iterations[0],
		&iterations[1],
		&iterations[2],
		&iterations[3],
		&iterations[4]
	};

	// IFFT, first a horizontal pass then a vertical pass
	compute_->DispatchX(5, horizontalIFFTPipeline_, 1, WATER_RESOLUTION, 1, pushConstants, sizeof(IFFTPushConstants));
	compute_->DispatchX(5, verticalIFFTPipeline_, 1, WATER_RESOLUTION, 1, pushConstants, sizeof(IFFTPushConstants));

	// Finally assemble the displacement map to be used in the vertex shader each frame
	compute_->Dispatch(displacementPipeline_, WATER_RESOLUTION / 32, WATER_RESOLUTION / 32, 1);

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


	precalcPipeline_ = compute_->CreatePipeline("Resources/Shaders/Compiled/precalc_comp.spv", "main");
	compute_->BindResource(precalcPipeline_, "UBO", precalcResources_.ubo.handle);
	compute_->BindResource(precalcPipeline_, "h0tilde", precalcResources_.h0Tilde);
	compute_->BindResource(precalcPipeline_, "h0tilde_conj", precalcResources_.h0TildeConj);
	compute_->BindResource(precalcPipeline_, "UNIF_RAND_STORAGE_BUF", precalcResources_.uniformRandomsStorageBuffer);
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

	waveheightPipeline_ = compute_->CreatePipeline("Resources/Shaders/Compiled/waveheight_comp.spv", "main");
	compute_->BindResource(waveheightPipeline_, "UBO", waveheightResources_.ubo.handle);
	compute_->BindResource(waveheightPipeline_, "h0tilde", precalcResources_.h0Tilde);
	compute_->BindResource(waveheightPipeline_, "h0tilde_conj", precalcResources_.h0TildeConj);
	compute_->BindResource(waveheightPipeline_, "h0tilde_tx", waveheightResources_.h0TildeTx);
	compute_->BindResource(waveheightPipeline_, "h0tilde_ty", waveheightResources_.h0TildeTy);
	compute_->BindResource(waveheightPipeline_, "h0tilde_tz", waveheightResources_.h0TildeTz);
	compute_->BindResource(waveheightPipeline_, "h0tilde_slopex", waveheightResources_.h0TildeSlopeX);
	compute_->BindResource(waveheightPipeline_, "h0tilde_slopez", waveheightResources_.h0TildeSlopeZ);
}

void Water::InitInverseFFTComputeInstances() {
	const int WATER_RESOLUTION_LOG2 = eastl::Internal::Log2(WATER_RESOLUTION);
	horizontalIFFTResources_.specData = { WATER_RESOLUTION, 1, 1, WATER_RESOLUTION, WATER_RESOLUTION_LOG2, 0 };
	verticalIFFTResources_.specData = { WATER_RESOLUTION, 1, 1, WATER_RESOLUTION, WATER_RESOLUTION_LOG2, 1 };

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
		waveheightResources_.h0TildeTx,
		waveheightResources_.h0TildeTy,
		waveheightResources_.h0TildeTz,
		waveheightResources_.h0TildeSlopeX,
		waveheightResources_.h0TildeSlopeZ
	};
	eastl::vector horizontalImageArray = {
		horizontalIFFTResources_.dX,
		horizontalIFFTResources_.dY,
		horizontalIFFTResources_.dZ,
		horizontalIFFTResources_.dSlopeX,
		horizontalIFFTResources_.dSlopeZ
	};
	eastl::vector verticalImageArray = {
		verticalIFFTResources_.dX,
		verticalIFFTResources_.dY,
		verticalIFFTResources_.dZ,
		verticalIFFTResources_.dSlopeX,
		verticalIFFTResources_.dSlopeZ
	};

	horizontalIFFTPipeline_ = compute_->CreatePipeline("Resources/Shaders/Compiled/ifft_comp.spv", "main", horizontalIFFTResources_.specData.data());
	verticalIFFTPipeline_ = compute_->CreatePipeline("Resources/Shaders/Compiled/ifft_comp.spv", "main", verticalIFFTResources_.specData.data());

	compute_->BindResourceArray(horizontalIFFTPipeline_, "input_images", waveheightImageArray);
	compute_->BindResourceArray(horizontalIFFTPipeline_, "output_images", horizontalImageArray);

	compute_->BindResourceArray(verticalIFFTPipeline_, "input_images", horizontalImageArray);
	compute_->BindResourceArray(verticalIFFTPipeline_, "output_images", verticalImageArray);
}

void Water::InitDisplacementInstance() {
	displacementResources_.displacementMap = graphics_->CreateStorageTexture(WATER_RESOLUTION, WATER_RESOLUTION, IMAGE_FORMAT);
	auto samplerInfo = graphics_->CreateImageSamplerInfo(VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_TRUE, 16.0f, VK_COMPARE_OP_ALWAYS, VK_SAMPLER_MIPMAP_MODE_LINEAR);
	displacementResources_.normalMap = graphics_->CreateStorageTexture(WATER_RESOLUTION, WATER_RESOLUTION, IMAGE_FORMAT, &samplerInfo);

	displacementPipeline_ = compute_->CreatePipeline("Resources/Shaders/Compiled/displacement_comp.spv", "main");
	compute_->BindResource(displacementPipeline_, "Dx", verticalIFFTResources_.dX);
	compute_->BindResource(displacementPipeline_, "Dy", verticalIFFTResources_.dY);
	compute_->BindResource(displacementPipeline_, "Dz", verticalIFFTResources_.dZ);
	compute_->BindResource(displacementPipeline_, "D_slopex", verticalIFFTResources_.dSlopeX);
	compute_->BindResource(displacementPipeline_, "D_slopez", verticalIFFTResources_.dSlopeZ);
	compute_->BindResource(displacementPipeline_, "displacement_map", displacementResources_.displacementMap);
	compute_->BindResource(displacementPipeline_, "normal_map", displacementResources_.normalMap);
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
