#include <PCH.h>
#include <ImGui/imgui.h>
#include "ImGuiPipeline.h"

#include "../Common/QbVkUtils.h"
#include "../../Global/InputHandler.h"
#include "../../Global/Time.h"
#include "../../Global/ImGuiState.h"

ImGuiPipeline::ImGuiPipeline(std::shared_ptr<QbVkContext> context) : context_(context) {
	InitImGui();

	CreateFontTexture();
	CreateFontTextureSampler();
	CreateDescriptorPoolAndSets();
	CreatePipeline();
}

ImGuiPipeline::~ImGuiPipeline() {
	ImGui::DestroyContext();

	// Since both the pipeline and layout are destroyed by the swapchain we should check before deleting
	if(pipeline_ != VK_NULL_HANDLE) {
		vkDestroyPipeline(context_->device, pipeline_, nullptr);
	}
	if(pipelineLayout_ != VK_NULL_HANDLE) {
		vkDestroyPipelineLayout(context_->device, pipelineLayout_, nullptr);
	}

	// Destroy font resources
	context_->allocator->DestroyImage(fontImage_);
	vkDestroyImageView(context_->device, fontImageView_, nullptr);
	vkDestroySampler(context_->device, fontSampler_, nullptr);

	// Since both the pipeline and pipeline layout are destroyed by the swapchain we ignore them here.
	vkDestroyDescriptorPool(context_->device, descriptorPool_, nullptr);

	// Destroy descript set layout
	vkDestroyDescriptorSetLayout(context_->device, descriptorSetLayout_, nullptr);

	// Destroy buffers
	for(auto i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		context_->allocator->DestroyBuffer(vertexBuffer_[i]);
		context_->allocator->DestroyBuffer(indexBuffer_[i]);
	}
}

void ImGuiPipeline::NewFrame() {
	ImGuiIO& io = ImGui::GetIO();
	io.DeltaTime = Time::deltaTime;

	io.MousePos = ImVec2(static_cast<float>(InputHandler::clientMousePos.x), static_cast<float>(InputHandler::clientMousePos.y));
	io.MouseDown[0] = InputHandler::mouseButtonStatus.left;
	io.MouseDown[1] = InputHandler::mouseButtonStatus.right;

	ImGui::NewFrame();

	ImGui::ShowDemoWindow();

	// Get injected ImGui commands from the global state
	for(const auto& injector : ImGuiState::injectors) {
		injector();
	}

	// Render to generate draw buffers
	ImGui::Render();
}

