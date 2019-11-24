#pragma once

#include <vulkan/vulkan.h>

#include <EASTL/array.h>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

#include "Engine/Core/Logging.h"
#include "Engine/Rendering/VulkanTypes.h"

namespace Quadbit {
	struct SkyGradientVertex {
		glm::vec3 position;
		glm::vec3 colour;
	};

	struct alignas(16) EnvironmentMapPushConstants {
		glm::mat4 proj;
		glm::mat4 view;
	};

	struct alignas(16) RenderMeshPushConstants {
		glm::mat4 model;
		glm::mat4 mvp;
	};

	struct RenderTransformComponent {
		glm::mat4 model;
		glm::vec3 position;
		glm::quat rotation;
		float scale;

		RenderTransformComponent(float scale, glm::vec3 pos, glm::quat rot) : position(pos), rotation(rot), scale(scale) {
			UpdateModel();
		}

		void UpdateModel() {
			auto T = glm::translate(glm::mat4(1.0f), position);
			auto R = glm::toMat4(rotation);
			auto S = glm::scale(glm::mat4(1.0f), glm::vec3(scale, scale, scale));
			model = T * R * S;
		}

		void UpdatePosition(const glm::vec3 deltaPos) {
			position += deltaPos;
			UpdateModel();
		}

		void UpdateRotation(const float angle, const glm::vec3 axis) {
			rotation = glm::rotate(rotation, angle, axis);
			UpdateModel();
		}

		void UpdateScale(const float deltaScale) {
			scale += deltaScale;
			UpdateModel();
		}

		void SetPosition(const glm::vec3 pos) {
			position = pos;
			UpdateModel();
		}

		void SetRotation(glm::quat rot) {
			rotation = rot;
			UpdateModel();
		}

		void SetScale(float scale) {
			this->scale = scale;
			UpdateModel();
		}
	};

	struct QbVkPBRMaterial {
		struct TextureIndices {
			int baseColorTextureIndex = -1;
			int metallicRoughnessTextureIndex = -1;
			int normalTextureIndex = -1;
			int occlusionTextureIndex = -1;
			int emissiveTextureIndex = -1;
		};

		QbVkTextureHandle baseColorTexture = QBVK_TEXTURE_NULL_HANDLE;
		QbVkTextureHandle metallicRoughnessTexture = QBVK_TEXTURE_NULL_HANDLE;
		QbVkTextureHandle normalTexture = QBVK_TEXTURE_NULL_HANDLE;
		QbVkTextureHandle occlusionTexture = QBVK_TEXTURE_NULL_HANDLE;
		QbVkTextureHandle emissiveTexture = QBVK_TEXTURE_NULL_HANDLE;

		TextureIndices textureIndices;

		glm::vec4 baseColorFactor{};
		glm::vec4 emissiveFactor{};
		float metallicFactor = 0.0f;
		float roughnessFactor = 0.0f;

		QbVkDescriptorAllocatorHandle descriptorAllocator = QBVK_DESCRIPTOR_ALLOCATOR_NULL_HANDLE;
		QbVkDescriptorSetsHandle descriptorSets = QBVK_DESCRIPTOR_SETS_NULL_HANDLE;
	};

	struct QbVkPBRPrimitive {
		QbVkPBRMaterial material;

		uint32_t vertexOffset;
		uint32_t indexOffset;
		uint32_t indexCount;
	};

	struct QbVkPBRMesh {
		glm::mat4 localTransform;
		eastl::vector<QbVkPBRPrimitive> primitives;
	};

	struct PBRSceneComponent {
		QbVkResourceHandle<QbVkBuffer> vertexHandle;
		QbVkResourceHandle<QbVkBuffer> indexHandle;

		eastl::vector<QbVkPBRMesh> meshes;

		eastl::array<float, 32> pushConstants;
		int pushConstantStride;

		template<typename T>
		T* GetSafePushConstPtr() {
			assert(sizeof(T) <= 128 && "Push Constants must have a max size of 128 bytes");
			return reinterpret_cast<T*>(pushConstants.data());
		}
	};

	class QbVkPipeline;
	struct CustomMeshComponent {
		QbVkResourceHandle<QbVkBuffer> vertexHandle;
		QbVkResourceHandle<QbVkBuffer> indexHandle;
		uint32_t indexCount;
		eastl::array<float, 32> pushConstants;
		int pushConstantStride;
		QbVkPipelineHandle pipelineHandle = QBVK_PIPELINE_NULL_HANDLE;
		QbVkDescriptorSetsHandle descriptorSetsHandle = QBVK_DESCRIPTOR_SETS_NULL_HANDLE;

		template<typename T>
		T* GetSafePushConstPtr() {
			assert(sizeof(T) <= 128 && "Push Constants must have a max size of 128 bytes");
			return reinterpret_cast<T*>(pushConstants.data());
		}
	};


	// The deletion delay should be the number of potential frames 
	// in a row that the mesh could be used by the rendering system
	struct CustomMeshDeleteComponent {
		QbVkBufferHandle vertexHandle;
		QbVkBufferHandle indexHandle;
		uint32_t deletionDelay = MAX_FRAMES_IN_FLIGHT;
	};

	// Just a tag, the tag is automatically removed when an entity with the tag is iterated over
	struct EventTagComponent {};
	struct CameraUpdateAspectRatioTag : public EventTagComponent {};

	struct RenderCamera {
		glm::vec3 position;
		glm::vec3 front;
		glm::vec3 up;
		float aspectRatio;
		float viewDistance;

		float yaw;
		float pitch;

		glm::mat4 view;
		glm::mat4 perspective;

		bool dragActive;

		RenderCamera(float camYaw, float camPitch, glm::vec3 camPos, float aspectRatio, float viewDist) :
			position(camPos), front(glm::vec3(0.0f, 0.0f, 0.0f)), up(glm::vec3(0.0f, 1.0f, 0.0f)),
			aspectRatio(aspectRatio), viewDistance(viewDist), yaw(camYaw), pitch(camPitch),
			view(glm::mat4(1)), dragActive(false) {

			perspective = glm::perspective(glm::radians(45.0f), aspectRatio, 0.1f, viewDistance);
			perspective[1][1] *= -1;
		}
	};
}