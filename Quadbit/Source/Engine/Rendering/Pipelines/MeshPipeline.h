#pragma once

#include "../Common/QbVkCommon.h"
#include "../../Entities/EntityManager.h"
#include "../../Entities/InternalTypes.h"

namespace Quadbit {
	struct alignas(16) UniformBufferObject {
		glm::mat4 view;
		glm::mat4 proj;
	};

	struct Vertex {
		glm::vec3 pos;
		glm::vec3 colour;

		static VkVertexInputBindingDescription GetBindingDescription() {
			VkVertexInputBindingDescription bindingDescription{};
			bindingDescription.binding = 0;
			bindingDescription.stride = sizeof(Vertex);
			bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
			return bindingDescription;
		}

		static std::array<VkVertexInputAttributeDescription, 2> GetAttributeDescriptions() {
			std::array<VkVertexInputAttributeDescription, 2> attributeDescriptions{};
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

	const std::vector<Vertex> cubeVertices = {
	{{-1.0f, -1.0f, 1.0f}, {1.0f, 0.0f, 0.0f}},
	{{1.0f, -1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}},
	{{1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}},
	{{-1.0f, 1.0f, 1.0f}, {1.0f, 1.0f, 1.0f}},

	{{-1.0f, -1.0f, -1.0f}, {1.0f, 0.0f, 0.0f}},
	{{1.0f, -1.0f, -1.0f}, {0.0f, 1.0f, 0.0f}},
	{{1.0f, 1.0f, -1.0f}, {0.0f, 0.0f, 1.0f}},
	{{-1.0f, 1.0f, -1.0f}, {1.0f, 1.0f, 1.0f}}
	};

	const std::vector<uint32_t> cubeIndices = {
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

	struct MeshBuffers {
		VertexBufHandle vertexBufferIdx_;
		IndexBufHandle indexBufferIdx_;
		std::array<QbVkBuffer, MAX_MESH_COUNT> vertexBuffers_;
		std::array<QbVkBuffer, MAX_MESH_COUNT> indexBuffers_;
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

	class MeshPipeline {
	public:
		MeshPipeline(std::shared_ptr<QbVkContext> context, std::shared_ptr<EntityManager> entityManager);
		~MeshPipeline();
		void RebuildPipeline();

		void DrawFrame(uint32_t resourceIndex, VkCommandBuffer commandbuffer);
		void UpdateUniformBuffers(uint32_t currentImage);

	private:
		friend class QbVkRenderer;

		std::shared_ptr<QbVkContext> context_ = nullptr;
		std::shared_ptr<EntityManager> entityManager_ = nullptr;

		VkDescriptorPool descriptorPool_ = VK_NULL_HANDLE;
		VkDescriptorSetLayout descriptorSetLayout_ = VK_NULL_HANDLE;
		std::array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT> descriptorSets_{};

		VkPipelineLayout pipelineLayout_ = VK_NULL_HANDLE;
		VkPipeline pipeline_ = VK_NULL_HANDLE;

		std::array<QbVkBuffer, MAX_FRAMES_IN_FLIGHT> uniformBuffers_;

		RenderMeshPushConstants pushConstants_{};
		MeshBuffers meshBufs_{};

		Entity mainCamera_;

		void CreateDescriptorPool();
		void CreateDescriptorSetLayout();
		void CreateDescriptorSets();
		void CreatePipeline();

		void DestroyVertexBuffer(VertexBufHandle handle);
		void DestroyIndexBuffer(IndexBufHandle handle);
		VertexBufHandle CreateVertexBuffer(const std::vector<Vertex>& vertices);
		IndexBufHandle CreateIndexBuffer(const std::vector<uint32_t>& indices);

		void CreateUniformBuffers();
	};
}