#pragma once
#include <EASTL/algorithm.h>
#include <EASTL/numeric.h>
#include <EASTL/unique_ptr.h>
#include <EASTL/vector.h>

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <glm/gtx/compatibility.hpp>

#include "Engine/Core/Logging.h"
#include "Engine/Rendering/Memory/Allocator.h"
#include "Engine/Rendering/Memory/ResourceManager.h"
#include "Engine/Rendering/VulkanTypes.h"

namespace Quadbit::VkUtils {
	// Wrappers around various Vulkan structs
	namespace Init {
		inline VkCommandBufferAllocateInfo CommandBufferAllocateInfo(VkCommandPool commandPool, VkCommandBufferLevel commandBufferLevel, uint32_t bufferCount) {
			VkCommandBufferAllocateInfo commandBufferAllocateInfo{};
			commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
			commandBufferAllocateInfo.level = commandBufferLevel;
			commandBufferAllocateInfo.commandPool = commandPool;
			commandBufferAllocateInfo.commandBufferCount = bufferCount;
			return commandBufferAllocateInfo;
		}

		inline VkRenderPassBeginInfo RenderPassBeginInfo() {
			VkRenderPassBeginInfo renderPassInfo{};
			renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			return renderPassInfo;
		}

		inline VkCommandBufferBeginInfo CommandBufferBeginInfo() {
			VkCommandBufferBeginInfo bufferBeginInfo{};
			bufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			return bufferBeginInfo;
		}

		inline VkBufferCreateInfo BufferCreateInfo(VkDeviceSize size, VkBufferUsageFlags usage) {
			VkBufferCreateInfo bufferInfo{};
			bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			bufferInfo.size = size;
			bufferInfo.usage = usage;
			return bufferInfo;
		}

		inline VkMemoryAllocateInfo MemoryAllocateInfo() {
			VkMemoryAllocateInfo allocInfo{};
			allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
			return allocInfo;
		}

		inline VkImageMemoryBarrier ImageMemoryBarrier() {
			VkImageMemoryBarrier imageMemoryBarrier{};
			imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			return imageMemoryBarrier;
		}

		inline VkImageViewCreateInfo ImageViewCreateInfo() {
			VkImageViewCreateInfo imageViewCreateInfo{};
			imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			return imageViewCreateInfo;
		}

		inline VkImageCreateInfo ImageCreateInfo(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage,
			VkSampleCountFlagBits numSamples = VK_SAMPLE_COUNT_1_BIT, uint32_t mipLevels = 1, uint32_t arrayLayers = 1, VkImageCreateFlags flags = 0) {
			VkImageCreateInfo imageCreateInfo{};
			imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
			imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
			imageCreateInfo.extent.width = width;
			imageCreateInfo.extent.height = height;
			imageCreateInfo.extent.depth = 1;
			imageCreateInfo.mipLevels = mipLevels;
			imageCreateInfo.arrayLayers = arrayLayers;
			imageCreateInfo.format = format;
			imageCreateInfo.tiling = tiling;
			imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			imageCreateInfo.usage = usage;
			imageCreateInfo.samples = numSamples;
			imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			imageCreateInfo.flags = flags;
			return imageCreateInfo;
		}

		inline VkCommandPoolCreateInfo CommandPoolCreateInfo() {
			VkCommandPoolCreateInfo commandPoolCreateInfo{};
			commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
			return commandPoolCreateInfo;
		}

		inline VkGraphicsPipelineCreateInfo GraphicsPipelineCreateInfo() {
			VkGraphicsPipelineCreateInfo graphicsPipelineCreateInfo{};
			graphicsPipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
			return graphicsPipelineCreateInfo;
		}

		inline VkComputePipelineCreateInfo ComputePipelineCreateInfo() {
			VkComputePipelineCreateInfo computePipelineCreateInfo{};
			computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
			return computePipelineCreateInfo;
		}

		inline VkPipelineLayoutCreateInfo PipelineLayoutCreateInfo() {
			VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
			pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
			return pipelineLayoutCreateInfo;
		}

		inline VkPipelineDynamicStateCreateInfo PipelineDynamicStateCreateInfo() {
			VkPipelineDynamicStateCreateInfo pipelineDynamicStateCreateInfo{};
			pipelineDynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
			return pipelineDynamicStateCreateInfo;
		}

		inline VkPipelineDepthStencilStateCreateInfo PipelineDepthStencilStateCreateInfo(VkBool32 depthTestEnable,
			VkBool32 depthWriteEnable, VkCompareOp depthCompareOp) {
			VkPipelineDepthStencilStateCreateInfo depthStencilCreateInfo{};
			depthStencilCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
			depthStencilCreateInfo.depthTestEnable = depthTestEnable;
			depthStencilCreateInfo.depthWriteEnable = depthWriteEnable;
			depthStencilCreateInfo.depthCompareOp = depthCompareOp;
			return depthStencilCreateInfo;
		}

		inline VkPipelineColorBlendAttachmentState PipelineColorBlendAttachmentState(VkColorComponentFlags colourWriteMask,
			VkBool32 blendEnable) {
			VkPipelineColorBlendAttachmentState colourBlendAttachment{};
			colourBlendAttachment.colorWriteMask = colourWriteMask;
			colourBlendAttachment.blendEnable = blendEnable;
			return colourBlendAttachment;
		}

		inline VkPipelineColorBlendStateCreateInfo PipelineColorBlendStateCreateInfo(uint32_t count,
			const VkPipelineColorBlendAttachmentState* pAttachments) {
			VkPipelineColorBlendStateCreateInfo colorBlendInfo{};
			colorBlendInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
			colorBlendInfo.attachmentCount = count;
			colorBlendInfo.pAttachments = pAttachments;
			return colorBlendInfo;
		}

