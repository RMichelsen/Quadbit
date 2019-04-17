#include <PCH.h>
#include "MeshPipeline.h"

#include "../../Application/Time.h"
#include "../Camera.h"
#include "../Common/QbVkUtils.h"

namespace Quadbit {
	MeshPipeline::MeshPipeline(std::shared_ptr<QbVkContext> context) {
		this->context_ = context;

		//CreateDescriptorSetLayout();
		CreatePipeline();

		CreateVertexBuffer();
		CreateIndexBuffer();
		//CreateUniformBuffers();
		//CreateDescriptorPool();
		//CreateDescriptorSets();
	}

	MeshPipeline::~MeshPipeline() {
		// Since both the pipeline and layout are destroyed by the swapchain we should check before deleting
		if(pipeline_ != VK_NULL_HANDLE) {
			vkDestroyPipeline(context_->device, pipeline_, nullptr);
		}
		if(pipelineLayout_ != VK_NULL_HANDLE) {
			vkDestroyPipelineLayout(context_->device, pipelineLayout_, nullptr);
		}

		vkDestroyDescriptorPool(context_->device, descriptorPool_, nullptr);
		vkDestroyDescriptorSetLayout(context_->device, descriptorSetLayout_, nullptr);

		//VkUtils::DestroyBuffer(context_, vertexbuffer_);
		context_->allocator->DestroyBuffer(vertexBuffer_);
		context_->allocator->DestroyBuffer(indexBuffer_);
	}

	void MeshPipeline::RebuildPipeline() {
		// Destroy old pipeline
		vkDestroyPipelineLayout(context_->device, pipelineLayout_, nullptr);
		vkDestroyPipeline(context_->device, pipeline_, nullptr);
		// Create new pipeline
		CreatePipeline();
	}

