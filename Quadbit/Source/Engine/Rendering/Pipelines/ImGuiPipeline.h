#pragma once

#include <cstdint>
#include <EASTL/array.h>
#include <EASTL/vector.h>

#include "Engine/Rendering/RenderTypes.h"
#include "Engine/Rendering/VulkanTypes.h"

namespace Quadbit {
	class ImGuiPipeline {
	public:
		ImGuiPipeline(QbVkContext& context);
		~ImGuiPipeline();

		void UpdateBuffers(uint32_t resourceIndex);
		void ImGuiDrawState();
		void DrawFrame(uint32_t resourceIndex, VkCommandBuffer commandBuffer);

	private:
		// Push constant struct to set UI parameters
		struct PushConstBlock {
			glm::vec2 scale;
			glm::vec2 translate;
		};

		QbVkContext& context_;

		size_t currentFPS_;
		float currentFrametime_;
		eastl::vector<float> frametimeCache_;

		QbVkPipelineHandle pipeline_;
		QbVkTextureHandle fontTexture_ = QBVK_TEXTURE_NULL_HANDLE;

		PushConstBlock pushConstBlock_{};

		eastl::array<QbVkBuffer, MAX_FRAMES_IN_FLIGHT> vertexBuffer_;
		eastl::array<QbVkBuffer, MAX_FRAMES_IN_FLIGHT> indexBuffer_;

		void InitImGui();
		void CreateFontTexture();
	};
}