		inline VkPipelineMultisampleStateCreateInfo PipelineMultisampleStateCreateInfo() {
			VkPipelineMultisampleStateCreateInfo multisampleCreateInfo{};
			multisampleCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
			return multisampleCreateInfo;
		}

		inline VkPipelineRasterizationStateCreateInfo PipelineRasterizationStateCreateInfo() {
			VkPipelineRasterizationStateCreateInfo rasterizerCreateInfo{};
			rasterizerCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
			return rasterizerCreateInfo;
		}

		inline VkPipelineViewportStateCreateInfo PipelineViewportStateCreateInfo() {
			VkPipelineViewportStateCreateInfo viewportCreateInfo{};
			viewportCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
			return viewportCreateInfo;
		}

		inline VkPipelineInputAssemblyStateCreateInfo PipelineInputAssemblyStateCreateInfo() {
			VkPipelineInputAssemblyStateCreateInfo inputAssemblyCreateInfo{};
			inputAssemblyCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
			return inputAssemblyCreateInfo;
		}

		inline VkPipelineInputAssemblyStateCreateInfo PipelineInputAssemblyStateCreateInfo(
			VkPrimitiveTopology topology, VkBool32 primitiveRestart = VK_FALSE) {
			VkPipelineInputAssemblyStateCreateInfo inputAssemblyCreateInfo{};
			inputAssemblyCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
			inputAssemblyCreateInfo.topology = topology;
			inputAssemblyCreateInfo.primitiveRestartEnable = primitiveRestart;
			return inputAssemblyCreateInfo;
		}

		inline VkPipelineVertexInputStateCreateInfo PipelineVertexInputStateCreateInfo() {
			VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
			vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
			return vertexInputInfo;
		}

		inline VkPipelineVertexInputStateCreateInfo PipelineVertexInputStateCreateInfo(
			const eastl::vector<VkVertexInputAttributeDescription>& attributeDescriptions,
			const VkVertexInputBindingDescription& bindingDescription) {
			VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
			vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
			vertexInputInfo.vertexBindingDescriptionCount = 1;
			vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
			vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
			vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();
			return vertexInputInfo;
		}

		inline VkPipelineShaderStageCreateInfo PipelineShaderStageCreateInfo() {
			VkPipelineShaderStageCreateInfo shaderCreateInfo{};
			shaderCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			return shaderCreateInfo;
		}

		inline VkVertexInputBindingDescription VertexInputBindingDescription(uint32_t binding, uint32_t stride, VkVertexInputRate inputRate) {
			VkVertexInputBindingDescription vertexInputBindingDesc{};
			vertexInputBindingDesc.binding = binding;
			vertexInputBindingDesc.stride = stride;
			vertexInputBindingDesc.inputRate = inputRate;
			return vertexInputBindingDesc;
		}

		inline VkVertexInputAttributeDescription VertexInputAttributeDescription(uint32_t binding, uint32_t index, VkFormat format, uint32_t offset) {
			VkVertexInputAttributeDescription vertexInputAttributeDesc{};
			vertexInputAttributeDesc.binding = binding;
			vertexInputAttributeDesc.location = index;
			vertexInputAttributeDesc.format = format;
			vertexInputAttributeDesc.offset = offset;
			return vertexInputAttributeDesc;
		}

		inline VkRenderPassCreateInfo RenderPassCreateInfo() {
			VkRenderPassCreateInfo renderPassCreateInfo{};
			renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
			return renderPassCreateInfo;
		}

		inline VkViewport Viewport(float height, float width, float minDepth, float maxDepth) {
			VkViewport viewport{};
			viewport.height = height;
			viewport.width = width;
			viewport.minDepth = minDepth;
			viewport.maxDepth = maxDepth;
			return viewport;
		}

		inline VkRect2D ScissorRect(int32_t offsetX, int32_t offsetY, uint32_t height, uint32_t width) {
			return VkRect2D{
				{offsetX, offsetY},
			    {height, width}
			};
		}

		inline VkFramebufferCreateInfo FramebufferCreateInfo() {
			VkFramebufferCreateInfo framebufferCreateInfo{};
			framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			return framebufferCreateInfo;
		}

		inline VkSwapchainCreateInfoKHR SwapchainCreateInfoKHR() {
			VkSwapchainCreateInfoKHR swapchainCreateInfo{};
			swapchainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
			return swapchainCreateInfo;
		}

		inline VkDeviceCreateInfo DeviceCreateInfo() {
			VkDeviceCreateInfo deviceCreateInfo{};
			deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
			return deviceCreateInfo;
		}

		inline VkInstanceCreateInfo InstanceCreateInfo() {
			VkInstanceCreateInfo instanceCreateInfo{};
			instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
			return instanceCreateInfo;
		}

		inline VkApplicationInfo ApplicationInfo() {
			VkApplicationInfo appInfo{};
			appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
			return appInfo;
		}

		inline VkWin32SurfaceCreateInfoKHR Win32SurfaceCreateInfoKHR() {
			VkWin32SurfaceCreateInfoKHR surfaceCreateInfo{};
			surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
			return surfaceCreateInfo;
		}

		inline VkDeviceQueueCreateInfo DeviceQueueCreateInfo() {
			VkDeviceQueueCreateInfo deviceQueueCreateInfo{};
			deviceQueueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			return deviceQueueCreateInfo;
		}

		inline VkSemaphoreCreateInfo SemaphoreCreateInfo() {
			VkSemaphoreCreateInfo sempahoreCreateInfo{};
			sempahoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
			return sempahoreCreateInfo;
		}