	void MeshPipeline::DrawFrame(VkCommandBuffer commandbuffer) {

		//VkViewport viewport = VkUtils::Init::Viewport(
		//	static_cast<float>(mContext->swapchain.extent.height),
		//	static_cast<float>(mContext->swapchain.extent.width),
		//	0.0f, 1.0f);
		//vkCmdSetViewport(commandbuffer, 0, 1, &viewport);

		//VkRect2D scissorRect = VkUtils::Init::ScissorRect(0, 0, mContext->swapchain.extent.height, mContext->swapchain.extent.width);
		//vkCmdSetScissor(commandbuffer, 0, 1, &scissorRect);

		static glm::mat4 rotation = glm::mat4(1.0f);

		rotation = glm::rotate(rotation, glm::radians(20.0f) * Time::deltaTime, glm::vec3(0, 1, 0));

		glm::mat4 perspective = Camera::GetPerspectiveMatrix();
		perspective[1][1] *= -1;
		glm::mat4 view = Camera::GetViewMatrix();
		pushConstants_.mvp = perspective * view * rotation;

		vkCmdBindPipeline(commandbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
		vkCmdPushConstants(commandbuffer, pipelineLayout_, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(pushConstants_), &pushConstants_);

		VkDeviceSize offsets[]{ 0 };
		vkCmdBindVertexBuffers(commandbuffer, 0, 1, &vertexBuffer_.buf, offsets);
		vkCmdBindIndexBuffer(commandbuffer, indexBuffer_.buf, 0, VK_INDEX_TYPE_UINT32);

		vkCmdDrawIndexed(commandbuffer, static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);

		// FOR UBO
		//vkCmdBindDescriptorSets(context->commandBuffers[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, 
		//	pipelineLayout, 0, 1, &descriptorSets[currentFrame], 0, nullptr);
	}

	void MeshPipeline::UpdateUniformBuffers(uint32_t currentImage) {
		pushConstants_.mvp[0][0] += 0.1f;
		return;
		static auto start = std::chrono::high_resolution_clock::now();
		auto current = std::chrono::high_resolution_clock::now();
		float delta = std::chrono::duration<float, std::chrono::seconds::period>(current - start).count();

		UniformBufferObject ubo{};
		ubo.model = glm::rotate(glm::mat4(1.0f), delta * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
		ubo.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
		ubo.proj = glm::perspective(glm::radians(45.0f), context_->swapchain.extent.width / (float)context_->swapchain.extent.height, 0.1f, 10.0f);
		ubo.proj[1][1] *= -1;

		void* data;
		VK_CHECK(vkMapMemory(context_->device, uniformBuffersMemory_[currentImage], 0, sizeof(ubo), 0, &data));
		memcpy(data, &ubo, sizeof(ubo));
		vkUnmapMemory(context_->device, uniformBuffersMemory_[currentImage]);
	}

	void MeshPipeline::CreateDescriptorPool() {
		VkDescriptorPoolSize poolSize{};
		poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		poolSize.descriptorCount = static_cast<uint32_t>(context_->swapchain.images.size());

		VkDescriptorPoolCreateInfo poolInfo{};
		poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.poolSizeCount = 1;
		poolInfo.pPoolSizes = &poolSize;
		poolInfo.maxSets = static_cast<uint32_t>(context_->swapchain.images.size());

		VK_CHECK(vkCreateDescriptorPool(context_->device, &poolInfo, nullptr, &descriptorPool_));
	}

	void MeshPipeline::CreateDescriptorSetLayout() {
		VkDescriptorSetLayoutBinding uboLayoutBinding{};
		uboLayoutBinding.binding = 0;
		uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		uboLayoutBinding.descriptorCount = 1;
		uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
		uboLayoutBinding.pImmutableSamplers = nullptr;

		VkDescriptorSetLayoutCreateInfo layoutInfo = VkUtils::Init::DescriptorSetLayoutCreateInfo();
		layoutInfo.bindingCount = 1;
		layoutInfo.pBindings = &uboLayoutBinding;

		VK_CHECK(vkCreateDescriptorSetLayout(context_->device, &layoutInfo, nullptr, &descriptorSetLayout_));
	}

	void MeshPipeline::CreateDescriptorSets() {
		std::vector<VkDescriptorSetLayout> layouts(context_->swapchain.images.size(), descriptorSetLayout_);
		VkDescriptorSetAllocateInfo descSetallocInfo = VkUtils::Init::DescriptorSetAllocateInfo();
		descSetallocInfo.descriptorPool = descriptorPool_;
		descSetallocInfo.descriptorSetCount = static_cast<uint32_t>(context_->swapchain.images.size());
		descSetallocInfo.pSetLayouts = layouts.data();

		VK_CHECK(vkAllocateDescriptorSets(context_->device, &descSetallocInfo, descriptorSets_.data()));

		for(auto i = 0; i < context_->swapchain.images.size(); i++) {
			VkDescriptorBufferInfo bufferInfo{};
			bufferInfo.buffer = uniformBuffers_[i];
			bufferInfo.offset = 0;
			bufferInfo.range = sizeof(UniformBufferObject);

			VkWriteDescriptorSet writeDesc = VkUtils::Init::WriteDescriptorSet();
			writeDesc.dstSet = descriptorSets_[i];
			writeDesc.dstBinding = 0;
			writeDesc.dstArrayElement = 0;
			writeDesc.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			writeDesc.descriptorCount = 1;
			writeDesc.pBufferInfo = &bufferInfo;
			writeDesc.pImageInfo = nullptr;
			writeDesc.pTexelBufferView = nullptr;

			vkUpdateDescriptorSets(context_->device, 1, &writeDesc, 0, nullptr);
		}
	}

	void MeshPipeline::CreatePipeline() {
		// We start off by reading the shader bytecode from disk and creating the modules subsequently
		std::vector<char> vertexShaderBytecode = VkUtils::ReadShader("Resources/Shaders/Compiled/mesh_vert.spv");
		std::vector<char> fragmentShaderBytecode = VkUtils::ReadShader("Resources/Shaders/Compiled/mesh_frag.spv");
		VkShaderModule vertShaderModule = VkUtils::CreateShaderModule(vertexShaderBytecode, context_->device);
		VkShaderModule fragShaderModule = VkUtils::CreateShaderModule(fragmentShaderBytecode, context_->device);

		// This part specifies the two shader types used in the pipeline
		VkPipelineShaderStageCreateInfo shaderStageInfo[2] = {
			VkUtils::Init::PipelineShaderStageCreateInfo(),
			VkUtils::Init::PipelineShaderStageCreateInfo()
		};
		// Specifies type of shader
		shaderStageInfo[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
		// Specifies shader module to load
		shaderStageInfo[0].module = vertShaderModule;
		// "main" refers to the Entrypoint of a given shader module
		shaderStageInfo[0].pName = "main";

		// Similarly for the fragment shader
		shaderStageInfo[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		shaderStageInfo[1].module = fragShaderModule;
		shaderStageInfo[1].pName = "main";

		// This part specifies the format of the vertex data passed to the vertex shader
		auto bindingDescription = Vertex::getBindingDescription();
		auto attributeDescriptions = Vertex::getAttributeDescriptions();
		VkPipelineVertexInputStateCreateInfo vertexInputInfo =
			VkUtils::Init::PipelineVertexInputStateCreateInfo();
		vertexInputInfo.vertexBindingDescriptionCount = 1;
		vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
		vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
		vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

		// This part specifies the geometry drawn
		VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo = VkUtils::Init::PipelineInputAssemblyStateCreateInfo();
		inputAssemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

		// This setting makes it possible to break up lines and triangles when using _STRIP topology modes
		inputAssemblyInfo.primitiveRestartEnable = VK_FALSE;

		// This part creates the viewport and scissor and combines them in a struct holding the data for creation
		VkViewport viewport{};
		VkRect2D scissor{};
		// In our case the viewport covers the entire window
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = static_cast<float>(context_->swapchain.extent.width);
		viewport.height = static_cast<float>(context_->swapchain.extent.height);
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;

		// And so does the scissor
		scissor.offset = { 0, 0 };
		scissor.extent = context_->swapchain.extent;

		// Onto the create info struct as usual
		VkPipelineViewportStateCreateInfo viewportInfo = VkUtils::Init::PipelineViewportStateCreateInfo();
		viewportInfo.viewportCount = 1;
		viewportInfo.pViewports = &viewport;
		viewportInfo.scissorCount = 1;
		viewportInfo.pScissors = &scissor;

		// This part specifies the rasterization stage
		VkPipelineRasterizationStateCreateInfo rasterizerInfo =
			VkUtils::Init::PipelineRasterizationStateCreateInfo();

		// Requires a GPU feature to be enabled, useful for special cases like shadow maps
		rasterizerInfo.depthClampEnable = VK_FALSE;

		// We will allow geometry to pass through the rasterizer
		rasterizerInfo.rasterizerDiscardEnable = VK_FALSE;

		// Specifies how fragments are generated,
		// using any other option than FILL requires an appropriate GPU feature to be enabled
		// VK_POLYGON_MODE_FILL: fills the areas of the polygon with fragments
		// VK_POLYGON_MODE_LINE: draws the polygon edges as lines
		// VK_POLYGON_MODE_POINT: draws the polygon vertices as points
		rasterizerInfo.polygonMode = VK_POLYGON_MODE_FILL;

		// Thickness of lines (in terms of fragments)
		rasterizerInfo.lineWidth = 1.0f;

		// Culling options and direction
		rasterizerInfo.cullMode = VK_CULL_MODE_BACK_BIT;
		rasterizerInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

		// We won't be doing any altering of depth values in the rasterizer
		rasterizerInfo.depthBiasEnable = VK_FALSE;

		// This part specifies the multisampling stage
		VkPipelineMultisampleStateCreateInfo multisampleInfo =
			VkUtils::Init::PipelineMultisampleStateCreateInfo();
		// For now everything is default or disabled
		multisampleInfo.sampleShadingEnable = VK_FALSE;
		multisampleInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
		multisampleInfo.minSampleShading = 1.0f;
		multisampleInfo.pSampleMask = nullptr;
		multisampleInfo.alphaToCoverageEnable = VK_FALSE;
		multisampleInfo.alphaToOneEnable = VK_FALSE;

		// This part specifies the color blending 
		// We won't be blending colours for now
		VkPipelineColorBlendAttachmentState colorBlendAttachment =
			VkUtils::Init::PipelineColorBlendAttachmentState(
				VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
				VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
				VK_FALSE
			);

		VkPipelineColorBlendStateCreateInfo colorBlendInfo =
			VkUtils::Init::PipelineColorBlendStateCreateInfo(
				1, &colorBlendAttachment);

		// This part specifies the depth and stencil testing
		VkPipelineDepthStencilStateCreateInfo depthStencilInfo =
			VkUtils::Init::PipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS);
		depthStencilInfo.depthBoundsTestEnable = VK_FALSE;
		depthStencilInfo.minDepthBounds = 0.0f;
		depthStencilInfo.maxDepthBounds = 1.0f;
		depthStencilInfo.stencilTestEnable = VK_FALSE;
		depthStencilInfo.front = {};
		depthStencilInfo.back = {};

		// This part specifies the dynamic states of the pipeline,
		// these are the structs that can be modified with recreating the pipeline.
		VkPipelineDynamicStateCreateInfo dynamicStateInfo = VkUtils::Init::PipelineDynamicStateCreateInfo();
		VkDynamicState dynamicStates[] = {
			VK_DYNAMIC_STATE_VIEWPORT,
			VK_DYNAMIC_STATE_LINE_WIDTH
		};
		dynamicStateInfo.dynamicStateCount = 2;
		dynamicStateInfo.pDynamicStates = dynamicStates;

		// This part specifies the pipeline layout, its mainly used to specify the layout of uniform values used in shaders
		VkPipelineLayoutCreateInfo pipelineLayoutInfo = VkUtils::Init::PipelineLayoutCreateInfo();
		// DISABLE UBO:
		//pipelineLayoutInfo.setLayoutCount = 1;
		//pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;
		pipelineLayoutInfo.setLayoutCount = 0;
		pipelineLayoutInfo.pSetLayouts = nullptr;
		// ENABLE PUSHCONSTANTS
		VkPushConstantRange pushConstantRange{};
		pushConstantRange.offset = 0;
		pushConstantRange.size = sizeof(PushConstants);
		pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
		// Push constant ranges are part of the pipeline layout
		pipelineLayoutInfo.pushConstantRangeCount = 1;
		pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

		VK_CHECK(vkCreatePipelineLayout(context_->device, &pipelineLayoutInfo, nullptr, &pipelineLayout_));

		// It's finally time to create the actual pipeline
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
		// IF USING DYNAMIC STATE REMEMBER TO CALL vkCmdSetViewport
		// pipelineInfo.pDynamicState = &dynamicStateInfo;
		pipelineInfo.pDynamicState = nullptr;
		pipelineInfo.layout = pipelineLayout_;
		pipelineInfo.renderPass = context_->mainRenderPass;

		// It's possible to make derivatives of pipelines, 
		// we won't be doing that but these are the variables that need to be set
		pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
		pipelineInfo.basePipelineIndex = -1;

		// Finally we can create the pipeline
		VK_CHECK(vkCreateGraphicsPipelines(context_->device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline_));

		vkDestroyShaderModule(context_->device, vertShaderModule, nullptr);
		vkDestroyShaderModule(context_->device, fragShaderModule, nullptr);
	}

	void MeshPipeline::CreateVertexBuffer() {
		VkDeviceSize bufferSize = static_cast<uint32_t>(vertices.size()) * sizeof(Vertex);

		QbVkBuffer stagingBuffer;
		context_->allocator->CreateStagingBuffer(stagingBuffer, bufferSize, vertices.data());

		VkBufferCreateInfo bufferInfo = VkUtils::Init::BufferCreateInfo(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
		context_->allocator->CreateBuffer(vertexBuffer_, bufferInfo, QBVK_MEMORY_USAGE_GPU_ONLY);

		VkUtils::CopyBuffer(context_, stagingBuffer.buf, vertexBuffer_.buf, bufferSize);

		context_->allocator->DestroyBuffer(stagingBuffer);
	}

	void MeshPipeline::CreateIndexBuffer() {
		VkDeviceSize bufferSize = static_cast<uint32_t>(indices.size()) * sizeof(uint32_t);

		QbVkBuffer stagingBuffer;
		context_->allocator->CreateStagingBuffer(stagingBuffer, bufferSize, indices.data());

		VkBufferCreateInfo bufferInfo = VkUtils::Init::BufferCreateInfo(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
		context_->allocator->CreateBuffer(indexBuffer_, bufferInfo, QBVK_MEMORY_USAGE_GPU_ONLY);

		VkUtils::CopyBuffer(context_, stagingBuffer.buf, indexBuffer_.buf, bufferSize);

		context_->allocator->DestroyBuffer(stagingBuffer);
	}

	void MeshPipeline::CreateUniformBuffers() {
		VkDeviceSize bufferSize = sizeof(UniformBufferObject);

		for(auto i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
			//VkUtils::CreateBuffer(context_, bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
			//	VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, uniformBuffers_[i], uniformBuffersMemory_[i]);
		}
	}
}