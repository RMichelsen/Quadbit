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
#include "Engine/Rendering/Geometry/Icosphere.h"
#include "Engine/Rendering/Geometry/Plane.h"
#include "Engine/Rendering/Memory/ResourceManager.h"
#include "Engine/Rendering/Systems/NoClipCameraSystem.h"


namespace Quadbit {
	PBRPipeline::PBRPipeline(QbVkContext& context) :
		context_(context) {

		// Register the mesh component to be used by the ECS
		context.entityManager->RegisterComponents<CustomMeshComponent, ResourceDeleteComponent, PBRSceneComponent, RenderTransformComponent,
			RenderCamera, CameraUpdateAspectRatioTag>();

		QbVkPipelineDescription pipelineDescription;
		pipelineDescription.colourBlending = QbVkPipelineColourBlending::QBVK_COLOURBLENDING_DISABLE;
		pipelineDescription.depth = QbVkPipelineDepth::QBVK_PIPELINE_DEPTH_ENABLE;
		pipelineDescription.dynamicState = QbVkPipelineDynamicState::QBVK_DYNAMICSTATE_VIEWPORTSCISSOR;
		pipelineDescription.enableMSAA = true;
		pipelineDescription.rasterization = QbVkPipelineRasterization::QBVK_PIPELINE_RASTERIZATION_DEFAULT;

		pipeline_ = context_.resourceManager->CreateGraphicsPipeline("Assets/Quadbit/Shaders/default_vert.glsl", "main",
			"Assets/Quadbit/Shaders/default_frag.glsl", "main", pipelineDescription, context_.mainRenderPass, 1024);

		sunUBO_ = context.resourceManager->CreateUniformBuffer<SunUBO>();
	}

	void PBRPipeline::DrawShadows(uint32_t resourceIndex, VkCommandBuffer commandBuffer) {
		//auto& pipeline = context_.resourceManager->pipelines_[context_.shadowmapResources.pipeline];

		//static float timer = 0.0f;

		//// Animate the light source
		//auto* ubo = context_.resourceManager->GetUniformBufferPtr<PBRUBO>(ubo_);
		//ubo->lightPos.x = sin(glm::radians(timer * 360.0f)) * 40.0f;
		//ubo->lightPos.y = 500.0f;
		//ubo->lightPos.z = cos(glm::radians(timer * 360.0f)) * 40.0f;

		//// Matrix from light's point of view
		//static float zNear = 1.0f;
		//static float zFar = 96.0f;
		//static float lightFOV = 45.0f;

		//ImGui::Begin("Camera Position", nullptr);
		//ImGui::DragFloat("zNear", &zNear);
		//ImGui::DragFloat("zFar", &zFar);
		//ImGui::DragFloat("FOV", &lightFOV);
		//ImGui::End();

		//glm::mat4 depthProjectionMatrix = glm::perspective(glm::radians(lightFOV), 1.0f, zNear, zFar);
		//glm::mat4 depthViewMatrix = glm::lookAt(ubo->lightPos, glm::vec3(0.0f), glm::vec3(0, 1, 0));
		//glm::mat4 depthModelMatrix = glm::mat4(1.0f);
		//glm::mat4 lightSpace = depthProjectionMatrix * depthViewMatrix * depthModelMatrix;
		//ubo->lightSpace = lightSpace;


		//ImGui::Begin("LightSpace");
		//ImGui::Text("%f %f %f %f\n", lightSpace[0][0], lightSpace[0][1], lightSpace[0][2], lightSpace[0][3]);
		//ImGui::Text("%f %f %f %f\n", lightSpace[1][0], lightSpace[1][1], lightSpace[1][2], lightSpace[1][3]);
		//ImGui::Text("%f %f %f %f\n", lightSpace[2][0], lightSpace[2][1], lightSpace[2][2], lightSpace[2][3]);
		//ImGui::Text("%f %f %f %f\n", lightSpace[3][0], lightSpace[3][1], lightSpace[3][2], lightSpace[3][3]);
		//ImGui::End();

		//timer += 0.0001f;

		//context_.entityManager->ForEach<PBRSceneComponent, RenderTransformComponent>(
		//	[&](Entity entity, PBRSceneComponent& scene, RenderTransformComponent& transform) noexcept {
		//		for (const auto& mesh : scene.meshes) {
		//			eastl::array<VkDeviceSize, 1> offsets{ 0 };

		//			for (const auto& primitive : mesh.primitives) {
		//				offsets[0] = primitive.vertexOffset * sizeof(QbVkVertex);
		//				vkCmdBindVertexBuffers(commandBuffer, 0, 1, &context_.resourceManager->buffers_[scene.vertexHandle].buf, offsets.data());
		//				vkCmdBindIndexBuffer(commandBuffer, context_.resourceManager->buffers_[scene.indexHandle].buf, primitive.indexOffset * sizeof(uint32_t), VK_INDEX_TYPE_UINT32);

		//				//RenderMeshPushConstants* pushConstants = scene.GetSafePushConstPtr<RenderMeshPushConstants>();
		//				//auto model = transform.model * mesh.localTransform;
		//				//pushConstants->model = model;	
		//				//pushConstants->mvp = camera->perspective * camera->view * model;
		//				vkCmdPushConstants(commandBuffer, pipeline->pipelineLayout_, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &lightSpace);

		//				//pipeline->BindDescriptorSets(commandBuffer, primitive.material.descriptorSets);

		//				vkCmdDrawIndexed(commandBuffer, primitive.indexCount, 1, 0, 0, 0);
		//			}
		//		}
		//	});
	}

