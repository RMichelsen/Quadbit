#pragma once

#include <Engine/Rendering/VulkanTypes.h>

namespace Quadbit {
	struct SkyPushConstants {
		glm::mat4 mvp;
		float azimuth;
		float altitude;
	};

	class SkyPipeline {
	public:
		SkyPipeline(QbVkContext& context);
		void DrawFrame(VkCommandBuffer& commandBuffer);

	private:
		QbVkContext& context_;

		SkyPushConstants pushConstants_{};

		QbVkPipelineHandle pipeline_ = QBVK_PIPELINE_NULL_HANDLE;
		QbVkBufferHandle vertexBuffer_ = QBVK_BUFFER_NULL_HANDLE;
		QbVkBufferHandle indexBuffer_ = QBVK_BUFFER_NULL_HANDLE;
		uint32_t indexCount_ = 0;
	};
}