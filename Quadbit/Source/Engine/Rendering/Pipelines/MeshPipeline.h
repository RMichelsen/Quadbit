#pragma once
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include "../Common/QbVkCommon.h"

namespace Quadbit {
	struct alignas(16) UniformBufferObject {
		glm::mat4 model;
		glm::mat4 view;
		glm::mat4 proj;
	};

	struct alignas(16) PushConstants {
		glm::mat4 mvp;
	};

	struct Vertex {
		glm::vec3 pos;
		glm::vec3 colour;

		static VkVertexInputBindingDescription getBindingDescription() {
			VkVertexInputBindingDescription bindingDescription = {};
			bindingDescription.binding = 0;
			bindingDescription.stride = sizeof(Vertex);
			bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
			return bindingDescription;
		}

		static std::array<VkVertexInputAttributeDescription, 2> getAttributeDescriptions() {
			std::array<VkVertexInputAttributeDescription, 2> attributeDescriptions = {};
			// Attribute description for the position
			attributeDescriptions[0].binding = 0;
			attributeDescriptions[0].location = 0;
			attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
			attributeDescriptions[0].offset = offsetof(Vertex, pos);
			// Attribute description for the colour
			attributeDescriptions[1].binding = 0;
			attributeDescriptions[1].location = 1;
			attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
			attributeDescriptions[1].offset = offsetof(Vertex, colour);
			return attributeDescriptions;
		}
	};

	const std::vector<Vertex> vertices = {
		{{-1.0f, -1.0f, 1.0f}, {1.0f, 0.0f, 0.0f}},
		{{1.0f, -1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}},
		{{1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}},
		{{-1.0f, 1.0f, 1.0f}, {1.0f, 1.0f, 1.0f}},

		{{-1.0f, -1.0f, -1.0f}, {1.0f, 0.0f, 0.0f}},
		{{1.0f, -1.0f, -1.0f}, {0.0f, 1.0f, 0.0f}},
		{{1.0f, 1.0f, -1.0f}, {0.0f, 0.0f, 1.0f}},
		{{-1.0f, 1.0f, -1.0f}, {1.0f, 1.0f, 1.0f}}
	};

	const std::vector<uint32_t> indices = {
		0, 1, 2,
		2, 3, 0,
		1, 5, 6,
		6, 2, 1,
		7, 6, 5,
		5, 4, 7,
		4, 0, 3,
		3, 7, 4,
		4, 5, 1,
		1, 0, 4,
		3, 2, 6,
		6, 7, 3
	};

	class MeshPipeline {
	public:
		MeshPipeline(std::shared_ptr<QbVkContext> context);
		~MeshPipeline();
		void RebuildPipeline();

		void DrawFrame(VkCommandBuffer commandbuffer);
		void UpdateUniformBuffers(uint32_t currentImage);

	private:
		PushConstants pushConstants_{};

		std::shared_ptr<QbVkContext> context_ = nullptr;

		VkDescriptorPool descriptorPool_ = VK_NULL_HANDLE;
		VkDescriptorSetLayout descriptorSetLayout_ = VK_NULL_HANDLE;
		std::array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT> descriptorSets_{};

		VkPipelineLayout pipelineLayout_ = VK_NULL_HANDLE;
		VkPipeline pipeline_ = VK_NULL_HANDLE;

		QbVkBuffer vertexBuffer_;
		QbVkBuffer indexBuffer_;

		std::array<VkBuffer, MAX_FRAMES_IN_FLIGHT> uniformBuffers_;
		std::array<VkDeviceMemory, MAX_FRAMES_IN_FLIGHT> uniformBuffersMemory_;

		void CreateDescriptorPool();
		void CreateDescriptorSetLayout();
		void CreateDescriptorSets();

		void CreatePipeline();
		void CreateVertexBuffer();
		void CreateIndexBuffer();
		void CreateUniformBuffers();
	};
}