	void PBRPipeline::DrawFrame(uint32_t resourceIndex, VkCommandBuffer commandBuffer) {
		auto& pipeline = context_.resourceManager->pipelines_[pipeline_];

		context_.entityManager->ForEach<RenderCamera, CameraUpdateAspectRatioTag>([&](Entity entity, auto& camera, auto& tag) noexcept {
			camera.perspective =
				glm::perspective(glm::radians(45.0f), static_cast<float>(context_.swapchain.extent.width) / static_cast<float>(context_.swapchain.extent.height), 0.1f, camera.viewDistance);
			camera.perspective[1][1] *= -1;
			});

		if (context_.userCamera == NULL_ENTITY || !context_.entityManager->IsValid(context_.userCamera)) {
			context_.entityManager->systemDispatch_->RunSystem<NoClipCameraSystem>(Time::deltaTime, context_.inputHandler);
		}

		Quadbit::RenderCamera* camera;
		(context_.userCamera != NULL_ENTITY && context_.entityManager->IsValid(context_.userCamera)) ?
			camera = context_.entityManager->GetComponentPtr<RenderCamera>(context_.userCamera) :
			camera = context_.entityManager->GetComponentPtr<RenderCamera>(context_.fallbackCamera);

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


		ImGui::Begin("Camera Position", nullptr);
		ImGui::Text("%f, %f, %f", camera->position.x, camera->position.y, camera->position.z);
		ImGui::End();

		VkDeviceSize offsets[]{ 0 };

		pipeline->Bind(commandBuffer);
		SetViewportAndScissor(commandBuffer);

		auto* sunUBO = context_.resourceManager->GetUniformBufferPtr<SunUBO>(sunUBO_);
		sunUBO->sunAltitude = context_.sunAltitude;
		sunUBO->sunAzimuth = context_.sunAzimuth;

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

					pipeline->BindDescriptorSets(commandBuffer, scene.materials[primitive.material].descriptorSets);

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
				pushConstants->mvp = (camera->perspective * camera->view * transform.model);
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

	QbVkPBRMaterial PBRPipeline::CreateMaterial(QbVkTextureHandle baseColourTexture, QbVkTextureHandle metallicRoughnessTexture, 
		QbVkTextureHandle normalTexture, QbVkTextureHandle occlusionTexture, QbVkTextureHandle emissiveTexture) {
		QbVkPBRMaterial material;
		material.ubo = context_.resourceManager->CreateUniformBuffer<MaterialUBO>();

		material.SetBaseColourTexture(baseColourTexture);
		material.SetMetallicRoughnessTexture(metallicRoughnessTexture);
		material.SetNormalTexture(normalTexture);
		material.SetOcclusionTexture(occlusionTexture);
		material.SetEmissiveTexture(emissiveTexture);

		MaterialUBO ubo{};
		ubo.alphaMask = material.alphaMask;
		ubo.alphaMaskCutoff = material.alphaCutoff;
		ubo.baseColourFactor = material.baseColourFactor;
		ubo.emissiveFactor = material.emissiveFactor;
		ubo.metallicFactor = material.metallicFactor;
		ubo.roughnessFactor = material.roughnessFactor;
		ubo.baseColourTextureIndex = material.textureIndices.baseColourTextureIndex;
		ubo.emissiveTextureIndex = material.textureIndices.emissiveTextureIndex;
		ubo.metallicRoughnessTextureIndex = material.textureIndices.metallicRoughnessTextureIndex;
		ubo.normalTextureIndex = material.textureIndices.normalTextureIndex;
		ubo.occlusionTextureIndex = material.textureIndices.occlusionTextureIndex;
		context_.resourceManager->InitializeUBO(material.ubo, &ubo);

		material.descriptorSets = WriteMaterialDescriptors(material);
		return material;
	}

	PBRSceneComponent PBRPipeline::LoadScene(const char* path)
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
		for (const auto& material : model.materials) {
			scene.materials.push_back(ParseMaterial(model, material));
		}

		eastl::vector<QbVkVertex> vertices;
		eastl::vector<uint32_t> indices;
		// Parse nodes recursively
		for (const auto& node : model.scenes[model.defaultScene].nodes) {
			ParseNode(model, model.nodes[node], scene, vertices, indices, glm::mat4(1.0f));
		}

		scene.vertexHandle = context_.resourceManager->CreateVertexBuffer(vertices.data(), sizeof(QbVkVertex), static_cast<uint32_t>(vertices.size()));
		scene.indexHandle = context_.resourceManager->CreateIndexBuffer(indices);

		return scene;
	}

