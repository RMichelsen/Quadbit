#include "PBRPipeline.h"

#include <string>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb/stb_image.h>
#include <stb/stb_image_write.h>

#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NOEXCEPTION
#define TINYGLTF_NO_INCLUDE_STB_IMAGE
#define TINYGLTF_NO_INCLUDE_STB_IMAGE_WRITE
#include <tinygltf/tiny_gltf.h>

#include "Engine/Core/Logging.h"
#include "Engine/Core/Time.h"
#include "Engine/Rendering/VulkanTypes.h"
#include "Engine/Rendering/RenderTypes.h"
#include "Engine/Entities/SystemDispatch.h"
#include "Engine/Entities/EntityManager.h"
#include "Engine/Rendering/VulkanUtils.h"
#include "Engine/Rendering/Systems/NoClipCameraSystem.h"
#include "Engine/Rendering/Memory/ResourceManager.h"


namespace Quadbit {
	PBRPipeline::PBRPipeline(QbVkContext& context) :
		context_(context) {

		// Register the mesh component to be used by the ECS
		context.entityManager->RegisterComponents<CustomMeshComponent, CustomMeshDeleteComponent, PBRSceneComponent, RenderTransformComponent,
			RenderCamera, CameraUpdateAspectRatioTag>();

		fallbackCamera_ = context.entityManager->Create();
		context.entityManager->AddComponent<RenderCamera>(fallbackCamera_, Quadbit::RenderCamera(145.0f, -42.0f, glm::vec3(130.0f, 190.0f, 25.0f), 16.0f / 9.0f, 10000.0f));

		QbVkPipelineDescription pipelineDescription;
		pipelineDescription.colorBlending = QbVkPipelineColorBlending::QBVK_COLORBLENDING_DISABLE;
		pipelineDescription.depth = QbVkPipelineDepth::QBVK_PIPELINE_DEPTH_ENABLE;
		pipelineDescription.dynamicState = QbVkPipelineDynamicState::QBVK_DYNAMICSTATE_VIEWPORTSCISSOR;
		pipelineDescription.enableMSAA = true;
		pipelineDescription.rasterization = QbVkPipelineRasterization::QBVK_PIPELINE_RASTERIZATION_DEFAULT;

		pipeline_ = context_.resourceManager->CreateGraphicsPipeline("Assets/Quadbit/Shaders/default_vert.glsl", "main",
			"Assets/Quadbit/Shaders/default_frag.glsl", "main", pipelineDescription, 1024);
	}

	void PBRPipeline::RebuildPipeline() {
		context_.entityManager->AddComponent<CameraUpdateAspectRatioTag>(fallbackCamera_);
		if (userCamera_ != NULL_ENTITY && context_.entityManager->IsValid(userCamera_)) context_.entityManager->AddComponent<CameraUpdateAspectRatioTag>(userCamera_);
	}

