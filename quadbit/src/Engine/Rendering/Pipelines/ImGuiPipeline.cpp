#include "ImGuiPipeline.h"

#include <EASTL/algorithm.h>
#include <EASTL/chrono.h>
#include <EASTL/numeric.h>
#include <imgui/imgui.h>

#include "Engine/Application/InputHandler.h"
#include "Engine/Core/Time.h"
#include "Engine/Entities/EntityManager.h"
#include "Engine/Rendering/VulkanUtils.h"
#include "Engine/Rendering/ShaderBytecode.h"
#include "Engine/Rendering/Pipelines/ComputePipeline.h"

namespace Quadbit {
	ImGuiPipeline::ImGuiPipeline(QbVkContext& context) : context_(context) {
		InitImGui();

		CreateFontTexture();

		QbVkPipelineDescription pipelineDescription;
		pipelineDescription.colorBlending = QbVkPipelineColorBlending::QBVK_COLORBLENDING_ENABLE;
		pipelineDescription.depth = QbVkPipelineDepth::QBVK_PIPELINE_DEPTH_DISABLE;
		pipelineDescription.dynamicState = QbVkPipelineDynamicState::QBVK_DYNAMICSTATE_VIEWPORTSCISSOR;
		pipelineDescription.enableMSAA = true;
		pipelineDescription.rasterization = QbVkPipelineRasterization::QBVK_PIPELINE_RASTERIZATION_NOCULL;

		pipeline_ = eastl::make_unique<QbVkPipeline>(context_, imguiVert.data(), imguiVert.size(),
			imguiFrag.data(), imguiFrag.size(), pipelineDescription, 1,
			eastl::vector<eastl::tuple<VkFormat, uint32_t>> { {
				{VK_FORMAT_R32G32_SFLOAT, 8},
				{VK_FORMAT_R32G32_SFLOAT, 8},
				{VK_FORMAT_R8G8B8A8_UNORM, 4}
			} });
		pipeline_->BindResource("fontSampler", fontTexture_);
	}

	ImGuiPipeline::~ImGuiPipeline() {
		ImGui::DestroyContext();

		// Destroy buffers
		for (auto i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
			context_.allocator->DestroyBuffer(vertexBuffer_[i]);
			context_.allocator->DestroyBuffer(indexBuffer_[i]);
		}
	}