	PBRSceneComponent PBRPipeline::CreatePlane(uint32_t xSize, uint32_t zSize, const QbVkPBRMaterial& material) {
		PBRSceneComponent scene{};
		Plane plane(xSize, zSize);
		scene.vertexHandle = context_.resourceManager->CreateVertexBuffer(plane.vertices.data(), sizeof(QbVkVertex), static_cast<uint32_t>(plane.vertices.size()));
		scene.indexHandle = context_.resourceManager->CreateIndexBuffer(plane.indices);

		scene.materials.push_back(material);
		QbVkPBRPrimitive primitive;
		primitive.material = 0;
		primitive.indexCount = static_cast<uint32_t>(plane.indices.size());
		primitive.indexOffset = 0;
		primitive.vertexOffset = 0;
		QbVkPBRMesh mesh;
		mesh.localTransform = glm::mat4(1.0f);
		mesh.primitives.push_back(primitive);

		scene.meshes.push_back(mesh);

		return scene;
	}

	void PBRPipeline::DestroyScene(const Entity& entity) {
		const auto& entityManager = context_.entityManager;
		QB_ASSERT(entityManager->HasComponent<PBRSceneComponent>(entity));
		const auto& model = entityManager->GetComponentPtr<PBRSceneComponent>(entity);
		eastl::vector<QbVkBufferHandle> bufferHandles;
		eastl::vector<QbVkTextureHandle> textureHandles;
		for (const auto& material : model->materials) {
			if (material.textureIndices.baseColourTextureIndex > -1) textureHandles.push_back(material.baseColourTexture);
			if (material.textureIndices.emissiveTextureIndex > -1) textureHandles.push_back(material.emissiveTexture);
			if (material.textureIndices.metallicRoughnessTextureIndex > -1) textureHandles.push_back(material.metallicRoughnessTexture);
			if (material.textureIndices.normalTextureIndex > -1) textureHandles.push_back(material.normalTexture);
			if (material.textureIndices.occlusionTextureIndex > -1) textureHandles.push_back(material.occlusionTexture);
			bufferHandles.push_back(material.ubo.handle);
		}
		bufferHandles.push_back(model->vertexHandle);
		bufferHandles.push_back(model->indexHandle);
		entityManager->AddComponent<ResourceDeleteComponent>(
			entityManager->Create(),
			{ bufferHandles, textureHandles }
		);
		entityManager->RemoveComponent<PBRSceneComponent>(entity);
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
			auto& texture = model.textures[material.values.at("baseColorTexture").TextureIndex()];
			mat.baseColourTexture = CreateTextureFromResource(model, texture);
			mat.textureIndices.baseColourTextureIndex = material.values.at("baseColorTexture").TextureTexCoord();
		}
		if (material.values.find("metallicRoughnessTexture") != material.values.end()) {
			auto& texture = model.textures[material.values.at("metallicRoughnessTexture").TextureIndex()];
			mat.metallicRoughnessTexture = CreateTextureFromResource(model, texture);
			mat.textureIndices.metallicRoughnessTextureIndex = material.values.at("metallicRoughnessTexture").TextureTexCoord();
		}
		if (material.additionalValues.find("normalTexture") != material.additionalValues.end()) {
			auto& texture = model.textures[material.additionalValues.at("normalTexture").TextureIndex()];
			mat.normalTexture = CreateTextureFromResource(model, texture);
			mat.textureIndices.normalTextureIndex = material.additionalValues.at("normalTexture").TextureTexCoord();
		}
		if (material.additionalValues.find("occlusionTexture") != material.additionalValues.end()) {
			auto& texture = model.textures[material.additionalValues.at("occlusionTexture").TextureIndex()];
			mat.occlusionTexture = CreateTextureFromResource(model, texture);
			mat.textureIndices.occlusionTextureIndex = material.additionalValues.at("occlusionTexture").TextureTexCoord();
		}
		if (material.additionalValues.find("emissiveTexture") != material.additionalValues.end()) {
			auto& texture = model.textures[material.additionalValues.at("emissiveTexture").TextureIndex()];
			mat.emissiveTexture = CreateTextureFromResource(model, texture);
			mat.textureIndices.emissiveTextureIndex = material.additionalValues.at("emissiveTexture").TextureTexCoord();
		}
		if (material.values.find("baseColorFactor") != material.values.end()) {
			mat.baseColourFactor = glm::make_vec4(material.values.at("baseColorFactor").ColorFactor().data());
		}
		if (material.additionalValues.find("emissiveFactor") != material.additionalValues.end()) {
			mat.emissiveFactor = glm::vec4(glm::make_vec3(material.additionalValues.at("emissiveFactor").ColorFactor().data()), 1.0f);
		}
		if (material.values.find("metallicFactor") != material.values.end()) {
			mat.metallicFactor = static_cast<float>(material.values.at("metallicFactor").Factor());
		}
		if (material.values.find("roughnessFactor") != material.values.end()) {
			mat.roughnessFactor = static_cast<float>(material.values.at("roughnessFactor").Factor());
		}
		if (material.additionalValues.find("alphaMode") != material.additionalValues.end()) {
			if (material.additionalValues.at("alphaMode").string_value == "MASK") {
				mat.alphaMask = 1.0f;
				mat.alphaCutoff = 0.5f;
				if (material.additionalValues.find("alphaCutoff") != material.additionalValues.end()) {
					mat.alphaCutoff = static_cast<float>(material.additionalValues.at("alphaCutoff").Factor());
				}
			}
		}

