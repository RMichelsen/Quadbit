#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <array>
#include <deque>
#include <unordered_set>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

#include "Engine/Rendering/VulkanTypes.h"
#include "Engine/Core/Logging.h"

namespace Quadbit {
	struct SkyGradientVertex {
		glm::vec3 position;
		glm::vec3 colour;
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

	struct RenderTexturedObjectComponent {
		QbVkResourceHandle<QbVkBuffer> vertexHandle;
		QbVkResourceHandle<QbVkBuffer> indexHandle;
		uint32_t indexCount;
		uint32_t descriptorIndex;
		std::array<float, 32> pushConstants;
		int pushConstantStride;

		template<typename T>
		T* GetSafePushConstPtr() {
			assert(sizeof(T) <= 128 && "Push Constants must have a max size of 128 bytes");
			return reinterpret_cast<T*>(pushConstants.data());
		}
	};

	struct RenderMeshComponent {
		QbVkResourceHandle<QbVkBuffer> vertexHandle;
		QbVkResourceHandle<QbVkBuffer> indexHandle;
		uint32_t indexCount;
		std::array<float, 32> pushConstants;
		int pushConstantStride;
		const QbVkRenderMeshInstance* instance;

		template<typename T>
		T* GetSafePushConstPtr() {
			assert(sizeof(T) <= 128 && "Push Constants must have a max size of 128 bytes");
			return reinterpret_cast<T*>(pushConstants.data());
		}
	};


	// The deletion delay should be the number of potential frames 
	// in a row that the mesh could be used by the rendering system
	struct RenderMeshDeleteComponent {
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