		inline VkFenceCreateInfo FenceCreateInfo() {
			VkFenceCreateInfo fenceCreateInfo{};
			fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
			return fenceCreateInfo;
		}

		inline VkSubmitInfo SubmitInfo() {
			VkSubmitInfo submitInfo{};
			submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
			return submitInfo;
		}

		inline VkDescriptorSetLayoutBinding DescriptorSetLayoutBinding(uint32_t index, VkDescriptorType descType, VkShaderStageFlags shaderFlags, uint32_t descCount = 1) {
			VkDescriptorSetLayoutBinding descSetLayoutBinding{};
			descSetLayoutBinding.binding = index;
			descSetLayoutBinding.descriptorType = descType;
			descSetLayoutBinding.descriptorCount = descCount;
			descSetLayoutBinding.stageFlags = shaderFlags;
			descSetLayoutBinding.pImmutableSamplers = nullptr;
			return descSetLayoutBinding;
		}

		inline VkDescriptorPoolSize DescriptorPoolSize(VkDescriptorType descriptorType, uint32_t count) {
			VkDescriptorPoolSize descriptorPoolSize{};
			descriptorPoolSize.type = descriptorType;
			descriptorPoolSize.descriptorCount = count;
			return descriptorPoolSize;
		}

		inline VkDescriptorSetLayoutCreateInfo DescriptorSetLayoutCreateInfo() {
			VkDescriptorSetLayoutCreateInfo descSetLayoutCreateInfo{};
			descSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
			return descSetLayoutCreateInfo;
		}

		inline VkDescriptorSetLayoutCreateInfo DescriptorSetLayoutCreateInfo(const eastl::vector<VkDescriptorSetLayoutBinding>& setLayoutBindings) {
			VkDescriptorSetLayoutCreateInfo descSetLayoutCreateInfo{};
			descSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
			descSetLayoutCreateInfo.bindingCount = static_cast<uint32_t>(setLayoutBindings.size());
			descSetLayoutCreateInfo.pBindings = setLayoutBindings.data();
			return descSetLayoutCreateInfo;
		}

		inline VkDescriptorSetAllocateInfo DescriptorSetAllocateInfo() {
			VkDescriptorSetAllocateInfo descAllocInfo{};
			descAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
			return descAllocInfo;
		}

		inline VkWriteDescriptorSet WriteDescriptorSet(VkDescriptorSet dstSet, uint32_t dstBinding, VkDescriptorType descType, VkDescriptorBufferInfo* bufferInfo, uint32_t descCount = 1) {
			VkWriteDescriptorSet writeDescriptorSet{};
			writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writeDescriptorSet.dstSet = dstSet;
			writeDescriptorSet.dstBinding = dstBinding;
			writeDescriptorSet.descriptorType = descType;
			writeDescriptorSet.pBufferInfo = bufferInfo;
			writeDescriptorSet.descriptorCount = descCount;
			return writeDescriptorSet;
		}

		inline VkWriteDescriptorSet WriteDescriptorSet(VkDescriptorSet dstSet, uint32_t dstBinding, VkDescriptorType descType, VkDescriptorImageInfo* imageInfo, uint32_t descCount = 1) {
			VkWriteDescriptorSet writeDescriptorSet{};
			writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writeDescriptorSet.dstSet = dstSet;
			writeDescriptorSet.descriptorType = descType;
			writeDescriptorSet.dstBinding = dstBinding;
			writeDescriptorSet.pImageInfo = imageInfo;
			writeDescriptorSet.descriptorCount = descCount;
			return writeDescriptorSet;
		}

		inline VkPresentInfoKHR PresentInfoKHR() {
			VkPresentInfoKHR presentInfo{};
			presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
			return presentInfo;
		}

		inline VkSamplerCreateInfo SamplerCreateInfo() {
			VkSamplerCreateInfo samplerCreateInfo{};
			samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
			return samplerCreateInfo;
		}

		inline VkPipelineCacheCreateInfo PipelineCacheCreateInfo() {
			VkPipelineCacheCreateInfo pipelineCacheCreateInfo{};
			pipelineCacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
			return pipelineCacheCreateInfo;
		}

		inline VkMappedMemoryRange MappedMemoryRange() {
			VkMappedMemoryRange mappedRange{};
			mappedRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
			return mappedRange;
		}

		inline VkSamplerCreateInfo SamplerCreateInfo(VkFilter samplerFilter, VkSamplerAddressMode addressMode, VkBool32 enableAnisotropy,
			float maxAnisotropy, VkCompareOp compareOperation, VkSamplerMipmapMode samplerMipmapMode, float maxLod = 0.0f) {

			VkSamplerCreateInfo samplerCreateInfo{};
			samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
			samplerCreateInfo.magFilter = samplerFilter;
			samplerCreateInfo.minFilter = samplerFilter;
			samplerCreateInfo.addressModeU = addressMode;
			samplerCreateInfo.addressModeV = addressMode;
			samplerCreateInfo.addressModeW = addressMode;
			samplerCreateInfo.anisotropyEnable = enableAnisotropy;
			samplerCreateInfo.maxAnisotropy = maxAnisotropy;
			samplerCreateInfo.unnormalizedCoordinates = VK_FALSE;
			samplerCreateInfo.compareEnable = VK_FALSE;
			samplerCreateInfo.compareOp = compareOperation;
			samplerCreateInfo.mipmapMode = samplerMipmapMode;
			samplerCreateInfo.maxLod = maxLod;
			return samplerCreateInfo;
		}