	void PBRPipeline::DrawFrame(uint32_t resourceIndex, VkCommandBuffer commandBuffer) {
		auto& pipeline = context_.resourceManager->pipelines_[pipeline_];

		// Here we clean up meshes that are due for removal
		context_.entityManager->ForEachWithCommandBuffer<CustomMeshDeleteComponent>([&](Entity entity, EntityCommandBuffer* cmdBuf, CustomMeshDeleteComponent& mesh) noexcept {
			if (mesh.deletionDelay == 0) {
				context_.resourceManager->DestroyResource(mesh.vertexHandle);
				context_.resourceManager->DestroyResource(mesh.indexHandle);
				cmdBuf->DestroyEntity(entity);
			}
			else {
				mesh.deletionDelay--;
			}
			});

		context_.entityManager->ForEach<RenderCamera, CameraUpdateAspectRatioTag>([&](Entity entity, auto& camera, auto& tag) noexcept {
			camera.perspective =
				glm::perspective(glm::radians(45.0f), static_cast<float>(context_.swapchain.extent.width) / static_cast<float>(context_.swapchain.extent.height), 0.1f, camera.viewDistance);
			camera.perspective[1][1] *= -1;
			});

		if (userCamera_ == NULL_ENTITY || !context_.entityManager->IsValid(userCamera_)) {
			context_.entityManager->systemDispatch_->RunSystem<NoClipCameraSystem>(Time::deltaTime, context_.inputHandler);
		}

		Quadbit::RenderCamera* camera;
		(userCamera_ != NULL_ENTITY && context_.entityManager->IsValid(userCamera_)) ? 
			camera = context_.entityManager->GetComponentPtr<RenderCamera>(userCamera_) : 
			camera = context_.entityManager->GetComponentPtr<RenderCamera>(fallbackCamera_);

		// Update environment map if applicable, use camera stuff to do this
		//if (environmentMap_ != NULL_ENTITY) {
		//	auto mesh = context_.entityManager->GetComponentPtr<Quadbit::RenderMeshComponent>(environmentMap_);
		//	EnvironmentMapPushConstants* pc = mesh->GetSafePushConstPtr<EnvironmentMapPushConstants>();
		//	pc->proj = camera->perspective;
		//	pc->proj[1][1] *= -1;

		//	// Invert pitch??
		//	camera->front.x = cos(glm::radians(camera->yaw)) * cos(glm::radians(-camera->pitch));
		//	camera->front.y = sin(glm::radians(-camera->pitch));
		//	camera->front.z = sin(glm::radians(camera->yaw)) * cos(glm::radians(-camera->pitch));
		//	camera->front = glm::normalize(camera->front);
		//	camera->view = glm::lookAt(camera->position, camera->position + camera->front, camera->up);
		//	pc->view = glm::mat4(glm::mat3(camera->view));

		//	// Reset
		//	camera->front.x = cos(glm::radians(camera->yaw)) * cos(glm::radians(camera->pitch));
		//	camera->front.y = sin(glm::radians(camera->pitch));
		//	camera->front.z = sin(glm::radians(camera->yaw)) * cos(glm::radians(camera->pitch));
		//	camera->front = glm::normalize(camera->front);
		//	camera->view = glm::lookAt(camera->position, camera->position + camera->front, camera->up);
		//}

		SetViewportAndScissor(commandBuffer);

		VkDeviceSize offsets[]{ 0 };

		pipeline->Bind(commandBuffer);
		context_.entityManager->ForEach<PBRSceneComponent, RenderTransformComponent>(
			[&](Entity entity, PBRSceneComponent& scene, RenderTransformComponent& transform) noexcept {
			for (const auto& mesh : scene.meshes) {
				eastl::array<VkDeviceSize, 1> offsets{ 0 };

				for (const auto& primitive : mesh.primitives) {
					offsets[0] = primitive.vertexOffset * sizeof(QbVkVertex);
					vkCmdBindVertexBuffers(commandBuffer, 0, 1, &context_.resourceManager->buffers_[scene.vertexHandle].buf, offsets.data());
					vkCmdBindIndexBuffer(commandBuffer, context_.resourceManager->buffers_[scene.indexHandle].buf, primitive.indexOffset * sizeof(uint32_t), VK_INDEX_TYPE_UINT32);

					RenderMeshPushConstants* pushConstants = scene.GetSafePushConstPtr<RenderMeshPushConstants>();
					auto model = transform.model * mesh.localTransform;
					pushConstants->model = model;
					pushConstants->mvp = camera->perspective * camera->view * model;
					vkCmdPushConstants(commandBuffer, pipeline->pipelineLayout_, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(RenderMeshPushConstants), pushConstants);

					pipeline->BindDescriptorSets(commandBuffer, primitive.material.descriptorSets);

					vkCmdDrawIndexed(commandBuffer, primitive.indexCount, 1, 0, 0, 0);
				}
			}
			});

		context_.entityManager->ForEach<CustomMeshComponent, RenderTransformComponent>(
			[&](Entity entity, CustomMeshComponent& mesh, RenderTransformComponent& transform) noexcept {
			QB_ASSERT(mesh.pipelineHandle != QBVK_PIPELINE_NULL_HANDLE && "Invalid pipeline handle!");
			const auto& meshPipeline = context_.resourceManager->pipelines_[mesh.pipelineHandle];
			meshPipeline->Bind(commandBuffer);

			if (mesh.pushConstantStride == -1) {
				RenderMeshPushConstants* pushConstants = mesh.GetSafePushConstPtr<RenderMeshPushConstants>();
				pushConstants->model = transform.model;
				pushConstants->mvp = camera->perspective * camera->view * transform.model;
				vkCmdPushConstants(commandBuffer, meshPipeline->pipelineLayout_, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(RenderMeshPushConstants), pushConstants);
			}
			else if (mesh.pushConstantStride > 0) {
				vkCmdPushConstants(commandBuffer, meshPipeline->pipelineLayout_, VK_SHADER_STAGE_VERTEX_BIT, 0, mesh.pushConstantStride, mesh.pushConstants.data());
			}

			vkCmdBindVertexBuffers(commandBuffer, 0, 1, &context_.resourceManager->buffers_[mesh.vertexHandle].buf, offsets);
			vkCmdBindIndexBuffer(commandBuffer, context_.resourceManager->buffers_[mesh.indexHandle].buf, 0, VK_INDEX_TYPE_UINT32);

			meshPipeline->BindDescriptorSets(commandBuffer, mesh.descriptorSetsHandle);

			vkCmdDrawIndexed(commandBuffer, mesh.indexCount, 1, 0, 0, 0);
			});
	}

