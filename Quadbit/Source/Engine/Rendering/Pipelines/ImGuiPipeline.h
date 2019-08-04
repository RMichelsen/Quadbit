#pragma once

#include "../Common/QbVkDefines.h"

namespace Quadbit {
	class ImGuiPipeline {
	public:
		ImGuiPipeline(std::shared_ptr<QbVkContext> context);
		~ImGuiPipeline();

		void UpdateBuffers(uint32_t resourceIndex);
		void DrawFrame(uint32_t resourceIndex, VkCommandBuffer commandbuffer);
		void RebuildPipeline();

	private:
		// Push constant struct to set UI parameters
		struct PushConstBlock {
			glm::vec2 scale;
			glm::vec2 translate;
		};

		std::shared_ptr<QbVkContext> context_ = nullptr;

		QbVkTexture fontTexture_{};

		VkDescriptorPool descriptorPool_ = VK_NULL_HANDLE;
		VkDescriptorSetLayout descriptorSetLayout_ = VK_NULL_HANDLE;
		VkDescriptorSet descriptorSet_ = VK_NULL_HANDLE;

		VkPipelineLayout pipelineLayout_ = VK_NULL_HANDLE;
		VkPipeline pipeline_ = VK_NULL_HANDLE;
		VkPipelineCache pipelineCache_ = VK_NULL_HANDLE;
		PushConstBlock pushConstBlock_{};

		std::array<QbVkBuffer, MAX_FRAMES_IN_FLIGHT> vertexBuffer_;
		std::array<QbVkBuffer, MAX_FRAMES_IN_FLIGHT> indexBuffer_;

		std::size_t currentFPS_;
		float currentFrametime_;
		std::vector<float> frametimeCache_;

		void InitImGui();
		void CreateFontTexture();
		void CreateDescriptorPoolAndSets();
		void CreatePipeline();

		void DrawStats();
		void NewFrame();
	};
}