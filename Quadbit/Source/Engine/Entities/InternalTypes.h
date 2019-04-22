#pragma once

#include <stdint.h>
#include <glm/vec3.hpp>

namespace Quadbit {
	using VertexBufHandle = uint16_t;
	using IndexBufHandle = uint16_t;

	struct alignas(16) RenderMeshPushConstants {
		glm::mat4 model;
	};

	struct RenderMesh {
		VertexBufHandle vertexHandle;
		IndexBufHandle indexHandle;
		uint32_t indexCount;
		float scale;
		glm::vec3 position;
		glm::quat rotation;
		RenderMeshPushConstants dynamicData;
	};

	// If components inherit from EventTag once any system processes 
	// the entity with the tag, it is removed.
	struct EventTag {};
	struct CameraUpdateAspectRatioTag : public EventTag {};

	struct RenderCamera {
		RenderCamera(float camPitch, float camYaw, glm::vec3 camPos, float aspectRatio, float viewDistance) :
			pitch(camPitch), yaw(camYaw), pos(camPos), roll(0.0f), dragActive(false), up(glm::vec3(0.0f, 1.0f, 0.0f)) {
			front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
			front.y = sin(glm::radians(pitch));
			front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
			front = glm::normalize(front);
			view = glm::lookAt(pos, pos + front, up);
			perspective = glm::perspective(glm::radians(45.0f), aspectRatio, 0.1f, viewDistance);
			perspective[1][1] *= -1;
		}

		glm::vec3 pos;
		glm::vec3 front;
		glm::vec3 up;
		float pitch;
		float yaw;
		float roll;
		glm::mat4 view;
		glm::mat4 perspective;
		bool dragActive;
	};
}
