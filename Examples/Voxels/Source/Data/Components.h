#pragma once
#include <type_traits>
#include <array>

#include "Defines.h"

enum class VisibleFaces {
	North = 1,		// 000001
	South = 1 << 1,   // 000010
	East = 1 << 2,   // 000100
	West = 1 << 3,	// 001000
	Top = 1 << 4,	// 010000
	Bottom = 1 << 5	// 100000
};
inline VisibleFaces operator | (VisibleFaces lhs, VisibleFaces rhs) {
	using type = std::underlying_type_t<VisibleFaces>;
	return static_cast<VisibleFaces>(static_cast<type>(lhs) | static_cast<type>(rhs));
}
inline VisibleFaces& operator |= (VisibleFaces& lhs, VisibleFaces rhs) {
	lhs = lhs | rhs;
	return lhs;
}
inline VisibleFaces operator & (VisibleFaces lhs, VisibleFaces rhs) {
	using type = std::underlying_type_t<VisibleFaces>;
	return static_cast<VisibleFaces>(static_cast<type>(lhs) & static_cast<type>(rhs));
}
inline VisibleFaces& operator &= (VisibleFaces& lhs, VisibleFaces rhs) {
	lhs = lhs & rhs;
	return lhs;
}
inline VisibleFaces operator ~ (const VisibleFaces a) {
	using type = std::underlying_type_t<VisibleFaces>;
	return static_cast<VisibleFaces>(~static_cast<type>(a));
}

enum FillType {
	Empty,
	Solid
};

struct Voxel {
	FillType fillType;
	VisibleFaces visibleFaces;
	glm::float4 ambientOcclusion;
};

struct VoxelBlockUpdateTag : Quadbit::EventTagComponent {};
struct MeshGenerationUpdateTag : Quadbit::EventTagComponent {};

struct VoxelBlockComponent {
	// Could be stored in a std::array but might overflow stack size
	// Heap allocated in a vector seems like the best option
	VoxelBlockComponent() {
		voxels.resize(VOXEL_BLOCK_SIZE);
	}
	std::vector<Voxel> voxels;
};

struct PlayerTag {};

struct VoxelVertex {
	glm::vec3 pos;
	glm::vec3 col;
};

const std::vector<VoxelVertex> cubeVertices = {
{{-1.0f, -1.0f, 1.0f},	{1.0f, 0.0f, 0.0f}},
{{1.0f, -1.0f, 1.0f},	{0.0f, 1.0f, 0.0f}},
{{1.0f, 1.0f, 1.0f},	{0.0f, 0.0f, 1.0f}},
{{-1.0f, 1.0f, 1.0f},	{1.0f, 1.0f, 0.0f}},

{{-1.0f, -1.0f, -1.0f}, {1.0f, 0.0f, 0.0f}},
{{1.0f, -1.0f, -1.0f},	{0.0f, 1.0f, 0.0f}},
{{1.0f, 1.0f, -1.0f},	{0.0f, 0.0f, 1.0f}},
{{-1.0f, 1.0f, -1.0f},	{1.0f, 1.0f, 0.0f}}
};

const std::vector<uint32_t> cubeIndices = {
	0, 1, 2,
	2, 3, 0,
	1, 5, 6,
	6, 2, 1,
	7, 6, 5,
	5, 4, 7,
	4, 0, 3,
	3, 7, 4,
	4, 5, 1,
	1, 0, 4,
	3, 2, 6,
	6, 7, 3
};