	void ImGuiPipeline::UpdateBuffers(uint32_t resourceIndex) {
		ImDrawData* imDrawData = ImGui::GetDrawData();

		VkDeviceSize vertexBufferSize = imDrawData->TotalVtxCount * sizeof(ImDrawVert);
		VkDeviceSize indexBufferSize = imDrawData->TotalIdxCount * sizeof(ImDrawIdx);

		if (vertexBufferSize == 0 || indexBufferSize == 0) return;

		// Only update buffers if the vertex or index count has changed
		if ((vertexBuffer_[resourceIndex].buf == VK_NULL_HANDLE) || (vertexBuffer_[resourceIndex].alloc.size != vertexBufferSize)) {
			context_.allocator->DestroyBuffer(vertexBuffer_[resourceIndex]);
			VkBufferCreateInfo bufferCreateInfo = VkUtils::Init::BufferCreateInfo(vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
			context_.allocator->CreateBuffer(vertexBuffer_[resourceIndex], bufferCreateInfo, QbVkMemoryUsage::QBVK_MEMORY_USAGE_CPU_TO_GPU);
		}
		if ((indexBuffer_[resourceIndex].buf == VK_NULL_HANDLE) || (indexBuffer_[resourceIndex].alloc.size != indexBufferSize)) {
			context_.allocator->DestroyBuffer(indexBuffer_[resourceIndex]);
			VkBufferCreateInfo bufferCreateInfo = VkUtils::Init::BufferCreateInfo(vertexBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
			context_.allocator->CreateBuffer(indexBuffer_[resourceIndex], bufferCreateInfo, QbVkMemoryUsage::QBVK_MEMORY_USAGE_CPU_TO_GPU);
		}

		ImDrawVert* vertexMappedData = reinterpret_cast<ImDrawVert*>(vertexBuffer_[resourceIndex].alloc.data);
		ImDrawIdx* indexMappedData = reinterpret_cast<ImDrawIdx*>(indexBuffer_[resourceIndex].alloc.data);
		for (auto i = 0; i < imDrawData->CmdListsCount; i++) {
			const ImDrawList* cmdList = imDrawData->CmdLists[i];
			// Copy buffer data
			memcpy(vertexMappedData, cmdList->VtxBuffer.Data, cmdList->VtxBuffer.Size * sizeof(ImDrawVert));
			memcpy(indexMappedData, cmdList->IdxBuffer.Data, cmdList->IdxBuffer.Size * sizeof(ImDrawIdx));
			// Increment pointer to get to the next data
			vertexMappedData += cmdList->VtxBuffer.Size;
			indexMappedData += cmdList->IdxBuffer.Size;
		}
	}

	void ImGuiPipeline::DrawFrame(uint32_t resourceIndex, VkCommandBuffer commandbuffer) {
		// Build new commandbuffer
		UpdateBuffers(resourceIndex);

		// Get draw data
		ImGuiIO& io = ImGui::GetIO();
		ImDrawData* imDrawData = ImGui::GetDrawData();
		uint32_t vertexOffset = 0;
		uint32_t indexOffset = 0;

		if (imDrawData == nullptr || imDrawData->CmdListsCount == 0) {
			return;
		}

		pipeline_->Bind(commandbuffer);

		auto& descriptorSets = context_.resourceManager->GetDescriptorSets(pipeline_->descriptorAllocator_, pipeline_->mainDescriptors_);
		vkCmdBindDescriptorSets(commandbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_->pipelineLayout_, 0, 
			static_cast<uint32_t>(descriptorSets[resourceIndex].size()), descriptorSets[resourceIndex].data(), 0, nullptr);

		// Set the viewport
		VkViewport viewport = VkUtils::Init::Viewport(io.DisplaySize.y, io.DisplaySize.x, 0.0f, 1.0f);
		vkCmdSetViewport(commandbuffer, 0, 1, &viewport);

		// Update the push constants for scale and translation
		pushConstBlock_.scale = glm::vec2(2.0f / io.DisplaySize.x, 2.0f / io.DisplaySize.y);
		pushConstBlock_.translate = glm::vec2(-1.0f);
		vkCmdPushConstants(commandbuffer, pipeline_->pipelineLayout_, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstBlock), &pushConstBlock_);

		if (imDrawData->CmdListsCount > 0) {
			VkDeviceSize offsets[1] = { 0 };
			vkCmdBindVertexBuffers(commandbuffer, 0, 1, &vertexBuffer_[resourceIndex].buf, offsets);
			vkCmdBindIndexBuffer(commandbuffer, indexBuffer_[resourceIndex].buf, 0, VK_INDEX_TYPE_UINT16);

			for (auto i = 0; i < imDrawData->CmdListsCount; i++) {
				const ImDrawList* cmdList = imDrawData->CmdLists[i];

				for (auto j = 0; j < cmdList->CmdBuffer.Size; j++) {
					const ImDrawCmd* pCmd = &cmdList->CmdBuffer[j];
					VkRect2D scissorRect = VkUtils::Init::ScissorRect(
						eastl::max(static_cast<int32_t>(pCmd->ClipRect.x), 0),
						eastl::max(static_cast<int32_t>(pCmd->ClipRect.y), 0),
						static_cast<uint32_t>(pCmd->ClipRect.z - pCmd->ClipRect.x),
						static_cast<uint32_t>(pCmd->ClipRect.w - pCmd->ClipRect.y));
					vkCmdSetScissor(commandbuffer, 0, 1, &scissorRect);
					vkCmdDrawIndexed(commandbuffer, pCmd->ElemCount, 1, indexOffset, vertexOffset, 0);
					indexOffset += pCmd->ElemCount;
				}
				vertexOffset += cmdList->VtxBuffer.Size;
			}
		}
	}

	void ImGuiPipeline::InitImGui() {
		ImGui::CreateContext();

		// Color scheme
		ImGui::StyleColorsDark();
		ImGuiStyle& style = ImGui::GetStyle();
		style.WindowRounding = 0.0f;
		style.GrabRounding = 0.0f;
		// Dimensions
		ImGuiIO& io = ImGui::GetIO();
		io.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/calibri.ttf", 20.0f);
		io.DisplaySize = ImVec2((float)context_.swapchain.extent.width, (float)context_.swapchain.extent.height);
	}

	void ImGuiPipeline::CreateFontTexture() {
		ImGuiIO& io = ImGui::GetIO();

		// Create font texture
		unsigned char* fontData;
		int textureWidth, textureHeight;
		io.Fonts->GetTexDataAsRGBA32(&fontData, &textureWidth, &textureHeight);

		// Font texture sampler
		VkSamplerCreateInfo samplerInfo = VkUtils::Init::SamplerCreateInfo();
		samplerInfo.magFilter = VK_FILTER_LINEAR;
		samplerInfo.minFilter = VK_FILTER_LINEAR;
		samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;

		fontTexture_ = context_.resourceManager->LoadTexture(textureWidth, textureHeight, fontData, &samplerInfo);
	}

	void ImGuiPipeline::ImGuiDrawState() {
		frametimeCache_.push_back(Time::deltaTime);

		static auto tStart = eastl::chrono::time_point_cast<eastl::chrono::nanoseconds>(eastl::chrono::high_resolution_clock::now());
		auto tEnd = eastl::chrono::high_resolution_clock::now();

		if (static_cast<eastl::chrono::duration<float, eastl::ratio<1>>>(tEnd - tStart).count() > 1) {
			currentFrametime_ = eastl::accumulate(frametimeCache_.begin(), frametimeCache_.end(), 0.0f) / frametimeCache_.size() * 1000.0f;
			currentFPS_ = frametimeCache_.size();
			tStart = eastl::chrono::high_resolution_clock::now();
			frametimeCache_.clear();
		}

		const float margin = 10.0f;
		ImVec2 window_pos = ImVec2(margin, margin);
		ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always);
		ImGui::SetNextWindowBgAlpha(0.5f);
		ImGui::Begin("Stats", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav);

		ImGui::Text("Frametime: %.2f ms", currentFrametime_);
		ImGui::Separator();
		ImGui::Text("FPS: %llu", currentFPS_);

		ImGui::End();
	}
}