#pragma once
#include <EASTL/vector.h>

#include "Engine/Rendering/VulkanTypes.h"

namespace Quadbit {
	struct Plane {
		eastl::vector<QbVkVertex> vertices;
		eastl::vector<uint32_t> indices;

		Plane(uint32_t xSize, uint32_t zSize, uint32_t textureTileCount = 1) {
			vertices.push_back({ .position = { 0.0f, 0.0f,  0.0f}, .normal = {0.0f, 1.0f, 0.0f}, .uv0 = {0.0f,			   0.0f},			  .uv1 = {0.0f, 0.0f} });
			vertices.push_back({ .position = { 0.0f, 0.0f, zSize}, .normal = {0.0f, 1.0f, 0.0f}, .uv0 = {0.0f,			   textureTileCount}, .uv1 = {0.0f, 0.0f} });
			vertices.push_back({ .position = {xSize, 0.0f, zSize}, .normal = {0.0f, 1.0f, 0.0f}, .uv0 = {textureTileCount, textureTileCount}, .uv1 = {0.0f, 0.0f} });
			vertices.push_back({ .position = {xSize, 0.0f,  0.0f}, .normal = {0.0f, 1.0f, 0.0f}, .uv0 = {textureTileCount, 0.0f},			  .uv1 = {0.0f, 0.0f} });
			indices.assign({ 0, 1, 2, 0, 2, 3 });
		}
	};
}