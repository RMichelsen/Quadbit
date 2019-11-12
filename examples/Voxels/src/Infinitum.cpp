#include "Infinitum.h"

//#include <imgui/imgui.h>

#include "Engine/Core/InputHandler.h"
#include "Engine/Global/ImGuiState.h"
#include "Engine/Entities/EntityManager.h"

#include "Data/Components.h"
#include "Systems/VoxelGenerationSystem.h"
#include "Systems/MeshGenerationSystem.h"
#include "Systems/ThirdPersonCameraSystem.h"
#include "Systems/PlayerMovementSystem.h"

#include "Utils/VoxelMesher.h"

void Infinitum::Init() {
	// Setup entities
	auto& entityManager = EntityManager::Instance();
	entityManager.RegisterComponents<VoxelBlockComponent, VoxelBlockUpdateTag, MeshGenerationUpdateTag, MeshReadyTag, PlayerTag>();

	renderer_->LoadSkyGradient(glm::vec3(1.0f, 1.0f, 1.0f), glm::vec3(0.1f, 0.4f, 0.8f));

	//camera_ = entityManager->Create();
	//camera_.AddComponent<Quadbit::RenderCamera>(Quadbit::RenderCamera(0.0f, 0.0f, glm::vec3(), renderer_->GetAspectRatio(), 10000.0f));
	//renderer_->RegisterCamera(camera_);

	Quadbit::QbVkShaderInstance shaderInstance = renderer_->CreateShaderInstance();
	shaderInstance.AddShader("Resources/Shaders/Compiled/voxel_vert.spv", "main", VK_SHADER_STAGE_VERTEX_BIT);
	shaderInstance.AddShader("Resources/Shaders/Compiled/voxel_frag.spv", "main", VK_SHADER_STAGE_FRAGMENT_BIT);

	renderMeshInstance_ = renderer_->CreateRenderMeshInstance({
		Quadbit::QbVkVertexInputAttribute::QBVK_VERTEX_ATTRIBUTE_POSITION,
		Quadbit::QbVkVertexInputAttribute::QBVK_VERTEX_ATTRIBUTE_COLOUR
		}, shaderInstance
	);

	player_ = entityManager.Create();
	player_.AddComponent<Quadbit::RenderTransformComponent>(Quadbit::RenderTransformComponent(1.0f, glm::vec3(16.0f, 30.0f, 16.0f), glm::quat()));
	player_.AddComponent<Quadbit::RenderMeshComponent>(renderer_->CreateMesh(cubeVertices, sizeof(VoxelVertex), cubeIndices, renderMeshInstance_));
	player_.AddComponent<PlayerTag>();

	uint32_t globalSeed = 4646;

	// Initialize the noise generator 
	fastnoiseTerrain_ = FastNoiseSIMD::NewFastNoiseSIMD(globalSeed);
	fastnoiseTerrain_->SetNoiseType(FastNoiseSIMD::SimplexFractal);
	fastnoiseTerrain_->SetFrequency(0.025f);
	fastnoiseTerrain_->SetFractalType(FastNoiseSIMD::FractalType::FBM);
	fastnoiseTerrain_->SetFractalOctaves(2);
	fastnoiseTerrain_->SetFractalLacunarity(2.33f);
	fastnoiseTerrain_->SetFractalGain(0.366f);

	// Initialize the colour generator
	fastnoiseColours_ = FastNoiseSIMD::NewFastNoiseSIMD(globalSeed);
	fastnoiseColours_->SetNoiseType(FastNoiseSIMD::NoiseType::Simplex);
	fastnoiseColours_->SetFrequency(0.002f);

	// "Simulate voronoi noise"
	fastnoiseRegions_ = FastNoiseSIMD::NewFastNoiseSIMD(globalSeed);
	fastnoiseRegions_->SetNoiseType(FastNoiseSIMD::NoiseType::Cellular);
	fastnoiseRegions_->SetFrequency(0.00025f);
	fastnoiseRegions_->SetCellularReturnType(FastNoiseSIMD::CellularReturnType::CellValue);
	fastnoiseRegions_->SetCellularDistanceFunction(FastNoiseSIMD::CellularDistanceFunction::Euclidean);
	fastnoiseRegions_->SetCellularJitter(0.3f);

	static const char* noiseTypes[] = { "Value", "ValueFractal", "Perlin", "PerlinFractal", "Simplex", "SimplexFractal", "WhiteNoise", "Cellular", "Cubic", "CubicFractal" };
	static const char* fractalTypes[] = { "FBM", "Billow", "RigidMulti" };
	static const char* perturbTypes[] = { "None", "Gradient", "Gradient Fractal", "Normalize", "Gradient + Normalize", "Gradient Fractal + Normalize" };

	//Quadbit::ImGuiState::Inject([]() {
	//	ImGui::SetNextWindowSize(ImVec2(300, 100), ImGuiCond_FirstUseEver);
	//	ImGui::Begin("Terrain Generation", nullptr);
	//	ImGui::DragInt("Seed", &terrainSettings_.seed, 0.1f, 0, 9999999);
	//	ImGui::Combo("Noise Type", &terrainSettings_.noiseType, noiseTypes, IM_ARRAYSIZE(noiseTypes));
	//	ImGui::DragFloat("Noise Frequency", &terrainSettings_.noiseFrequency, 0.001f, 0.0f, 5.0f);
	//	ImGui::Combo("Fractal Noise Type", &terrainSettings_.fractalType, fractalTypes, IM_ARRAYSIZE(fractalTypes));
	//	ImGui::DragInt("Fractal Octaves", &terrainSettings_.fractalOctaves, 0.01f, 0, 100);
	//	ImGui::DragFloat("Fractal Lacunarity", &terrainSettings_.fractalLacunarity, 0.001f, 0.0f, 5.0f);
	//	ImGui::DragFloat("Fractal Gain", &terrainSettings_.fractalGain, 0.001f, 0.0f, 5.0f);
	//	ImGui::Combo("Perturb Type", &terrainSettings_.perturbType, perturbTypes, IM_ARRAYSIZE(perturbTypes));
	//	ImGui::Text("Press G to generate new terrain");
	//	ImGui::End();
	//});

	//Quadbit::ImGuiState::Inject([]() {
	//	ImGui::SetNextWindowSize(ImVec2(300, 100), ImGuiCond_FirstUseEver);
	//	ImGui::Begin("Colour Generation", nullptr);
	//	ImGui::DragInt("Seed", &colourSettings_.seed, 0.1f, 0, 9999999);
	//	ImGui::Combo("Noise Type", &colourSettings_.noiseType, noiseTypes, IM_ARRAYSIZE(noiseTypes));
	//	ImGui::DragFloat("Noise Frequency", &colourSettings_.noiseFrequency, 0.001f, 0.0f, 5.0f);
	//	ImGui::Combo("Fractal Noise Type", &colourSettings_.fractalType, fractalTypes, IM_ARRAYSIZE(fractalTypes));
	//	ImGui::DragInt("Fractal Octaves", &colourSettings_.fractalOctaves, 0.01f, 0, 100);
	//	ImGui::DragFloat("Fractal Lacunarity", &colourSettings_.fractalLacunarity, 0.001f, 0.0f, 5.0f);
	//	ImGui::DragFloat("Fractal Gain", &colourSettings_.fractalGain, 0.001f, 0.0f, 5.0f);
	//	ImGui::Combo("Perturb Type", &colourSettings_.perturbType, perturbTypes, IM_ARRAYSIZE(perturbTypes));
	//	ImGui::Text("Press G to generate new terrain");
	//	ImGui::End();
	//	});

	// Spawn base
	for(auto i = 0; i < VOXEL_BLOCK_WIDTH * 10; i += VOXEL_BLOCK_WIDTH) {
		for(auto j = 0; j < VOXEL_BLOCK_WIDTH * 10; j += VOXEL_BLOCK_WIDTH) {
			auto entity = entityManager.Create();
			entity.AddComponent<Quadbit::RenderTransformComponent>(Quadbit::RenderTransformComponent(1.0f, glm::vec3(i, 0.0f, j), glm::quat()));
			entity.AddComponents<VoxelBlockComponent, VoxelBlockUpdateTag>();
			chunks_.push_back(entity);
		}
	}
}