	PBRSceneComponent PBRPipeline::LoadModel(const char* path)
	{
		eastl::string extension = VkUtils::GetFileExtension(path);
		PBRSceneComponent scene;

		std::string err, warn;
		tinygltf::TinyGLTF loader;
		tinygltf::Model model;

		bool loaded = false;
		if (extension.compare(".gltf") == 0) {
			loaded = loader.LoadASCIIFromFile(&model, &err, &warn, path);
		}
		else if (extension.compare(".glb") == 0) {
			loaded = loader.LoadBinaryFromFile(&model, &err, &warn, path);
		}
		if (!loaded) {
			QB_LOG_ERROR("Failed to load model at %s\n", path);
		}

		// For now we only support loading single scenes
		QB_ASSERT(model.scenes.size() == 1 && "Failed to parse model, only one scene allowed!");

		// Parse materials
		eastl::vector<QbVkPBRMaterial> materials;
		for (const auto& material : model.materials) {
			materials.push_back(ParseMaterial(model, material));
		}

		eastl::vector<QbVkVertex> vertices;
		eastl::vector<uint32_t> indices;
		// Parse nodes recursively
		for (const auto& node : model.scenes[model.defaultScene].nodes) {
			ParseNode(model, model.nodes[node], scene, vertices, indices, materials, glm::mat4(1.0f));
		}

		scene.vertexHandle = context_.resourceManager->CreateVertexBuffer(vertices.data(), sizeof(QbVkVertex), static_cast<uint32_t>(vertices.size()));
		scene.indexHandle = context_.resourceManager->CreateIndexBuffer(indices);

		return scene;
	}

	Entity PBRPipeline::GetActiveCamera() {
		return (userCamera_ == NULL_ENTITY) ? fallbackCamera_ : userCamera_;
	}

	void PBRPipeline::SetCamera(Entity entity) {
		if(entity != NULL_ENTITY) userCamera_ = entity;
	}

	void PBRPipeline::LoadSkyGradient(glm::vec3 botColour, glm::vec3 topColour) {
		//eastl::vector<Quadbit::QbVkVertexInputAttribute> vertexModel{
		//	Quadbit::QbVkVertexInputAttribute::QBVK_VERTEX_ATTRIBUTE_POSITION,
		//	Quadbit::QbVkVertexInputAttribute::QBVK_VERTEX_ATTRIBUTE_COLOUR
		//};

		//QbVkShaderInstance shaderInstance(context_);
		//shaderInstance.AddShader(gradientSkyboxVert.data(), static_cast<uint32_t>(gradientSkyboxVert.size()), "main", VK_SHADER_STAGE_VERTEX_BIT);
		//shaderInstance.AddShader(gradientSkyboxFrag.data(), static_cast<uint32_t>(gradientSkyboxFrag.size()), "main", VK_SHADER_STAGE_FRAGMENT_BIT);

		//eastl::vector<QbVkRenderDescriptor> empty{};
		//const QbVkRenderMeshInstance* instance = CreateInstance(empty, vertexModel, shaderInstance, sizeof(EnvironmentMapPushConstants), VK_SHADER_STAGE_VERTEX_BIT, VK_FALSE);

		//const eastl::vector<SkyGradientVertex> cubeVertices = {
		//	{{-1.0f, -1.0f, 1.0f},	topColour},
		//	{{1.0f, -1.0f, 1.0f},	topColour},
		//	{{1.0f, 1.0f, 1.0f},	botColour},
		//	{{-1.0f, 1.0f, 1.0f},	botColour},

		//	{{-1.0f, -1.0f, -1.0f}, topColour},
		//	{{1.0f, -1.0f, -1.0f},	topColour},
		//	{{1.0f, 1.0f, -1.0f},	botColour},
		//	{{-1.0f, 1.0f, -1.0f},	botColour}
		//};

		//const eastl::vector<uint32_t> cubeIndices = {
		//	0, 1, 2,
		//	2, 3, 0,
		//	1, 5, 6,
		//	6, 2, 1,
		//	7, 6, 5,
		//	5, 4, 7,
		//	4, 0, 3,
		//	3, 7, 4,
		//	4, 5, 1,
		//	1, 0, 4,
		//	3, 2, 6,
		//	6, 7, 3
		//};

		//environmentMap_ = context_.entityManager->Create();
		//context_.entityManager->AddComponent<RenderMeshComponent>(environmentMap_, {
		//	context_.resourceManager->CreateVertexBuffer(cubeVertices.data(), sizeof(SkyGradientVertex), static_cast<uint32_t>(cubeVertices.size())),
		//	context_.resourceManager->CreateIndexBuffer(cubeIndices),
		//	static_cast<uint32_t>(cubeIndices.size()),
		//	eastl::array<float, 32>(),
		//	sizeof(EnvironmentMapPushConstants),
		//	instance
		//	});
		//context_.entityManager->AddComponent<Quadbit::RenderTransformComponent>(environmentMap_,
		//	Quadbit::RenderTransformComponent(1.0f, { 0.0f, 0.0f, 0.0f }, { 0, 0, 0, 1 }));
	}

