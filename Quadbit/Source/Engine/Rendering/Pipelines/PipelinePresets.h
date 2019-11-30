#pragma once

#include <vulkan/vulkan.h>

#include "Engine/Core/Logging.h"

namespace Quadbit {
	enum class QbVkPipelineDepth {
		QBVK_PIPELINE_DEPTH_DISABLE,
		QBVK_PIPELINE_DEPTH_ENABLE
	};

	enum class QbVkPipelineRasterization {
		QBVK_PIPELINE_RASTERIZATION_DEFAULT,
		QBVK_PIPELINE_RASTERIZATION_NOCULL,
		QBVK_PIPELINE_RASTERIZATION_SHADOWMAP
	};

	enum class QbVkPipelineColourBlending {
		QBVK_COLOURBLENDING_DISABLE,
		QBVK_COLOURBLENDING_ENABLE,
		QBVK_COLOURBLENDING_NOATTACHMENT
	};

	enum class QbVkPipelineDynamicState {
		QBVK_DYNAMICSTATE_VIEWPORTSCISSOR,
		QBVK_DYNAMICSTATE_DEPTHBIAS,
		QBVK_DYNAMICSTATE_NONE
	};

	struct QbVkPipelineDescription {
		QbVkPipelineDepth depth;
		QbVkPipelineRasterization rasterization;
		QbVkPipelineColourBlending colourBlending;
		QbVkPipelineDynamicState dynamicState;
		bool enableMSAA;
	};

	namespace Presets {
		constexpr VkPipelineDepthStencilStateCreateInfo PIPELINE_DEPTH_ENABLE{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.depthTestEnable = VK_TRUE,
			.depthWriteEnable = VK_TRUE,
			.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL,
			.depthBoundsTestEnable = VK_FALSE,
			.stencilTestEnable = VK_FALSE,
			.front = {},
			.back = {},
			.minDepthBounds = 0.0f,
			.maxDepthBounds = 1.0f
		};

		constexpr VkPipelineDepthStencilStateCreateInfo PIPELINE_DEPTH_DISABLE{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.depthTestEnable = VK_FALSE,
			.depthWriteEnable = VK_FALSE,
			.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL,
			.depthBoundsTestEnable = VK_FALSE,
			.stencilTestEnable = VK_FALSE,
			.front = {},
			.back = {},
			.minDepthBounds = 0.0f,
			.maxDepthBounds = 1.0f
		};

		constexpr VkPipelineColorBlendAttachmentState PIPELINE_BLEND_DISABLE{
			.blendEnable = VK_FALSE,
			.srcColorBlendFactor = VK_BLEND_FACTOR_ZERO,
			.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO,
			.colorBlendOp = VK_BLEND_OP_ADD,
			.srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
			.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
			.alphaBlendOp = VK_BLEND_OP_ADD,
			.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
							  VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
		};

		constexpr VkPipelineColorBlendAttachmentState PIPELINE_BLEND_ENABLE{
			.blendEnable = VK_TRUE,
			.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
			.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
			.colorBlendOp = VK_BLEND_OP_ADD,
			.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
			.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
			.alphaBlendOp = VK_BLEND_OP_ADD,
			.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
							  VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
		};

		constexpr VkPipelineRasterizationStateCreateInfo PIPELINE_RASTERIZATION_DEFAULT{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.depthClampEnable = VK_FALSE, // Requires a GPU feature to be enabled, useful for special cases like shadow maps
			.rasterizerDiscardEnable = VK_FALSE, // We will allow geometry to pass through the rasterizer
			.polygonMode = VK_POLYGON_MODE_FILL,
			.cullMode = VK_CULL_MODE_BACK_BIT,
			.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
			.depthBiasEnable = VK_FALSE, // We won't be doing any altering of depth values in the rasterizer
			.depthBiasConstantFactor = 0.0f,
			.depthBiasClamp = 0.0f,
			.depthBiasSlopeFactor = 0.0f,
			.lineWidth = 1.0f // Thickness of lines (in terms of fragments)
		};

		constexpr VkPipelineRasterizationStateCreateInfo PIPELINE_RASTERIZATION_NOCULL{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.depthClampEnable = VK_FALSE, // Requires a GPU feature to be enabled, useful for special cases like shadow maps
			.rasterizerDiscardEnable = VK_FALSE, // We will allow geometry to pass through the rasterizer
			.polygonMode = VK_POLYGON_MODE_FILL,
			.cullMode = VK_CULL_MODE_NONE,
			.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
			.depthBiasEnable = VK_FALSE, // We won't be doing any altering of depth values in the rasterizer
			.depthBiasConstantFactor = 0.0f,
			.depthBiasClamp = 0.0f,
			.depthBiasSlopeFactor = 0.0f,
			.lineWidth = 1.0f // Thickness of lines (in terms of fragments)
		};