		inline VkMemoryBarrier MemoryBarrierVk(VkAccessFlags srcMask, VkAccessFlags dstMask) {
			VkMemoryBarrier memoryBarrier{};
			memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
			memoryBarrier.srcAccessMask = srcMask;
			memoryBarrier.dstAccessMask = dstMask;
			return memoryBarrier;
		}

		inline VkDescriptorPoolCreateInfo DescriptorPoolCreateInfo(const eastl::vector<VkDescriptorPoolSize>& poolSizes, uint32_t maxSets) {
			VkDescriptorPoolCreateInfo descriptorPoolInfo{};
			descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
			descriptorPoolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
			descriptorPoolInfo.pPoolSizes = poolSizes.data();
			descriptorPoolInfo.maxSets = maxSets;
			return descriptorPoolInfo;
		}
	}

	inline bool IsSuitableGPUOfType(const QbVkContext& context, GPU& gpu, VkPhysicalDevice physicalDevice, VkPhysicalDeviceType GPUType) {
		vkGetPhysicalDeviceProperties(physicalDevice, &gpu.deviceProps);

		if (gpu.deviceProps.deviceType != GPUType) return false;

		// Get device queues
		uint32_t queueCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueCount, nullptr);
		if (queueCount == 0) return false;

		gpu.queueProps.resize(queueCount);
		vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueCount, gpu.queueProps.data());

		// Get device extensions
		uint32_t extensionCount = 0;
		VK_CHECK(vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, nullptr));
		if (extensionCount == 0) return false;

		gpu.extensionProps.resize(extensionCount);
		VK_CHECK(vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, gpu.extensionProps.data()));

		// Get surface capabilities
		VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, context.surface, &gpu.surfaceCapabilities));

		// Get supported surface formats
		uint32_t formatCount = 0;
		VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, context.surface, &formatCount, nullptr));
		if (formatCount == 0) return false;

		gpu.surfaceFormats.resize(formatCount);
		VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, context.surface, &formatCount, gpu.surfaceFormats.data()));

		// Get supported present modes
		uint32_t presentModeCount = 0;
		VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, context.surface, &presentModeCount, nullptr));
		if (presentModeCount == 0) return false;

		gpu.presentModes.resize(presentModeCount);
		VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, context.surface, &presentModeCount, gpu.presentModes.data()));

		// Get device supported memory types
		vkGetPhysicalDeviceMemoryProperties(physicalDevice, &gpu.memoryProps);

		// Get core device properties
		vkGetPhysicalDeviceProperties(physicalDevice, &gpu.deviceProps);

		// Get device features
		vkGetPhysicalDeviceFeatures(physicalDevice, &gpu.features);

		// Lastly we'll make sure that the appropriate queues exist on the GPU
		for (auto i = 0; i < gpu.queueProps.size(); i++) {
			VkQueueFamilyProperties props = gpu.queueProps[i];

			if (props.queueCount == 0) continue;

			if (props.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
				gpu.graphicsFamilyIdx = i;
			}
			if (props.queueFlags & VK_QUEUE_COMPUTE_BIT) {
				gpu.computeFamilyIdx = i;
			}
		}

		VkBool32 supportsPresent = VK_FALSE;
		VK_CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, gpu.graphicsFamilyIdx, context.surface, &supportsPresent));
		if (supportsPresent) {
			gpu.presentFamilyIdx = gpu.graphicsFamilyIdx;
		}
		else {
			for (auto i = 0; i < gpu.queueProps.size(); i++) {
				VkQueueFamilyProperties props = gpu.queueProps[i];

				if (props.queueCount == 0) continue;

				VK_CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, context.surface, &supportsPresent));
				if (supportsPresent) {
					gpu.presentFamilyIdx = i;
				}
			}
		}

		if (gpu.graphicsFamilyIdx < 0 || gpu.presentFamilyIdx < 0 || gpu.computeFamilyIdx < 0) return false;

		gpu.physicalDevice = physicalDevice;
		return true;
	}

	inline VkSampleCountFlagBits GetMaxSampleCount(const VkPhysicalDeviceProperties& deviceProperties) {
		VkSampleCountFlags commonCount = eastl::min(deviceProperties.limits.framebufferColorSampleCounts,
			deviceProperties.limits.framebufferDepthSampleCounts);

		// Pick the maximum usable sample count (the minimum of colour and depth max sample counts)
		if (commonCount & VK_SAMPLE_COUNT_64_BIT) return VK_SAMPLE_COUNT_64_BIT;
		if (commonCount & VK_SAMPLE_COUNT_32_BIT) return VK_SAMPLE_COUNT_32_BIT;
		if (commonCount & VK_SAMPLE_COUNT_16_BIT) return VK_SAMPLE_COUNT_16_BIT;
		if (commonCount & VK_SAMPLE_COUNT_8_BIT) return VK_SAMPLE_COUNT_8_BIT;
		if (commonCount & VK_SAMPLE_COUNT_4_BIT) return VK_SAMPLE_COUNT_4_BIT;
		if (commonCount & VK_SAMPLE_COUNT_2_BIT) return VK_SAMPLE_COUNT_2_BIT;
		return VK_SAMPLE_COUNT_1_BIT;
	}

	inline bool FindSuitableGPU(QbVkContext& context, const eastl::vector<VkPhysicalDevice>& physicalDevices) {
		size_t physicalDeviceCount = physicalDevices.size();
		GPU gpu{};

		// Check and return a discrete GPU if possible
		for (auto i = 0; i < physicalDeviceCount; i++) {
			ZeroMemory(&gpu, sizeof(gpu));
			if (!IsSuitableGPUOfType(context, gpu, physicalDevices[i], VkPhysicalDeviceType::VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)) continue;

			QB_LOG_INFO("Found Appropriate Discrete GPU: %s\n", gpu.deviceProps.deviceName);
			context.gpu = eastl::make_unique<GPU>(gpu);
			context.multisamplingResources.msaaSamples = GetMaxSampleCount(context.gpu->deviceProps);
			return true;
		}

		// Otherwise check and return an integrated GPU if possible
		for (auto i = 0; i < physicalDeviceCount; i++) {
			ZeroMemory(&gpu, sizeof(gpu));
			if (!IsSuitableGPUOfType(context, gpu, physicalDevices[i], VkPhysicalDeviceType::VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU)) continue;

			QB_LOG_INFO("Found Appropriate Integrated GPU: %s\n", gpu.deviceProps.deviceName);
			context.gpu = eastl::make_unique<GPU>(gpu);
			context.multisamplingResources.msaaSamples = GetMaxSampleCount(context.gpu->deviceProps);
			return true;
		}

		return false;
	}

	// Aligns value to nearest multiple. For example: Align(11, 8) = 16.
	template <typename T>
	inline T AlignUp(T val, T alignment)
	{
		static_assert(eastl::is_unsigned<T>::value, "AlignUp requires an unsigned value");
		return ((val + alignment - 1) / alignment) * alignment;
	}

	// Returns whether or not the end of the first resource and the beginning of the second resource
	// reside in separate pages. Algorithm from the Vulkan 1.1.106 specification.
	inline bool IsOnSamePage(VkDeviceSize resourceAOffset, VkDeviceSize resourceASize, VkDeviceSize resourceBOffset, VkDeviceSize pageSize) {
		QB_ASSERT(resourceAOffset + resourceASize <= resourceBOffset && resourceASize > 0 && pageSize > 0);
		VkDeviceSize firstResourceEnd = resourceAOffset + resourceASize - 1;
		VkDeviceSize firstResourceEndPage = firstResourceEnd & ~(pageSize - 1);
		VkDeviceSize secondResourceStartPage = resourceBOffset & ~(pageSize - 1);
		return firstResourceEndPage == secondResourceStartPage;
	}

	inline bool HasGranularityConflict(QbVkAllocationType allocTypeA, QbVkAllocationType allocTypeB) {
		// Swap
		if (allocTypeA > allocTypeB) {
			auto temp = allocTypeA;
			allocTypeA = allocTypeB;
			allocTypeB = temp;
		}

		// Assume conflict for unknown alloc types
		switch (allocTypeA) {
		case QbVkAllocationType::QBVK_ALLOCATION_TYPE_UNKNOWN:
			return true;
		case QbVkAllocationType::QBVK_ALLOCATION_TYPE_FREE:
			return false;
		case QbVkAllocationType::QBVK_ALLOCATION_TYPE_BUFFER:
			return
				allocTypeB == QbVkAllocationType::QBVK_ALLOCATION_TYPE_IMAGE_UNKNOWN ||
				allocTypeB == QbVkAllocationType::QBVK_ALLOCATION_TYPE_IMAGE_OPTIMAL;
		case QbVkAllocationType::QBVK_ALLOCATION_TYPE_IMAGE_UNKNOWN:
			return
				allocTypeB == QbVkAllocationType::QBVK_ALLOCATION_TYPE_IMAGE_UNKNOWN ||
				allocTypeB == QbVkAllocationType::QBVK_ALLOCATION_TYPE_IMAGE_LINEAR ||
				allocTypeB == QbVkAllocationType::QBVK_ALLOCATION_TYPE_IMAGE_OPTIMAL;
		case QbVkAllocationType::QBVK_ALLOCATION_TYPE_IMAGE_LINEAR:
			return allocTypeB == QbVkAllocationType::QBVK_ALLOCATION_TYPE_IMAGE_OPTIMAL;
		case QbVkAllocationType::QBVK_ALLOCATION_TYPE_IMAGE_OPTIMAL:
			return false;
		default:
			QB_ASSERT(false);
			return true;
		}
	}

	inline VkSurfaceFormatKHR ChooseSurfaceFormat(eastl::vector<VkSurfaceFormatKHR>& formats) {
		const VkFormat desiredFormat = VK_FORMAT_B8G8R8_UNORM;
		const VkColorSpaceKHR desiredColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;

		// Vulkan returns undefined if there isn't a preferred format, in which case we will our desired format
		if (formats.size() == 1 && formats[0].format == VK_FORMAT_UNDEFINED) {
			return { desiredFormat, desiredColorSpace };
		}

		// Otherwise let's see if the desired combination is available anyway
		for (int i = 0; i < formats.size(); i++) {
			if (formats[i].format == desiredFormat && formats[i].colorSpace == desiredColorSpace) {
				return formats[i];
			}
		}

		// Otherwise just return the first available format (could do rankings, but should be unnecessary)
		return formats[0];
	}

	inline VkPresentModeKHR ChoosePresentMode(eastl::vector<VkPresentModeKHR>& modes) {
		const VkPresentModeKHR desiredPresentMode = VK_PRESENT_MODE_MAILBOX_KHR;

		for (int i = 0; i < modes.size(); i++) {
			if (modes[i] == desiredPresentMode) {
				return desiredPresentMode;
			}
		}

		// If the desired mode is not present, we will fall back to FIFO which should be available
		return VK_PRESENT_MODE_FIFO_KHR;
	}

	inline VkExtent2D ChooseSurfaceExtent(VkSurfaceCapabilitiesKHR& capabilities, HWND& hwnd) {
		// This should be the current extent of the surface capabilities
		if (capabilities.currentExtent.width != (uint32_t)-1) {
			return capabilities.currentExtent;
		}
		// Otherwise we'll get the appropriate size from the window handle (perhaps some global settings in the future?)
		else {
			RECT clientRect;
			GetClientRect(hwnd, &clientRect);
			return { static_cast<uint32_t>(clientRect.right), static_cast<uint32_t>(clientRect.bottom) };
		}
	}

	inline VkShaderModule CreateShaderModule(const eastl::vector<char>& bytecode, VkDevice& device) {
		// Creates the shader module using raw bytecode
		VkShaderModuleCreateInfo shaderModuleInfo{};
		shaderModuleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		shaderModuleInfo.codeSize = bytecode.size();
		shaderModuleInfo.pCode = reinterpret_cast<const uint32_t*>(bytecode.data());

		VkShaderModule shaderModule;
		VK_CHECK(vkCreateShaderModule(device, &shaderModuleInfo, nullptr, &shaderModule));

		return shaderModule;
	}

	inline VkShaderModule CreateShaderModule(const eastl::vector<uint32_t>& bytecode, VkDevice& device) {
		// Creates the shader module using raw bytecode
		VkShaderModuleCreateInfo shaderModuleInfo{};
		shaderModuleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		shaderModuleInfo.codeSize = bytecode.size();
		shaderModuleInfo.pCode = bytecode.data();

		VkShaderModule shaderModule;
		VK_CHECK(vkCreateShaderModule(device, &shaderModuleInfo, nullptr, &shaderModule));

		return shaderModule;
	}

	inline VkShaderModule CreateShaderModule(const uint32_t* data, const uint32_t size, VkDevice& device) {
		// Creates the shader module using raw bytecode
		VkShaderModuleCreateInfo shaderModuleInfo{};
		shaderModuleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		shaderModuleInfo.codeSize = size * sizeof(uint32_t);
		shaderModuleInfo.pCode = data;

		VkShaderModule shaderModule;
		VK_CHECK(vkCreateShaderModule(device, &shaderModuleInfo, nullptr, &shaderModule));

		return shaderModule;
	}

	inline bool HasStencilComponent(VkFormat format) {
		return (format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT);
	}

	inline eastl::vector<char> ReadShader(const char* filename) {
		FILE* pFile = fopen(filename, "rb");
		if (pFile == nullptr) {
			QB_LOG_ERROR("Couldn't open file %s\n", filename);
			return {};
		}

		// Get file size
		fseek(pFile, 0, SEEK_END);
		uint32_t fileSize = ftell(pFile);
		rewind(pFile);

		// Read data into a char vector
		eastl::vector<char> bytecode(fileSize);
		size_t result = fread(bytecode.data(), sizeof(char), fileSize, pFile);
		if (result != fileSize) {
			QB_LOG_ERROR("Error reading shader file %s\n", filename);
			return {};
		}
		fclose(pFile);

		return bytecode;
	}

	inline uint32_t FindMemoryType(VkPhysicalDeviceMemoryProperties memoryProperties, uint32_t typeFilter, VkMemoryPropertyFlags requestedProperties) {
		for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; i++) {
			if ((typeFilter & (1 << i)) && (memoryProperties.memoryTypes[i].propertyFlags & requestedProperties) == requestedProperties) {
				return i;
			}
		}
		return eastl::numeric_limits<uint32_t>().max();
	}

	inline VkFormat ChooseSupportedFormat(const QbVkContext& context, const eastl::vector<VkFormat>& formats, VkImageTiling tiling, VkFormatFeatureFlags features) {
		for (VkFormat format : formats) {
			VkFormatProperties formatProperties;
			vkGetPhysicalDeviceFormatProperties(context.gpu->physicalDevice, format, &formatProperties);
			if (tiling == VK_IMAGE_TILING_LINEAR && (formatProperties.linearTilingFeatures & features) == features) {
				return format;
			}
			else if (tiling == VK_IMAGE_TILING_OPTIMAL && (formatProperties.optimalTilingFeatures & features) == features) {
				return format;
			}
		}
		return VK_FORMAT_UNDEFINED;
	}

	inline VkFormat FindDepthFormat(const QbVkContext& context) {
		return ChooseSupportedFormat(context, { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
			VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
	}

	inline VkImageView CreateImageView(const QbVkContext& context, VkImage image, VkFormat format,
		VkImageAspectFlags aspectFlags, VkImageViewType imageViewType = VK_IMAGE_VIEW_TYPE_2D, uint32_t levelCount = 1, uint32_t layerCount = 1) {
		VkImageViewCreateInfo imageViewInfo = Init::ImageViewCreateInfo();
		imageViewInfo.image = image;
		imageViewInfo.viewType = imageViewType;
		imageViewInfo.format = format;

		imageViewInfo.components.r = VK_COMPONENT_SWIZZLE_R;
		imageViewInfo.components.g = VK_COMPONENT_SWIZZLE_G;
		imageViewInfo.components.b = VK_COMPONENT_SWIZZLE_B;
		imageViewInfo.components.a = VK_COMPONENT_SWIZZLE_A;

		imageViewInfo.subresourceRange.aspectMask = aspectFlags;

		imageViewInfo.subresourceRange.baseMipLevel = 0;
		// Level count is the number of images in the mipmap
		imageViewInfo.subresourceRange.levelCount = levelCount;

		// We don't use multiple layers
		imageViewInfo.subresourceRange.baseArrayLayer = 0;
		imageViewInfo.subresourceRange.layerCount = layerCount;
		imageViewInfo.flags = 0;

		// Create the image view
		VkImageView imageView;
		vkCreateImageView(context.device, &imageViewInfo, nullptr, &imageView);
		return imageView;
	}

	inline VkCommandBuffer CreatePersistentCommandBuffer(const QbVkContext& context) {
		VkCommandBuffer commandbuffer;
		VkCommandBufferAllocateInfo commandBufferAllocateInfo = VkUtils::Init::CommandBufferAllocateInfo(context.commandPool,
			VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1);
		VK_CHECK(vkAllocateCommandBuffers(context.device, &commandBufferAllocateInfo, &commandbuffer));
		return commandbuffer;
	}

	inline VkCommandBuffer CreateSingleTimeCommandBuffer(const QbVkContext& context) {
		VkCommandBufferAllocateInfo allocInfo = Init::CommandBufferAllocateInfo(context.commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1);

		// Create command buffer
		VkCommandBuffer commandBuffer;
		VK_CHECK(vkAllocateCommandBuffers(context.device, &allocInfo, &commandBuffer));

		// Begin recording
		VkCommandBufferBeginInfo cmdBeginInfo = Init::CommandBufferBeginInfo();
		cmdBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		VK_CHECK(vkBeginCommandBuffer(commandBuffer, &cmdBeginInfo));

		return commandBuffer;
	}

	inline void FlushCommandBuffer(const QbVkContext& context, VkCommandBuffer commandBuffer) {
		// End recording
		VK_CHECK(vkEndCommandBuffer(commandBuffer));

		VkSubmitInfo submitInfo = Init::SubmitInfo();
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffer;

		// Create fence to wait on command buffer
		VkFenceCreateInfo fenceCreateInfo = Init::FenceCreateInfo();
		VkFence fence;
		VK_CHECK(vkCreateFence(context.device, &fenceCreateInfo, nullptr, &fence));

		// Submit to queue
		VK_CHECK(vkQueueSubmit(context.graphicsQueue, 1, &submitInfo, fence));
		// Wait for the fence to signal that the command buffer has finished
		VK_CHECK(vkWaitForFences(context.device, 1, &fence, VK_TRUE, eastl::numeric_limits<uint64_t>().max()));

		vkFreeCommandBuffers(context.device, context.commandPool, 1, &commandBuffer);
		vkDestroyFence(context.device, fence, nullptr);
	}

	inline void CopyBuffer(const QbVkContext& context, VkBuffer src, VkBuffer dst, VkDeviceSize size) {
		VkCommandBuffer commandBuffer = CreateSingleTimeCommandBuffer(context);
		// Issue copy command
		VkBufferCopy copyRegion{};
		copyRegion.srcOffset = 0;
		copyRegion.dstOffset = 0;
		copyRegion.size = size;
		vkCmdCopyBuffer(commandBuffer, src, dst, 1, &copyRegion);

		// Flush command buffer (End, submit and delete)
		FlushCommandBuffer(context, commandBuffer);
	}

	inline void CopyBufferToImage(const QbVkContext& context, VkBuffer src, VkImage dst, uint32_t width, uint32_t height) {
		VkCommandBuffer commandBuffer = CreateSingleTimeCommandBuffer(context);
		// Issue copy command
		VkBufferImageCopy copyRegion{};
		copyRegion.bufferOffset = 0;
		copyRegion.bufferImageHeight = 0;
		copyRegion.bufferRowLength = 0;
		copyRegion.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
		copyRegion.imageOffset = { 0, 0, 0 };
		copyRegion.imageExtent = { width, height, 1 };
		vkCmdCopyBufferToImage(commandBuffer, src, dst, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

		// Flush command buffer
		FlushCommandBuffer(context, commandBuffer);
	}

	inline void TransferDataToGPUBuffer(const QbVkContext& context, const void* data, VkDeviceSize size, QbVkBuffer& destination) {
		// Utilize a staging buffer to transfer the data onto the GPU
		QbVkBuffer stagingBuffer;
		context.allocator->CreateStagingBuffer(stagingBuffer, size, data);
		CopyBuffer(context, stagingBuffer.buf, destination.buf, size);
		context.allocator->DestroyBuffer(stagingBuffer);
	}

	inline void CreateFrameBuffer(const QbVkContext& context, VkFramebuffer& framebuffer, VkImageView imageView) {
		// Might be troublemaker
		if (framebuffer != VK_NULL_HANDLE) {
			vkDestroyFramebuffer(context.device, framebuffer, nullptr);
			framebuffer = VK_NULL_HANDLE;
		}

		// Here we will fill out the common information for the framebuffer
		VkFramebufferCreateInfo framebufferInfo = Init::FramebufferCreateInfo();
		framebufferInfo.renderPass = context.mainRenderPass;
		framebufferInfo.width = context.swapchain.extent.width;
		framebufferInfo.height = context.swapchain.extent.height;
		framebufferInfo.layers = 1;
		eastl::array<VkImageView, 3> attachments = { context.multisamplingResources.msaaImageView, context.depthResources.imageView, imageView };
		framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
		framebufferInfo.pAttachments = attachments.data();

		// Finally create it
		VK_CHECK(vkCreateFramebuffer(context.device, &framebufferInfo, nullptr, &framebuffer))
	}

	inline void TransitionImageLayout(const QbVkContext& context, VkImage image, VkImageAspectFlags aspectFlags,
		VkImageLayout oldLayout, VkImageLayout newLayout, VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage, uint32_t levelCount = 1, uint32_t layerCount = 1) {

		VkCommandBuffer commandBuffer = CreateSingleTimeCommandBuffer(context);

		VkImageMemoryBarrier memoryBarrier = Init::ImageMemoryBarrier();
		memoryBarrier.oldLayout = oldLayout;
		memoryBarrier.newLayout = newLayout;
		memoryBarrier.image = image;

		memoryBarrier.subresourceRange.baseMipLevel = 0;
		memoryBarrier.subresourceRange.levelCount = levelCount;
		memoryBarrier.subresourceRange.baseArrayLayer = 0;
		memoryBarrier.subresourceRange.layerCount = layerCount;
		memoryBarrier.subresourceRange.aspectMask = aspectFlags;

		switch (oldLayout) {
		case VK_IMAGE_LAYOUT_UNDEFINED:
			// Image layout is undefined (no access mask required)
			memoryBarrier.srcAccessMask = 0;
			break;
		case VK_IMAGE_LAYOUT_PREINITIALIZED:
			// Image is preinitialized, make sure host writes have finished
			memoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
			break;
		case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
			// Image is a color attachment
			memoryBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			break;
		case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
			// Image is a depth/stencil attachment
			memoryBarrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			break;
		case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
			// Image is a transfer source
			memoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
			break;
		case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
			// Image is a transfer destination
			memoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			break;
		case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
			// Image is read by a shader
			memoryBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
			break;
		default:
			break;
		}
		switch (newLayout) {
		case VK_IMAGE_LAYOUT_GENERAL:
			memoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			break;
		case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
			// image is a transfer destination
			memoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			break;
		case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
			// image is a transfer source
			memoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
			break;
		case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
			// image is a color attachment
			memoryBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			break;
		case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
			// image is a depth/stencil attachment
			memoryBarrier.dstAccessMask =
				VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			break;
		case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
			// image is read in shader
			if (memoryBarrier.srcAccessMask == 0) {
				memoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
			}
			memoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			break;
		default:
			break;
		}

		vkCmdPipelineBarrier(commandBuffer, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &memoryBarrier);

		FlushCommandBuffer(context, commandBuffer);
	}

	inline VkFormat GetVertexFormatFromSize(const uint32_t size) {
		switch (size) {
		case 4:
			return VK_FORMAT_R32_SFLOAT;
		case 8:
			return VK_FORMAT_R32G32_SFLOAT;
		case 12:
			return VK_FORMAT_R32G32B32_SFLOAT;
		case 16:
			return VK_FORMAT_R32G32B32A32_SFLOAT;
		default:
			QB_ASSERT(false && "Vertex input attribute not recognized");
			return VK_FORMAT_UNDEFINED;
		}
	}

	inline eastl::vector<VkVertexInputAttributeDescription> CreateVertexInputAttributeDescription(eastl::vector<QbVkVertexInputAttribute> attributes) {
		eastl::vector<VkVertexInputAttributeDescription> vertexAttributes;

		uint32_t offset = 0;
		for (uint32_t i = 0; i < attributes.size(); i++) {
			switch (attributes[i]) {
			case QbVkVertexInputAttribute::QBVK_VERTEX_ATTRIBUTE_POSITION:
				vertexAttributes.push_back({ i, 0, VK_FORMAT_R32G32B32_SFLOAT, offset });
				offset += sizeof(float) * 3;
				break;
			case QbVkVertexInputAttribute::QBVK_VERTEX_ATTRIBUTE_NORMAL:
				vertexAttributes.push_back({ i, 0, VK_FORMAT_R32G32B32_SFLOAT, offset });
				offset += sizeof(float) * 3;
				break;
			case QbVkVertexInputAttribute::QBVK_VERTEX_ATTRIBUTE_UV:
				vertexAttributes.push_back({ i, 0, VK_FORMAT_R32G32_SFLOAT, offset });
				offset += sizeof(float) * 2;
				break;
			case QbVkVertexInputAttribute::QBVK_VERTEX_ATTRIBUTE_COLOUR:
				vertexAttributes.push_back({ i, 0, VK_FORMAT_R32G32B32_SFLOAT, offset });
				offset += sizeof(float) * 3;
				break;
			case QbVkVertexInputAttribute::QBVK_VERTEX_ATTRIBUTE_FLOAT:
				vertexAttributes.push_back({ i, 0, VK_FORMAT_R32G32B32_SFLOAT, offset });
				offset += sizeof(float);
				break;
			case QbVkVertexInputAttribute::QBVK_VERTEX_ATTRIBUTE_FLOAT4:
				vertexAttributes.push_back({ i, 0, VK_FORMAT_R32G32B32_SFLOAT, offset });
				offset += sizeof(float) * 4;
				break;
			}
		}

		return vertexAttributes;
	}

	inline VkVertexInputBindingDescription GetVertexBindingDescription(eastl::vector<QbVkVertexInputAttribute> attributes) {
		uint32_t stride = 0;
		for (uint32_t i = 0; i < attributes.size(); i++) {
			switch (attributes[i]) {
			case QbVkVertexInputAttribute::QBVK_VERTEX_ATTRIBUTE_POSITION:
				stride += sizeof(float) * 3;
				break;
			case QbVkVertexInputAttribute::QBVK_VERTEX_ATTRIBUTE_NORMAL:
				stride += sizeof(float) * 3;
				break;
			case QbVkVertexInputAttribute::QBVK_VERTEX_ATTRIBUTE_UV:
				stride += sizeof(float) * 2;
				break;
			case QbVkVertexInputAttribute::QBVK_VERTEX_ATTRIBUTE_COLOUR:
				stride += sizeof(float) * 3;
				break;
			case QbVkVertexInputAttribute::QBVK_VERTEX_ATTRIBUTE_FLOAT:
				stride += sizeof(float);
				break;
			case QbVkVertexInputAttribute::QBVK_VERTEX_ATTRIBUTE_FLOAT4:
				stride += sizeof(float) * 4;
				break;
			}
		}

		VkVertexInputBindingDescription bindingDescription{};
		bindingDescription.binding = 0;
		bindingDescription.stride = stride;
		bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
		return bindingDescription;
	}
}
