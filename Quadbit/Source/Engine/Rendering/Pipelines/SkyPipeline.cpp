#include "SkyPipeline.h"

#include <imgui/imgui.h>

#include "Engine/Entities/EntityManager.h"
#include "Engine/Rendering/Geometry/Icosphere.h"
#include "Engine/Rendering/Memory/ResourceManager.h"
#include "Engine/Rendering/Pipelines/PipelinePresets.h"

Quadbit::SkyPipeline::SkyPipeline(QbVkContext& context) : context_(context) {
	QbVkPipelineDescription pipelineDescription;
	pipelineDescription.colourBlending = QbVkPipelineColourBlending::QBVK_COLOURBLENDING_DISABLE;
	pipelineDescription.depth = QbVkPipelineDepth::QBVK_PIPELINE_DEPTH_DISABLE;
	pipelineDescription.dynamicState = QbVkPipelineDynamicState::QBVK_DYNAMICSTATE_NONE;
	pipelineDescription.enableMSAA = true;
	pipelineDescription.rasterization = QbVkPipelineRasterization::QBVK_PIPELINE_RASTERIZATION_NOCULL;
	pipeline_ = context_.resourceManager->CreateGraphicsPipeline("Assets/Quadbit/Shaders/sky.vert", "main",
		"Assets/Quadbit/Shaders/sky.frag", "main", pipelineDescription, context_.mainRenderPass);

	auto sphere = Icosphere(0);
	vertexBuffer_ = context_.resourceManager->CreateVertexBuffer(sphere.vertices_.data(), sizeof(IcosphereVertex), static_cast<uint32_t>(sphere.vertices_.size()));
	indexBuffer_ = context_.resourceManager->CreateIndexBuffer(sphere.indices_);
	indexCount_ = static_cast<uint32_t>(sphere.indices_.size());
}

void Quadbit::SkyPipeline::DrawFrame(VkCommandBuffer& commandBuffer) {
	constexpr float PIHALF = 1.5707963268f;
	constexpr float PI2 = 6.2831853072f;

	ImGui::Begin("Sun Position");
	ImGui::DragFloat("Sun Azimuth", &context_.sunAzimuth, 0.01f, 0.0f, PI2);
	ImGui::DragFloat("Sun Altitude", &context_.sunAltitude, 0.01f, 0.0f, PIHALF);
	ImGui::End();

	auto& pipeline = context_.resourceManager->pipelines_[pipeline_];
	static const VkDeviceSize offsets[]{ 0 };

	pipeline->Bind(commandBuffer);

	auto* camera = context_.entityManager->GetComponentPtr<RenderCamera>(context_.GetActiveCamera());
	pushConstants_.mvp = (camera->perspective * glm::mat4(glm::mat3(camera->view)) * glm::mat4(1.0f));
	pushConstants_.azimuth = context_.sunAzimuth;
	pushConstants_.altitude = context_.sunAltitude;

	vkCmdPushConstants(commandBuffer, pipeline->pipelineLayout_, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(SkyPushConstants), &pushConstants_);

	vkCmdBindVertexBuffers(commandBuffer, 0, 1, &context_.resourceManager->buffers_[vertexBuffer_].buf, offsets);
	vkCmdBindIndexBuffer(commandBuffer, context_.resourceManager->buffers_[indexBuffer_].buf, 0, VK_INDEX_TYPE_UINT32);

	vkCmdDrawIndexed(commandBuffer, indexCount_, 1, 0, 0, 0);
}
