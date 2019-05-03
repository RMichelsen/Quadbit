#pragma once

#include "../RenderTypes.h"
#include "../Common/QbVkDefines.h"
#include "../../Entities/EntityManager.h"

namespace Quadbit {
	constexpr int MAX_MESH_COUNT = 65536;

	struct alignas(16) MainUBO {
		glm::mat4 depthMVP;
		glm::vec3 lightPosition;
	};

	struct alignas(16) OffscreenUBO {
		glm::mat4 depthMVP;
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
		MeshPipeline(std::shared_ptr<QbVkContext> context);
		~MeshPipeline();
		void RebuildPipeline();

		void DrawShadows(uint32_t resourceIndex, VkCommandBuffer commandbuffer);
		void DrawFrame(uint32_t resourceIndex, VkCommandBuffer commandbuffer);

	private:
		friend class QbVkRenderer;

		std::shared_ptr<QbVkContext> context_ = nullptr;
		EntityManager* entityManager_ = nullptr;

		VkDescriptorPool descriptorPool_ = VK_NULL_HANDLE;
		VkDescriptorSetLayout descriptorSetLayout_ = VK_NULL_HANDLE;
		std::array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT> descriptorSets_{};
		std::array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT> offscreenDescriptorSets_{};

		VkPipelineLayout pipelineLayout_ = VK_NULL_HANDLE;
		VkPipeline pipeline_ = VK_NULL_HANDLE;

		VkPipelineLayout offscreenPipelineLayout_ = VK_NULL_HANDLE;
		VkPipeline offscreenPipeline_ = VK_NULL_HANDLE;

		std::array<QbVkBuffer, MAX_FRAMES_IN_FLIGHT> offscreenUniformBuffers_;
		std::array<QbVkBuffer, MAX_FRAMES_IN_FLIGHT> uniformBuffers_;

		RenderMeshPushConstants pushConstants_{};
		MeshBuffers meshBufs_{};

		Entity fallbackCamera_ = NULL_ENTITY;
		Entity userCamera_ = NULL_ENTITY;

		void CreateDescriptorPool();
		void CreateDescriptorSetLayout();
		void CreateDescriptorSets();
		void CreatePipeline();

		void DestroyVertexBuffer(VertexBufHandle handle);
		void DestroyIndexBuffer(IndexBufHandle handle);
		VertexBufHandle CreateVertexBuffer(const std::vector<MeshVertex>& vertices);
		IndexBufHandle CreateIndexBuffer(const std::vector<uint32_t>& indices);

		void CreateUniformBuffers();

		void UpdateUniformBuffers(uint32_t resourceIndex);
		void UpdateOffscreenUniformBuffers(uint32_t resourceIndex);
	};
}