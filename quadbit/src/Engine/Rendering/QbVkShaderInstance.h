#pragma once

#include <vulkan/vulkan.h>
#include <vector>

#include "Engine/Core/QbVulkanDefs.h"

namespace Quadbit {
	class QbVkShaderInstance {
	public:
		std::vector<VkPipelineShaderStageCreateInfo> stages;

		QbVkShaderInstance(QbVkContext& context) : context_(context) {}
		~QbVkShaderInstance();
		void AddShader(const uint32_t* shaderBytecode, const uint32_t shaderSize, const char* shaderEntry, VkShaderStageFlagBits shaderStage);
		void AddShader(const char* shaderPath, const char* shaderEntry, VkShaderStageFlagBits shaderStage);

	private:
		QbVkContext& context_;
	};
}
