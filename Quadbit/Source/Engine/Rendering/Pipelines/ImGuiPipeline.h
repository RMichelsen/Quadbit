#pragma once

#include "../Common/QbVkCommon.h"

namespace Quadbit {
	class ImGuiPipeline {
	public:
		ImGuiPipeline(std::shared_ptr<QbVkContext> context);
		~ImGuiPipeline();

		void NewFrame();
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

		//VkDeviceMemory fontImageMemory_ = VK_NULL_HANDLE;
		//VkImage fontImage_ = VK_NULL_HANDLE;
		QbVkImage fontImage_{};
		VkImageView fontImageView_ = VK_NULL_HANDLE;

		VkSampler fontSampler_ = VK_NULL_HANDLE;

		VkDescriptorPool descriptorPool_ = VK_NULL_HANDLE;
		VkDescriptorSetLayout descriptorSetLayout_ = VK_NULL_HANDLE;
		VkDescriptorSet descriptorSet_ = VK_NULL_HANDLE;

		VkPipelineLayout pipelineLayout_ = VK_NULL_HANDLE;
		VkPipeline pipeline_ = VK_NULL_HANDLE;
		VkPipelineCache pipelineCache_ = VK_NULL_HANDLE;
		PushConstBlock pushConstBlock_{};

		std::array<QbVkBuffer, MAX_FRAMES_IN_FLIGHT> vertexBuffer_;
		std::array<QbVkBuffer, MAX_FRAMES_IN_FLIGHT> indexBuffer_;

		void InitImGui();
		void CreateFontTexture();
		void CreateFontTextureSampler();
		void CreateDescriptorPoolAndSets();
		void CreatePipeline();
	};
}