	void PBRPipeline::SetViewportAndScissor(VkCommandBuffer& commandBuffer) {
		// Dynamic update viewport and scissor for user-defined pipelines (also doesn't necessitate rebuilding the pipeline on window resize)
		VkViewport viewport{};
		VkRect2D scissor{};
		// In our case the viewport covers the entire window
		viewport.width = static_cast<float>(context_.swapchain.extent.width);
		viewport.height = static_cast<float>(context_.swapchain.extent.height);
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
		// And so does the scissor
		scissor.offset = { 0, 0 };
		scissor.extent = context_.swapchain.extent;
		vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
	}

	QbVkPBRMaterial PBRPipeline::ParseMaterial(const tinygltf::Model& model, const tinygltf::Material& material) {
		auto& pipeline = context_.resourceManager->pipelines_[pipeline_];

		QbVkPBRMaterial mat{};
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
		mat.descriptorSets = WriteMaterialDescriptors(mat);
		//mat.descriptorAllocator = pipeline->descriptorAllocator_;
		return mat;
	}

	void PBRPipeline::ParseNode(const tinygltf::Model& model, const tinygltf::Node& node, PBRSceneComponent& scene,
		eastl::vector<QbVkVertex>& vertices, eastl::vector<uint32_t>& indices, const eastl::vector<QbVkPBRMaterial> materials, glm::mat4 parentTransform) {
		// Construct transform matrix from node data and parent data
		// Even though the data read is in double precision, we will 
		// take the precision hit and store them as floats since it should
		// be plenty precision for simple transforms
		glm::f64mat4 translation = glm::f64mat4(1.0f);
		if (node.translation.size() == 3) {
			translation = glm::translate(translation, glm::make_vec3(node.translation.data()));
		} 
		glm::f64mat4 rotation = glm::f64mat4(1.0f);
		if (node.rotation.size() == 4) {
			rotation = glm::f64mat4(glm::make_quat(node.rotation.data()));
		}
		glm::f64mat4 scale = glm::f64mat4(1.0f);
		if (node.scale.size() == 3) {
			scale = glm::scale(scale, glm::make_vec3(node.scale.data()));
		}
		glm::mat4 transform = glm::mat4(1.0f);
		if (node.matrix.size() == 16) {
			transform = translation * rotation * scale * glm::make_mat4x4(node.matrix.data());
		}
		else {
			transform = translation * rotation * scale;
		}

		// Let parent transform data propagate down through children on recurse
		if (node.children.size() > 0) {
			for (const auto& node : node.children) {
				ParseNode(model, model.nodes[node], scene, vertices, indices, materials, transform);
			}
		}

		// Parse mesh if node contains a mesh
		if (node.mesh > -1) {
			QbVkPBRMesh mesh = ParseMesh(model, model.meshes[node.mesh], vertices, indices, materials);
			mesh.localTransform = transform;
			scene.meshes.push_back(mesh);
		}
	}