void ImGuiPipeline::UpdateBuffers(uint32_t resourceIndex) {
	ImDrawData* imDrawData = ImGui::GetDrawData();

	VkDeviceSize vertexBufferSize = imDrawData->TotalVtxCount * sizeof(ImDrawVert);
	VkDeviceSize indexBufferSize = imDrawData->TotalIdxCount * sizeof(ImDrawIdx);

	if(vertexBufferSize == 0 || indexBufferSize == 0) return;

	// Only update buffers if the vertex or index count has changed
	if((vertexBuffer_[resourceIndex].buf == VK_NULL_HANDLE) || (vertexBuffer_[resourceIndex].alloc.size != vertexBufferSize)) {
		context_->allocator->DestroyBuffer(vertexBuffer_[resourceIndex]);
		VkBufferCreateInfo bufferCreateInfo = VkUtils::Init::BufferCreateInfo(vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
		context_->allocator->CreateBuffer(vertexBuffer_[resourceIndex], bufferCreateInfo, QBVK_MEMORY_USAGE_CPU_TO_GPU);
	}
	if((indexBuffer_[resourceIndex].buf == VK_NULL_HANDLE) || (indexBuffer_[resourceIndex].alloc.size != indexBufferSize)) {
		context_->allocator->DestroyBuffer(indexBuffer_[resourceIndex]);
		VkBufferCreateInfo bufferCreateInfo = VkUtils::Init::BufferCreateInfo(vertexBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
		context_->allocator->CreateBuffer(indexBuffer_[resourceIndex], bufferCreateInfo, QBVK_MEMORY_USAGE_CPU_TO_GPU);
	}

	ImDrawVert* vertexMappedData = reinterpret_cast<ImDrawVert*>(vertexBuffer_[resourceIndex].alloc.data);
	ImDrawIdx* indexMappedData = reinterpret_cast<ImDrawIdx*>(indexBuffer_[resourceIndex].alloc.data);
	for(auto i = 0; i < imDrawData->CmdListsCount; i++) {
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
	// Setup a new frame
	NewFrame();
	// Build new commandbuffer
	UpdateBuffers(resourceIndex);

	// Get draw data
	ImGuiIO& io = ImGui::GetIO();
	ImDrawData* imDrawData = ImGui::GetDrawData();
	uint32_t vertexOffset = 0;
	uint32_t indexOffset = 0;

	if(imDrawData == nullptr || imDrawData->CmdListsCount == 0) {
		return;
	}

	vkCmdBindPipeline(commandbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
	vkCmdBindDescriptorSets(commandbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout_, 0, 1, &descriptorSet_, 0, nullptr);

	// Set the viewport
	VkViewport viewport = VkUtils::Init::Viewport(io.DisplaySize.y, io.DisplaySize.x, 0.0f, 1.0f);
	vkCmdSetViewport(commandbuffer, 0, 1, &viewport);

	// Update the push constants for scale and translation
	pushConstBlock_.scale = glm::vec2(2.0f / io.DisplaySize.x, 2.0f / io.DisplaySize.y);
	pushConstBlock_.translate = glm::vec2(-1.0f);
	vkCmdPushConstants(commandbuffer, pipelineLayout_, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstBlock), &pushConstBlock_);

	if(imDrawData->CmdListsCount > 0) {
		VkDeviceSize offsets[1] = { 0 };
		vkCmdBindVertexBuffers(commandbuffer, 0, 1, &vertexBuffer_[resourceIndex].buf, offsets);
		vkCmdBindIndexBuffer(commandbuffer, indexBuffer_[resourceIndex].buf, 0, VK_INDEX_TYPE_UINT16);

		for(auto i = 0; i < imDrawData->CmdListsCount; i++) {
			const ImDrawList* cmdList = imDrawData->CmdLists[i];

			for(auto j = 0; j < cmdList->CmdBuffer.Size; j++) {
				const ImDrawCmd* pCmd = &cmdList->CmdBuffer[j];
				VkRect2D scissorRect = VkUtils::Init::ScissorRect(
					std::max(static_cast<int32_t>(pCmd->ClipRect.x), 0),
					std::max(static_cast<int32_t>(pCmd->ClipRect.y), 0),
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

void ImGuiPipeline::RebuildPipeline() {
	// Destroy old pipeline
	vkDestroyPipelineLayout(context_->device, pipelineLayout_, nullptr);
	vkDestroyPipeline(context_->device, pipeline_, nullptr);
	// Create new pipeline
	CreatePipeline();
}

void ImGuiPipeline::InitImGui() {
	ImGui::CreateContext();

	// Color scheme
	ImGuiStyle& style = ImGui::GetStyle();
	style.Colors[ImGuiCol_TitleBg] = ImVec4(1.0f, 0.0f, 0.0f, 0.6f);
	style.Colors[ImGuiCol_TitleBgActive] = ImVec4(1.0f, 0.0f, 0.0f, 0.8f);
	style.Colors[ImGuiCol_MenuBarBg] = ImVec4(1.0f, 0.0f, 0.0f, 0.4f);
	style.Colors[ImGuiCol_Header] = ImVec4(1.0f, 0.0f, 0.0f, 0.4f);
	style.Colors[ImGuiCol_CheckMark] = ImVec4(0.0f, 1.0f, 0.0f, 1.0f);
	style.Colors[ImGuiCol_ButtonHovered] = ImVec4(1.0f, 0.0f, 0.0f, 0.6f);
	// Dimensions
	ImGuiIO& io = ImGui::GetIO();
	io.DisplaySize = ImVec2((float)context_->swapchain.extent.width, (float)context_->swapchain.extent.height);
	//io.DisplaySize = ImVec2(1920.0f, 1080.0f);
	io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);
}

void ImGuiPipeline::CreateFontTexture() {
	ImGuiIO& io = ImGui::GetIO();

	// Create font texture
	unsigned char* fontData;
	int textureWidth, textureHeight;
	io.Fonts->GetTexDataAsRGBA32(&fontData, &textureWidth, &textureHeight);
	VkDeviceSize uploadSize = static_cast<uint64_t>(textureWidth) * static_cast<uint64_t>(textureHeight) * 4;

	// Create target image
	//VkUtils::CreateImage(context_, textureWidth, textureHeight, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_OPTIMAL,
		//VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, fontImage_, fontImageMemory_);

	VkImageCreateInfo imageInfo = VkUtils::Init::ImageCreateInfo(textureWidth, textureHeight, VK_FORMAT_R8G8B8A8_UNORM, 
		VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
	context_->allocator->CreateImage(fontImage_, imageInfo, QBVK_MEMORY_USAGE_GPU_ONLY);

	// Create target image view
	fontImageView_ = VkUtils::CreateImageView(context_, fontImage_.img, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT);

	// Create buffer for transfer
	VkCommandBuffer commandBuffer = VkUtils::InitSingleTimeCommandBuffer(context_);

	// Create staging buffer
	QbVkBuffer stagingBuffer;
	context_->allocator->CreateStagingBuffer(stagingBuffer, uploadSize, fontData);

	// Prepare image for transfer
	VkUtils::TransitionImageLayout(context_, commandBuffer, fontImage_.img, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

	// Copy image
	VkBufferImageCopy bufferCopyRegion{};
	bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	bufferCopyRegion.imageSubresource.layerCount = 1;
	bufferCopyRegion.imageExtent.width = textureWidth;
	bufferCopyRegion.imageExtent.height = textureHeight;
	bufferCopyRegion.imageExtent.depth = 1;
	vkCmdCopyBufferToImage(commandBuffer, stagingBuffer.buf, fontImage_.img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &bufferCopyRegion);

	// Prepare for shader read
	VkUtils::TransitionImageLayout(context_, commandBuffer, fontImage_.img, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

	// Flush command buffer (also frees it)
	VkUtils::FlushCommandBuffer(context_, commandBuffer);

	// Free staging buffer
	context_->allocator->DestroyBuffer(stagingBuffer);
}

void ImGuiPipeline::CreateFontTextureSampler() {
	// Create font texture sampler
	VkSamplerCreateInfo samplerInfo = VkUtils::Init::SamplerCreateInfo();
	samplerInfo.magFilter = VK_FILTER_LINEAR;
	samplerInfo.minFilter = VK_FILTER_LINEAR;
	samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
	VK_CHECK(vkCreateSampler(context_->device, &samplerInfo, nullptr, &fontSampler_));
}

void ImGuiPipeline::CreateDescriptorPoolAndSets() {
	// Create descriptor pool
	VkDescriptorPoolSize poolSize{};
	poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	poolSize.descriptorCount = 1;

	// Maxsets here refer to the amount of descriptor sets that can be allocated from the pool
	VkDescriptorPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.poolSizeCount = 1;
	poolInfo.pPoolSizes = &poolSize;
	poolInfo.maxSets = static_cast<uint32_t>(context_->swapchain.images.size());
	VK_CHECK(vkCreateDescriptorPool(context_->device, &poolInfo, nullptr, &descriptorPool_));

	// Create descriptor set layout
	// The layout binding here specifies that we're creating an image sampler to be used in the fragment shader
	VkDescriptorSetLayoutBinding layoutBinding = VkUtils::Init::DescriptorSetLayoutBinding(0,
		VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);

	VkDescriptorSetLayoutCreateInfo layoutInfo = VkUtils::Init::DescriptorSetLayoutCreateInfo();
	layoutInfo.bindingCount = 1;
	layoutInfo.pBindings = &layoutBinding;
	VK_CHECK(vkCreateDescriptorSetLayout(context_->device, &layoutInfo, nullptr, &descriptorSetLayout_));

	// Finally, create descriptor set
	VkDescriptorSetAllocateInfo descAllocInfo = VkUtils::Init::DescriptorSetAllocateInfo();
	descAllocInfo.descriptorPool = descriptorPool_;
	descAllocInfo.descriptorSetCount = 1;
	descAllocInfo.pSetLayouts = &descriptorSetLayout_;

	// Allocate
	VK_CHECK(vkAllocateDescriptorSets(context_->device, &descAllocInfo, &descriptorSet_));

	// Create descriptor
	VkDescriptorImageInfo fontDescriptor{};
	fontDescriptor.sampler = fontSampler_;
	fontDescriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	fontDescriptor.imageView = fontImageView_;

	// Write
	VkWriteDescriptorSet writeDesc = VkUtils::Init::WriteDescriptorSet();
	writeDesc.dstSet = descriptorSet_;
	writeDesc.dstBinding = 0;
	writeDesc.dstArrayElement = 0;
	writeDesc.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writeDesc.descriptorCount = 1;
	writeDesc.pBufferInfo = nullptr;
	writeDesc.pImageInfo = &fontDescriptor;
	writeDesc.pTexelBufferView = nullptr;

	// Update
	vkUpdateDescriptorSets(context_->device, 1, &writeDesc, 0, nullptr);
}

void ImGuiPipeline::CreatePipeline() {
	// Create pipeline layout
	VkPipelineLayoutCreateInfo pipelineLayoutInfo = VkUtils::Init::PipelineLayoutCreateInfo();

	// Specify push constants
	VkPushConstantRange pushConstantRange{};
	pushConstantRange.size = sizeof(PushConstBlock);
	pushConstantRange.offset = 0;
	pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	pipelineLayoutInfo.pushConstantRangeCount = 1;
	pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
	pipelineLayoutInfo.setLayoutCount = 1;
	pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout_;
	VK_CHECK(vkCreatePipelineLayout(context_->device, &pipelineLayoutInfo, nullptr, &pipelineLayout_));


	// Create pipeline cache
	VkPipelineCacheCreateInfo pipelineCacheInfo = VkUtils::Init::PipelineCacheCreateInfo();
	VK_CHECK(vkCreatePipelineCache(context_->device, &pipelineCacheInfo, nullptr, &pipelineCache_));

	// We start off by reading the shader bytecode from disk and creating the modules subsequently
	std::vector<char> vertexShaderBytecode = VkUtils::ReadShader("Resources/Shaders/Compiled/imgui_vert.spv");
	std::vector<char> fragmentShaderBytecode = VkUtils::ReadShader("Resources/Shaders/Compiled/imgui_frag.spv");
	VkShaderModule vertShaderModule = VkUtils::CreateShaderModule(vertexShaderBytecode, context_->device);
	VkShaderModule fragShaderModule = VkUtils::CreateShaderModule(fragmentShaderBytecode, context_->device);

	// This part specifies the two shader types used in the pipeline
	VkPipelineShaderStageCreateInfo shaderStageInfo[2] = {
		VkUtils::Init::PipelineShaderStageCreateInfo(),
		VkUtils::Init::PipelineShaderStageCreateInfo()
	};
	shaderStageInfo[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	shaderStageInfo[0].module = vertShaderModule;
	shaderStageInfo[0].pName = "main";
	shaderStageInfo[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	shaderStageInfo[1].module = fragShaderModule;
	shaderStageInfo[1].pName = "main";

	// This part specifies the format of the vertex data passed to the vertex shader
	const std::array<VkVertexInputBindingDescription, 1> bindingDescriptions{
		VkUtils::Init::VertexInputBindingDescription(0, sizeof(ImDrawVert), VK_VERTEX_INPUT_RATE_VERTEX)
	};
	const std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions {
		VkUtils::Init::VertexInputAttributeDescription(0, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(ImDrawVert, pos)),
		VkUtils::Init::VertexInputAttributeDescription(0, 1, VK_FORMAT_R32G32_SFLOAT, offsetof(ImDrawVert, uv)),
		VkUtils::Init::VertexInputAttributeDescription(0, 2, VK_FORMAT_R8G8B8A8_UNORM, offsetof(ImDrawVert, col)),
	};
	VkPipelineVertexInputStateCreateInfo vertexInputInfo =
		VkUtils::Init::PipelineVertexInputStateCreateInfo();
	vertexInputInfo.vertexBindingDescriptionCount = static_cast<uint32_t>(bindingDescriptions.size());;
	vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
	vertexInputInfo.pVertexBindingDescriptions = bindingDescriptions.data();
	vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

	// This part specifies the geometry drawn
	VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo = VkUtils::Init::PipelineInputAssemblyStateCreateInfo();
	inputAssemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

	// Onto the create info struct as usual
	VkPipelineViewportStateCreateInfo viewportInfo = VkUtils::Init::PipelineViewportStateCreateInfo();
	viewportInfo.viewportCount = 1;
	viewportInfo.scissorCount = 1;

	// This part specifies the rasterization stage
	VkPipelineRasterizationStateCreateInfo rasterizerInfo = VkUtils::Init::PipelineRasterizationStateCreateInfo();
	// Standard fill mode polygons
	rasterizerInfo.polygonMode = VK_POLYGON_MODE_FILL;
	// No culling for ImGui
	rasterizerInfo.cullMode = VK_CULL_MODE_NONE;
	// Counter clockwise triangle faces
	rasterizerInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterizerInfo.lineWidth = 1.0f;

	// This part specifies the multisampling stage for ImGui
	VkPipelineMultisampleStateCreateInfo multisampleInfo =
		VkUtils::Init::PipelineMultisampleStateCreateInfo();
	multisampleInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	// This part specifies the blending for ImGui
	VkPipelineColorBlendAttachmentState colorBlendAttachment =
		VkUtils::Init::PipelineColorBlendAttachmentState(
			VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
			VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
			VK_TRUE
		);
	colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
	colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

	VkPipelineColorBlendStateCreateInfo colorBlendInfo = 
		VkUtils::Init::PipelineColorBlendStateCreateInfo(1, &colorBlendAttachment);

	// This part specifies the depth and stencil testing
	VkPipelineDepthStencilStateCreateInfo depthStencilInfo =
		VkUtils::Init::PipelineDepthStencilStateCreateInfo(VK_FALSE, VK_FALSE, VK_COMPARE_OP_LESS_OR_EQUAL);

	// This part specifies the dynamic states of the pipeline,
	// these are the structs that can be modified with recreating the pipeline.
	VkPipelineDynamicStateCreateInfo dynamicStateInfo = VkUtils::Init::PipelineDynamicStateCreateInfo();
	VkDynamicState dynamicStates[] = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR
	};
	dynamicStateInfo.dynamicStateCount = 2;
	dynamicStateInfo.pDynamicStates = dynamicStates;

	VkGraphicsPipelineCreateInfo pipelineInfo = VkUtils::Init::GraphicsPipelineCreateInfo();
	pipelineInfo.stageCount = 2;
	pipelineInfo.pStages = shaderStageInfo;
	pipelineInfo.pVertexInputState = &vertexInputInfo;
	pipelineInfo.pInputAssemblyState = &inputAssemblyInfo;
	pipelineInfo.pViewportState = &viewportInfo;
	pipelineInfo.pRasterizationState = &rasterizerInfo;
	pipelineInfo.pMultisampleState = &multisampleInfo;
	pipelineInfo.pDepthStencilState = &depthStencilInfo;
	pipelineInfo.pColorBlendState = &colorBlendInfo;
	pipelineInfo.pDynamicState = &dynamicStateInfo;
	pipelineInfo.layout = pipelineLayout_;
	pipelineInfo.renderPass = context_->mainRenderPass;

	VK_CHECK(vkCreateGraphicsPipelines(context_->device, pipelineCache_, 1, &pipelineInfo, nullptr, &pipeline_));

	vkDestroyShaderModule(context_->device, vertShaderModule, nullptr);
	vkDestroyShaderModule(context_->device, fragShaderModule, nullptr);
}