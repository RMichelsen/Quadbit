#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <array>
#include <deque>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

#include "Engine/Core/QbVulkanDefs.h"
#include "Engine/Core/Logging.h"

inline constexpr int MAX_MESH_COUNT = 65536;
inline constexpr int MAX_TEXTURES = 50;

namespace Quadbit {
	struct MeshBuffers {
		VertexBufHandle vertexBufferIdx_;
		IndexBufHandle indexBufferIdx_;
		std::array<QbVkAsyncBuffer, MAX_MESH_COUNT> vertexBuffers_;
		std::array<QbVkAsyncBuffer, MAX_MESH_COUNT> indexBuffers_;
		std::deque<VertexBufHandle> vertexBufferFreeList_;
		std::deque<IndexBufHandle> indexBufferFreeList_;

		VertexBufHandle GetNextVertexHandle() {
			VertexBufHandle next = vertexBufferIdx_;
			if(vertexBufferFreeList_.empty()) {
				vertexBufferIdx_++;
				assert(next < MAX_MESH_COUNT && "Cannot get next vertex buffer handle for mesh, max number of handles in use!");
				return next;
			}
			else {
				next = vertexBufferFreeList_.front();
				vertexBufferFreeList_.pop_front();
				assert(next < MAX_MESH_COUNT && "Cannot get next index buffer handle for mesh, max number of handles in use!");
				return next;
			}
		}

		IndexBufHandle GetNextIndexHandle() {
			IndexBufHandle next = indexBufferIdx_;
			if(indexBufferFreeList_.empty()) {
				indexBufferIdx_++;
				assert(next < MAX_MESH_COUNT);
				return next;
			}
			else {
				next = indexBufferFreeList_.front();
				indexBufferFreeList_.pop_front();
				assert(next < MAX_MESH_COUNT);
				return next;
			}
		}
	};

	struct MeshVertex {
		glm::vec3 position;
		glm::vec3 normal;
		glm::vec2 uv;

		static VkVertexInputBindingDescription GetBindingDescription() {
			VkVertexInputBindingDescription bindingDescription{};
			bindingDescription.binding = 0;
			bindingDescription.stride = sizeof(MeshVertex);
			bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
			return bindingDescription;
		}

		static std::array<VkVertexInputAttributeDescription, 3> GetAttributeDescriptions() {
			std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions{};
			// Attribute description for the position
			attributeDescriptions[0].binding = 0;
			attributeDescriptions[0].location = 0;
			attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
			attributeDescriptions[0].offset = offsetof(MeshVertex, position);
			// Attribute description for the normal
			attributeDescriptions[1].binding = 0;
			attributeDescriptions[1].location = 1;
			attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
			attributeDescriptions[1].offset = offsetof(MeshVertex, normal);
			// Attribute description for the uv
			attributeDescriptions[2].binding = 0;
			attributeDescriptions[2].location = 2;
			attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
			attributeDescriptions[2].offset = offsetof(MeshVertex, uv);
			return attributeDescriptions;
		}
	};

	using VertexBufHandle = uint16_t;
	using IndexBufHandle = uint16_t;

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

		RenderTransformComponent(float scale, glm::vec3 pos, glm::quat rot) : scale(scale), position(pos), rotation(rot) {
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

	struct RenderTexturedObjectComponent {
		VertexBufHandle vertexHandle;
		IndexBufHandle indexHandle;
		uint32_t indexCount;
		uint32_t descriptorIndex;
		std::array<float, 32> pushConstants;
		int pushConstantStride;

		template<typename T>
		T* GetSafePushConstPtr() {
			if(sizeof(T) > 128) {
				QB_LOG_ERROR("Push Constants must have a max size of 128 bytes");
				return nullptr;
			}
			return reinterpret_cast<T*>(pushConstants.data());
		}
	};

	struct RenderMeshComponent {
		VertexBufHandle vertexHandle;
		IndexBufHandle indexHandle;
		uint32_t indexCount;
		std::array<float, 32> pushConstants;
		int pushConstantStride;
		std::shared_ptr<QbVkRenderMeshInstance> instance;

		template<typename T>
		T* GetSafePushConstPtr() {
			if(sizeof(T) > 128) {
				QB_LOG_ERROR("Push Constants must have a max size of 128 bytes");
				return nullptr;
			}
			return reinterpret_cast<T*>(pushConstants.data());
		}
	};

	struct RenderMeshDeleteComponent {
		VertexBufHandle vertexHandle;
		IndexBufHandle indexHandle;
		uint32_t count;
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
			yaw(camYaw), pitch(camPitch), position(camPos), aspectRatio(aspectRatio), viewDistance(viewDist),
			dragActive(false), up(glm::vec3(0.0f, 1.0f, 0.0f)) {

			perspective = glm::perspective(glm::radians(45.0f), aspectRatio, 0.1f, viewDistance);
			perspective[1][1] *= -1;
		}
	};
}