		mat.ubo = context_.resourceManager->CreateUniformBuffer<MaterialUBO>();
		MaterialUBO ubo{};
		ubo.alphaMask = mat.alphaMask;
		ubo.alphaMaskCutoff = mat.alphaCutoff;
		ubo.baseColourFactor = mat.baseColourFactor;
		ubo.emissiveFactor = mat.emissiveFactor;
		ubo.metallicFactor = mat.metallicFactor;
		ubo.roughnessFactor = mat.roughnessFactor;
		ubo.baseColourTextureIndex = mat.textureIndices.baseColourTextureIndex;
		ubo.emissiveTextureIndex = mat.textureIndices.emissiveTextureIndex;
		ubo.metallicRoughnessTextureIndex = mat.textureIndices.metallicRoughnessTextureIndex;
		ubo.normalTextureIndex = mat.textureIndices.normalTextureIndex;
		ubo.occlusionTextureIndex = mat.textureIndices.occlusionTextureIndex;
		context_.resourceManager->InitializeUBO(mat.ubo, &ubo);

		mat.descriptorSets = WriteMaterialDescriptors(mat);
		return mat;
	}

	void PBRPipeline::ParseNode(const tinygltf::Model& model, const tinygltf::Node& node, PBRSceneComponent& scene,
		eastl::vector<QbVkVertex>& vertices, eastl::vector<uint32_t>& indices, glm::mat4 parentTransform) {
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
				ParseNode(model, model.nodes[node], scene, vertices, indices, transform);
			}
		}

		// Parse mesh if node contains a mesh
		if (node.mesh > -1) {
			QbVkPBRMesh mesh = ParseMesh(model, model.meshes[node.mesh], vertices, indices, scene.materials);
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
			qbPrimitive.material = primitive.material;
			qbMesh.primitives.push_back(qbPrimitive);
		}

		return qbMesh;
	}

	QbVkDescriptorSetsHandle PBRPipeline::WriteMaterialDescriptors(QbVkPBRMaterial& material) {
		auto& pipeline = context_.resourceManager->pipelines_[pipeline_];

		auto descriptorSetsHandle = pipeline->GetNextDescriptorSetsHandle();
		auto emptyTextureHandle = context_.resourceManager->GetEmptyTexture();

		std::vector<VkWriteDescriptorSet> writeDescSets;

		auto& baseColourHandle = (material.textureIndices.baseColourTextureIndex > -1) ?
			material.baseColourTexture : emptyTextureHandle;

		auto& metallicRoughnessHandle = (material.textureIndices.metallicRoughnessTextureIndex > -1) ?
			material.metallicRoughnessTexture : emptyTextureHandle;

		auto& normalHandle = (material.textureIndices.normalTextureIndex > -1) ?
			material.normalTexture : emptyTextureHandle;

		auto& occlusionHandle = (material.textureIndices.occlusionTextureIndex > -1) ?
			material.occlusionTexture : emptyTextureHandle;

		auto& emissiveHandle = (material.textureIndices.emissiveTextureIndex > -1) ?
			material.emissiveTexture : emptyTextureHandle;

		// Bind UBO for vertex shader
		pipeline->BindResource(descriptorSetsHandle, "SunUBO", sunUBO_.handle);

		// Bind texture for fragment shader
		pipeline->BindResource(descriptorSetsHandle, "shadowMap", context_.shadowmapResources.texture);
		pipeline->BindResource(descriptorSetsHandle, "baseColourMap", baseColourHandle);
		pipeline->BindResource(descriptorSetsHandle, "metallicRoughnessMap", metallicRoughnessHandle);
		pipeline->BindResource(descriptorSetsHandle, "normalMap", normalHandle);
		pipeline->BindResource(descriptorSetsHandle, "occlusionMap", occlusionHandle);
		pipeline->BindResource(descriptorSetsHandle, "emissiveMap", emissiveHandle);

		pipeline->BindResource(descriptorSetsHandle, "MaterialUBO", material.ubo.handle);

		return descriptorSetsHandle;
	}
	QbVkTextureHandle PBRPipeline::CreateTextureFromResource(const tinygltf::Model& model, const tinygltf::Texture& texture) {
		auto& image = model.images[texture.source];

		auto samplerInfo = GetSamplerInfo(model, texture.sampler);
		return context_.resourceManager->LoadTexture(image.width, image.height, image.image.data(), &samplerInfo);
	}

	VkSamplerCreateInfo PBRPipeline::GetSamplerInfo(const tinygltf::Model& model, int samplerIndex) {
		VkSamplerCreateInfo samplerCreateInfo = DEFAULT_TEXTURE_SAMPLER;
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