	QbVkPBRMesh PBRPipeline::ParseMesh(const tinygltf::Model& model, const tinygltf::Mesh& mesh,
		eastl::vector<QbVkVertex>& vertices, eastl::vector<uint32_t>& indices, const eastl::vector<QbVkPBRMaterial>& materials) {
		QbVkPBRMesh qbMesh{};

		for (const auto& primitive : mesh.primitives) {
			QB_ASSERT(primitive.attributes.find("POSITION") != primitive.attributes.end());
			QB_ASSERT(primitive.mode == TINYGLTF_MODE_TRIANGLES);

			const auto vertexCount = model.accessors[primitive.attributes.find("POSITION")->second].count;

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
				const auto& accessor = model.accessors[index];
				const auto& bufferView = model.bufferViews[accessor.bufferView];
				const auto& buffer = model.buffers[bufferView.buffer];

				if (name.compare("POSITION") == 0) {
					positions = reinterpret_cast<const float*>(&buffer.data[accessor.byteOffset + bufferView.byteOffset]);
					positionStride = accessor.ByteStride(bufferView) / sizeof(float);
					QB_ASSERT(positions != nullptr && positionStride > 0);
				}
				else if (name.compare("NORMAL") == 0) {
					normals = reinterpret_cast<const float*>(&buffer.data[accessor.byteOffset + bufferView.byteOffset]);
					normalStride = accessor.ByteStride(bufferView) / sizeof(float);
					QB_ASSERT(normals != nullptr && normalStride > 0);
				}
				else if (name.compare("TEXCOORD_0") == 0) {
					uvs0 = reinterpret_cast<const float*>(&buffer.data[accessor.byteOffset + bufferView.byteOffset]);
					uv0Stride = accessor.ByteStride(bufferView) / sizeof(float);
					QB_ASSERT(uvs0 != nullptr && uv0Stride > 0);
				}
				else if (name.compare("TEXCOORD_1") == 0) {
					uvs1 = reinterpret_cast<const float*>(&buffer.data[accessor.byteOffset + bufferView.byteOffset]);
					uv1Stride = accessor.ByteStride(bufferView) / sizeof(float);
					QB_ASSERT(uvs1 != nullptr && uv1Stride > 0);
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
			QB_ASSERT(primitive.indices > -1);

			const auto& accessor = model.accessors[primitive.indices];
			const auto& bufferView = model.bufferViews[accessor.bufferView];
			const auto& buffer = model.buffers[bufferView.buffer];
			const auto indexCount = accessor.count;

			QB_ASSERT((accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE ||
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

			QbVkPBRPrimitive qbPrimitive;
			qbPrimitive.vertexOffset = vertexOffset;
			qbPrimitive.indexOffset = indexOffset;
			qbPrimitive.indexCount = static_cast<uint32_t>(indices.size()) - indexOffset;
			qbPrimitive.material = materials[primitive.material];
			qbMesh.primitives.push_back(qbPrimitive);
		}

		return qbMesh;
	}

	QbVkDescriptorSetsHandle PBRPipeline::WriteMaterialDescriptors(const QbVkPBRMaterial& material) {
		auto& pipeline = context_.resourceManager->pipelines_[pipeline_];

		auto descriptorSetsHandle = pipeline->GetNextDescriptorSetsHandle();
		auto emptyTextureHandle = context_.resourceManager->GetEmptyTexture();

		std::vector<VkWriteDescriptorSet> writeDescSets;

		auto& baseColorHandle = (material.textureIndices.baseColorTextureIndex > -1) ?
			material.baseColorTexture : emptyTextureHandle;

		auto& metallicRoughnessHandle = (material.textureIndices.metallicRoughnessTextureIndex > -1) ?
			material.metallicRoughnessTexture : emptyTextureHandle;

		auto& normalHandle = (material.textureIndices.normalTextureIndex > -1) ?
			material.normalTexture : emptyTextureHandle;

		auto& occlusionHandle = (material.textureIndices.occlusionTextureIndex > -1) ?
			material.occlusionTexture : emptyTextureHandle;

		auto& emissiveHandle = (material.textureIndices.emissiveTextureIndex > -1) ?
			material.emissiveTexture : emptyTextureHandle;

		pipeline->BindResource(descriptorSetsHandle, "baseColorMap", baseColorHandle);
		pipeline->BindResource(descriptorSetsHandle, "metallicRoughnessMap", metallicRoughnessHandle);
		pipeline->BindResource(descriptorSetsHandle, "normalMap", normalHandle);
		pipeline->BindResource(descriptorSetsHandle, "occlusionMap", occlusionHandle);
		pipeline->BindResource(descriptorSetsHandle, "emissiveMap", emissiveHandle);

		return descriptorSetsHandle;
	}
	QbVkTextureHandle PBRPipeline::CreateTextureFromResource(const tinygltf::Model& model, const tinygltf::Material& material, const char* textureName) {
		auto& texture = model.textures[material.values.at(textureName).TextureIndex()];
		auto& image = model.images[texture.source];

		auto samplerInfo = GetSamplerInfo(model, texture.sampler);
		return context_.resourceManager->LoadTexture(image.width, image.height, image.image.data(), &samplerInfo);
	}

	VkSamplerCreateInfo PBRPipeline::GetSamplerInfo(const tinygltf::Model& model, int samplerIndex) {
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
}