void Infinitum::Simulate(float deltaTime) {
	auto& entityManager = EntityManager::Instance();

	entityManager.systemDispatch_->RunSystem<VoxelGenerationSystem>(deltaTime, fastnoiseTerrain_, fastnoiseRegions_, fastnoiseColours_);
	entityManager.systemDispatch_->RunSystem<MeshGenerationSystem>(deltaTime, renderer_.get(), renderMeshInstance_);

	if(Quadbit::InputHandler::KeyPressed(0x47)) {
		for(auto&& entity : chunks_) {
			renderer_->DestroyMesh(*entity.GetComponentPtr<RenderMeshComponent>());
			entity.RemoveComponent<RenderMeshComponent>();

			fastnoiseTerrain_->SetSeed(terrainSettings_.seed);
			fastnoiseTerrain_->SetNoiseType(static_cast<FastNoiseSIMD::NoiseType>(terrainSettings_.noiseType));
			fastnoiseTerrain_->SetFrequency(terrainSettings_.noiseFrequency);
			fastnoiseTerrain_->SetFractalType(static_cast<FastNoiseSIMD::FractalType>(terrainSettings_.fractalType));
			fastnoiseTerrain_->SetFractalOctaves(terrainSettings_.fractalOctaves);
			fastnoiseTerrain_->SetFractalLacunarity(terrainSettings_.fractalLacunarity);
			fastnoiseTerrain_->SetFractalGain(terrainSettings_.fractalGain);
			fastnoiseTerrain_->SetPerturbType(static_cast<FastNoiseSIMD::PerturbType>(terrainSettings_.perturbType));


			fastnoiseColours_->SetSeed(colourSettings_.seed);
			fastnoiseColours_->SetNoiseType(static_cast<FastNoiseSIMD::NoiseType>(colourSettings_.noiseType));
			fastnoiseColours_->SetFrequency(colourSettings_.noiseFrequency);
			fastnoiseColours_->SetFractalType(static_cast<FastNoiseSIMD::FractalType>(colourSettings_.fractalType));
			fastnoiseColours_->SetFractalOctaves(colourSettings_.fractalOctaves);
			fastnoiseColours_->SetFractalLacunarity(colourSettings_.fractalLacunarity);
			fastnoiseColours_->SetFractalGain(colourSettings_.fractalGain);
			fastnoiseColours_->SetPerturbType(static_cast<FastNoiseSIMD::PerturbType>(colourSettings_.perturbType));

			entity.AddComponents<VoxelBlockUpdateTag>();
		}
	}
}

void Infinitum::DrawFrame() {
	renderer_->DrawFrame();
}