		constexpr VkPipelineRasterizationStateCreateInfo PIPELINE_RASTERIZATION_SHADOWMAP{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.depthClampEnable = VK_FALSE, // Requires a GPU feature to be enabled, useful for special cases like shadow maps
			.rasterizerDiscardEnable = VK_FALSE, // We will allow geometry to pass through the rasterizer
			.polygonMode = VK_POLYGON_MODE_FILL,
			.cullMode = VK_CULL_MODE_BACK_BIT,
			.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
			.depthBiasEnable = VK_TRUE,
			.depthBiasConstantFactor = 0.0f,
			.depthBiasClamp = 0.0f,
			.depthBiasSlopeFactor = 0.0f,
			.lineWidth = 1.0f // Thickness of lines (in terms of fragments)
		};

		// Dynamic states will be viewport and scissor for user-defined pipelines
		constexpr eastl::array<VkDynamicState, 2> DYNAMICSTATES_VIEWPORTSCISSOR{
			VK_DYNAMIC_STATE_VIEWPORT,
			VK_DYNAMIC_STATE_SCISSOR
		};
		constexpr eastl::array<VkDynamicState, 1> DYNAMICSTATES_DEPTHBIAS{
			VK_DYNAMIC_STATE_DEPTH_BIAS
		};
		constexpr VkPipelineDynamicStateCreateInfo PIPELINE_DYNAMICSTATES_VIEWPORTSCISSOR{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.dynamicStateCount = static_cast<uint32_t>(DYNAMICSTATES_VIEWPORTSCISSOR.size()),
			.pDynamicStates = DYNAMICSTATES_VIEWPORTSCISSOR.data()
		};
		constexpr VkPipelineDynamicStateCreateInfo PIPELINE_DYNAMICSTATES_DEPTHBIAS{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.dynamicStateCount = static_cast<uint32_t>(DYNAMICSTATES_DEPTHBIAS.size()),
			.pDynamicStates = DYNAMICSTATES_DEPTHBIAS.data()
		};
		constexpr VkPipelineDynamicStateCreateInfo PIPELINE_DYNAMICSTATES_NONE{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.dynamicStateCount = 0,
			.pDynamicStates = nullptr
		};

		constexpr VkPipelineDepthStencilStateCreateInfo GetDepth(QbVkPipelineDepth depth) {
			switch (depth) {
			case QbVkPipelineDepth::QBVK_PIPELINE_DEPTH_DISABLE:
				return PIPELINE_DEPTH_DISABLE;
			case QbVkPipelineDepth::QBVK_PIPELINE_DEPTH_ENABLE:
				return PIPELINE_DEPTH_ENABLE;
			default:
				QB_ASSERT(false && "Unknown Depth Preset!");
				return PIPELINE_DEPTH_DISABLE;
			}
		}

		constexpr VkPipelineRasterizationStateCreateInfo GetRasterization(QbVkPipelineRasterization rasterization) {
			switch (rasterization) {
			case QbVkPipelineRasterization::QBVK_PIPELINE_RASTERIZATION_DEFAULT:
				return PIPELINE_RASTERIZATION_DEFAULT;
			case QbVkPipelineRasterization::QBVK_PIPELINE_RASTERIZATION_NOCULL:
				return PIPELINE_RASTERIZATION_NOCULL;
			case QbVkPipelineRasterization::QBVK_PIPELINE_RASTERIZATION_SHADOWMAP:
				return PIPELINE_RASTERIZATION_SHADOWMAP;
			default:
				QB_ASSERT(false && "Unknown Rasterization Preset!");
				return PIPELINE_RASTERIZATION_DEFAULT;
			}
		}

		constexpr VkPipelineColorBlendAttachmentState GetColorBlending(QbVkPipelineColourBlending blending) {
			switch (blending) {
			case QbVkPipelineColourBlending::QBVK_COLOURBLENDING_DISABLE:
				return PIPELINE_BLEND_DISABLE;
			case QbVkPipelineColourBlending::QBVK_COLOURBLENDING_ENABLE:
				return PIPELINE_BLEND_ENABLE;
			default:
				QB_ASSERT(false && "Unknown Blend Preset!");
				return PIPELINE_BLEND_DISABLE;
			}
		}

		constexpr VkPipelineDynamicStateCreateInfo GetDynamicState(QbVkPipelineDynamicState dynamicState) {
			switch (dynamicState) {
			case QbVkPipelineDynamicState::QBVK_DYNAMICSTATE_VIEWPORTSCISSOR:
				return PIPELINE_DYNAMICSTATES_VIEWPORTSCISSOR;
			case QbVkPipelineDynamicState::QBVK_DYNAMICSTATE_DEPTHBIAS:
				return PIPELINE_DYNAMICSTATES_DEPTHBIAS;
			case QbVkPipelineDynamicState::QBVK_DYNAMICSTATE_NONE:
				return PIPELINE_DYNAMICSTATES_NONE;
			default:
				QB_ASSERT(false && "Unknown DynamicStates Preset!");
				return PIPELINE_DYNAMICSTATES_VIEWPORTSCISSOR;
			}
		}
	}



}