#pragma once

#include "../RenderTypes.h"
#include "../Common/QbVkDefines.h"
#include "../../Entities/EntityManager.h"

namespace Quadbit {
	constexpr int MAX_MESH_COUNT = 65536;

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

		void DrawFrame(uint32_t resourceIndex, VkCommandBuffer commandbuffer);

	private:
		friend class QbVkRenderer;

		std::shared_ptr<QbVkContext> context_ = nullptr;
		EntityManager* entityManager_ = nullptr;

		VkPipelineLayout pipelineLayout_ = VK_NULL_HANDLE;
		VkPipeline pipeline_ = VK_NULL_HANDLE;

		MeshBuffers meshBufs_{};

		std::vector<QbVkRenderMeshInstance> externalInstances_;

		Entity fallbackCamera_ = NULL_ENTITY;
		Entity userCamera_ = NULL_ENTITY;

		void CreatePipeline();

		void DestroyVertexBuffer(VertexBufHandle handle);
		void DestroyIndexBuffer(IndexBufHandle handle);
		VertexBufHandle CreateVertexBuffer(const std::vector<MeshVertex>& vertices);
		IndexBufHandle CreateIndexBuffer(const std::vector<uint32_t>& indices);

		QbVkRenderMeshInstance* CreateInstance(std::vector<std::tuple<VkDescriptorType, void*, VkShaderStageFlagBits>> descriptors,
			const char* vertexShader, const char* vertexEntry, const char* fragmentShader, const char* fragmentEntry);
		void CreateUniformBuffers(QbVkRenderMeshInstance& renderMeshInstance, VkDeviceSize size);
	};
}