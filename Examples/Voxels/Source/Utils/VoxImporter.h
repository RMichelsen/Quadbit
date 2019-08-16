#include <vector>
#include <iostream>
#include <fstream>

#include <glm/glm.hpp>
#include <glm/gtx/compatibility.hpp>

#include "../Data/Components.h"

namespace VoxImporter {
	inline std::array<uint32_t, 256> default_palette = {
		0x00000000, 0xffffffff, 0xffccffff, 0xff99ffff, 0xff66ffff, 0xff33ffff, 0xff00ffff, 0xffffccff, 0xffccccff, 0xff99ccff, 0xff66ccff, 0xff33ccff, 0xff00ccff, 0xffff99ff, 0xffcc99ff, 0xff9999ff,
		0xff6699ff, 0xff3399ff, 0xff0099ff, 0xffff66ff, 0xffcc66ff, 0xff9966ff, 0xff6666ff, 0xff3366ff, 0xff0066ff, 0xffff33ff, 0xffcc33ff, 0xff9933ff, 0xff6633ff, 0xff3333ff, 0xff0033ff, 0xffff00ff,
		0xffcc00ff, 0xff9900ff, 0xff6600ff, 0xff3300ff, 0xff0000ff, 0xffffffcc, 0xffccffcc, 0xff99ffcc, 0xff66ffcc, 0xff33ffcc, 0xff00ffcc, 0xffffcccc, 0xffcccccc, 0xff99cccc, 0xff66cccc, 0xff33cccc,
		0xff00cccc, 0xffff99cc, 0xffcc99cc, 0xff9999cc, 0xff6699cc, 0xff3399cc, 0xff0099cc, 0xffff66cc, 0xffcc66cc, 0xff9966cc, 0xff6666cc, 0xff3366cc, 0xff0066cc, 0xffff33cc, 0xffcc33cc, 0xff9933cc,
		0xff6633cc, 0xff3333cc, 0xff0033cc, 0xffff00cc, 0xffcc00cc, 0xff9900cc, 0xff6600cc, 0xff3300cc, 0xff0000cc, 0xffffff99, 0xffccff99, 0xff99ff99, 0xff66ff99, 0xff33ff99, 0xff00ff99, 0xffffcc99,
		0xffcccc99, 0xff99cc99, 0xff66cc99, 0xff33cc99, 0xff00cc99, 0xffff9999, 0xffcc9999, 0xff999999, 0xff669999, 0xff339999, 0xff009999, 0xffff6699, 0xffcc6699, 0xff996699, 0xff666699, 0xff336699,
		0xff006699, 0xffff3399, 0xffcc3399, 0xff993399, 0xff663399, 0xff333399, 0xff003399, 0xffff0099, 0xffcc0099, 0xff990099, 0xff660099, 0xff330099, 0xff000099, 0xffffff66, 0xffccff66, 0xff99ff66,
		0xff66ff66, 0xff33ff66, 0xff00ff66, 0xffffcc66, 0xffcccc66, 0xff99cc66, 0xff66cc66, 0xff33cc66, 0xff00cc66, 0xffff9966, 0xffcc9966, 0xff999966, 0xff669966, 0xff339966, 0xff009966, 0xffff6666,
		0xffcc6666, 0xff996666, 0xff666666, 0xff336666, 0xff006666, 0xffff3366, 0xffcc3366, 0xff993366, 0xff663366, 0xff333366, 0xff003366, 0xffff0066, 0xffcc0066, 0xff990066, 0xff660066, 0xff330066,
		0xff000066, 0xffffff33, 0xffccff33, 0xff99ff33, 0xff66ff33, 0xff33ff33, 0xff00ff33, 0xffffcc33, 0xffcccc33, 0xff99cc33, 0xff66cc33, 0xff33cc33, 0xff00cc33, 0xffff9933, 0xffcc9933, 0xff999933,
		0xff669933, 0xff339933, 0xff009933, 0xffff6633, 0xffcc6633, 0xff996633, 0xff666633, 0xff336633, 0xff006633, 0xffff3333, 0xffcc3333, 0xff993333, 0xff663333, 0xff333333, 0xff003333, 0xffff0033,
		0xffcc0033, 0xff990033, 0xff660033, 0xff330033, 0xff000033, 0xffffff00, 0xffccff00, 0xff99ff00, 0xff66ff00, 0xff33ff00, 0xff00ff00, 0xffffcc00, 0xffcccc00, 0xff99cc00, 0xff66cc00, 0xff33cc00,
		0xff00cc00, 0xffff9900, 0xffcc9900, 0xff999900, 0xff669900, 0xff339900, 0xff009900, 0xffff6600, 0xffcc6600, 0xff996600, 0xff666600, 0xff336600, 0xff006600, 0xffff3300, 0xffcc3300, 0xff993300,
		0xff663300, 0xff333300, 0xff003300, 0xffff0000, 0xffcc0000, 0xff990000, 0xff660000, 0xff330000, 0xff0000ee, 0xff0000dd, 0xff0000bb, 0xff0000aa, 0xff000088, 0xff000077, 0xff000055, 0xff000044,
		0xff000022, 0xff000011, 0xff00ee00, 0xff00dd00, 0xff00bb00, 0xff00aa00, 0xff008800, 0xff007700, 0xff005500, 0xff004400, 0xff002200, 0xff001100, 0xffee0000, 0xffdd0000, 0xffbb0000, 0xffaa0000,
		0xff880000, 0xff770000, 0xff550000, 0xff440000, 0xff220000, 0xff110000, 0xffeeeeee, 0xffdddddd, 0xffbbbbbb, 0xffaaaaaa, 0xff888888, 0xff777777, 0xff555555, 0xff444444, 0xff222222, 0xff111111
	};

	struct MagicaChunkHeader {
		std::array<char, 4> identifier;
		int numBytesChunkContent;
		int numBytesChildrenChunks;
	};

	struct MagicaVoxel {
		bool solid = false;
		uint8_t colourIndex = 0;
		VisibleFaces visibleFaces = VisibleFaces::North;
	};

	struct MagicaVoxelColour {
		uint8_t r, g, b, a;
	};

	struct MagicaVoxelModel {
		std::vector<MagicaVoxel> voxels;
		std::array<MagicaVoxelColour, 256> palette;
		bool customPalette;
		glm::int3 extents;
	};
	
	struct MagicaVoxelMesh {
		std::vector<VoxelVertex> vertices;
		std::vector<uint32_t> indices;
	};

	inline MagicaChunkHeader ReadMagicaChunkHeader(std::ifstream& model) {
		MagicaChunkHeader header{};
		model.read(header.identifier.data(), 4);
		model.read(reinterpret_cast<char*>(&header.numBytesChunkContent), 4);
		model.read(reinterpret_cast<char*>(&header.numBytesChildrenChunks), 4);
		return header;
	}

	inline glm::float3 ExtractRGBA(uint32_t colour) {
		return glm::float3 { colour & 0xFF, (colour >> 8) & 0xFF, (colour >> 16) & 0xFF };
	}

	inline MagicaVoxelModel LoadVoxModel(const char* voxPath) {
		auto model = std::ifstream(voxPath, std::ios::binary);
		if(!model) {
			printf("Failed to load object from file: %s\n", voxPath);
			return {};
		}

		std::array<char, 4> identifier;
		model.read(identifier.data(), 4);
		if(identifier != std::array { 'V', 'O', 'X', ' '}) {
			printf("Wrong identifier, can't load file\n");
			return {};
		}

		int version;
		model.read(reinterpret_cast<char*>(&version), 4);
		if(version != 150) {
			printf("Version isn't 150, can't load file\n");
			return {};
		}

		auto header = ReadMagicaChunkHeader(model);
		if(header.identifier != std::array{ 'M', 'A', 'I', 'N' }) {
			printf("Unrecognized chunk ID, can't load file\n");
			return {};
		}

		header = ReadMagicaChunkHeader(model);
		if(header.identifier == std::array{ 'P', 'A', 'C', 'K' }) {
			printf("Voxel importer only supports single models, can't load file\n");
			return {};
		}
		else if(header.identifier != std::array{ 'S', 'I', 'Z', 'E' }) {
			printf("Unrecognized chunk ID, can't load file\n");
			return {};
		}

		int xSize;
		int ySize;
		int zSize;
		model.read(reinterpret_cast<char*>(&xSize), 4);
		model.read(reinterpret_cast<char*>(&zSize), 4);
		model.read(reinterpret_cast<char*>(&ySize), 4);

		header = ReadMagicaChunkHeader(model);
		if(header.identifier != std::array{ 'X', 'Y', 'Z', 'I' }) {
			printf("Unrecognized chunk ID, can't load file\n");
			return {};
		}

		int numVoxels;
		model.read(reinterpret_cast<char*>(&numVoxels), 4);

		MagicaVoxelModel voxelModel;
		voxelModel.extents = { xSize, ySize, zSize };
		voxelModel.voxels.resize(static_cast<uint64_t>(xSize) * static_cast<uint64_t>(ySize) * static_cast<uint64_t>(zSize) * 5);

		for(auto i = 0; i < numVoxels; i++) {
			uint8_t x, y, z, colourIndex;
			model.read(reinterpret_cast<char*>(&x), 1);
			model.read(reinterpret_cast<char*>(&z), 1);
			model.read(reinterpret_cast<char*>(&y), 1);
			model.read(reinterpret_cast<char*>(&colourIndex), 1);

			voxelModel.voxels[static_cast<uint64_t>(x) + xSize * (static_cast<uint64_t>(z) + zSize * static_cast<uint64_t>(y))] = {
				true,
				colourIndex,
				VisibleFaces::North
			};
		}

		header = ReadMagicaChunkHeader(model);
		if(header.identifier == std::array{ 'R', 'G', 'B', 'A' }) {
			voxelModel.customPalette = true;

			for(auto i = 0; i < 0xFF; i++) {
				model.read(reinterpret_cast<char*>(&voxelModel.palette[static_cast<uint64_t>(i) + 1].r), 1);
				model.read(reinterpret_cast<char*>(&voxelModel.palette[static_cast<uint64_t>(i) + 1].g), 1);
				model.read(reinterpret_cast<char*>(&voxelModel.palette[static_cast<uint64_t>(i) + 1].b), 1);
				model.read(reinterpret_cast<char*>(&voxelModel.palette[static_cast<uint64_t>(i) + 1].a), 1);
			}
		}
		return voxelModel;
	}

	inline uint32_t ConvertIndex(glm::int3 index, glm::int3 extents) {
		return index.x + extents.x * (index.z + extents.z * index.y);
	}

	// For a given face, returns a direction for the greedy meshing
	inline glm::int2x3 GetDirectionVectors(const VisibleFaces face) {
		switch(face) {
		case VisibleFaces::North:
		case VisibleFaces::South:
			return { {1, 0, 0}, {0, 1, 0} };
		case VisibleFaces::East:
		case VisibleFaces::West:
			return { {0, 1, 0}, {0, 0, 1} };
		case VisibleFaces::Top:
		case VisibleFaces::Bottom:
			return { {1, 0, 0}, {0, 0, 1} };
		default:
			return { {0, 0, 0}, {0, 0, 0} };
		}
	}

	// Returns the offsets for the ambient occlusion for a particular face.
	// The first two float3's are the sides, the last is the corner offset.
	inline std::array<const glm::int3x3, 4> GetAmbientOcclusionOffsets(const VisibleFaces face) {
		static const std::array<glm::int3x3, 8> aoOffsets{ {
			{{0, 0, -1}, {-1, 0, 0}, {-1, 0, -1}},
			{{0, 1, -1}, {-1, 1, 0}, {-1, 1, -1}},
			{{-1, 0, 0}, {0, 0, 1},  {-1, 0, 1}},
			{{-1, 1, 0}, {0, 1, 1},  {-1, 1, 1}},
			{{1, 0, 0},  {0, 0, -1}, {1, 0, -1}},
			{{1, 1, 0},  {0, 1, -1}, {1, 1, -1}},
			{{1, 0, 0},  {0, 0, 1},  {1, 0, 1}},
			{{1, 1, 0},  {0, 1, 1},  {1, 1, 1}}
		} };

		switch(face) {
		case VisibleFaces::North:
			//return { aoOffsets[6], aoOffsets[7], aoOffsets[3], aoOffsets[2] };
			return { glm::int3x3{{0,-1,1},{-1,0,1},{-1,-1,1}}, glm::int3x3{{0,1,1},{-1,0,1},{-1,1,1}}, glm::int3x3{{0,1,1},{1,0,1},{1,1,1}}, glm::int3x3{{0,-1,1},{1,0,1},{1,-1,1}} };
		case VisibleFaces::South:
			return { aoOffsets[4], aoOffsets[0], aoOffsets[1], aoOffsets[5] };
		case VisibleFaces::East:
			return { aoOffsets[4], aoOffsets[5], aoOffsets[7], aoOffsets[6] };
		case VisibleFaces::West:
			return { aoOffsets[0], aoOffsets[2], aoOffsets[3], aoOffsets[1] };
		case VisibleFaces::Top:
			return { aoOffsets[5], aoOffsets[1], aoOffsets[3], aoOffsets[7] };
		case VisibleFaces::Bottom:
			return { aoOffsets[4], aoOffsets[6], aoOffsets[2], aoOffsets[0] };
		default:
			return { aoOffsets[0], aoOffsets[0], aoOffsets[0], aoOffsets[0] };
		}
	}

	// Important: Side 3 is the corner side
	inline float CalculateVertexOcclusion(glm::int3x3 sides, const MagicaVoxelModel& model) {
		static const glm::float4 OCCLUSION_CURVE = glm::float4{ 0.55f, 0.7f, 0.85f, 1.0f };

		const bool ocSide1 = (sides[0].x < model.extents.x && sides[0].x >= 0 && sides[0].y < model.extents.y && sides[0].y >= 0 && sides[0].z < model.extents.z && sides[0].z >= 0)
			? (model.voxels[ConvertIndex(sides[0], model.extents)].solid) : false;
		const bool ocSide2 = (sides[1].x < model.extents.x && sides[1].x >= 0 && sides[1].y < model.extents.y && sides[1].y >= 0 && sides[1].z < model.extents.z && sides[1].z >= 0)
			? (model.voxels[ConvertIndex(sides[1], model.extents)].solid) : false;
		const bool ocCorner = (sides[2].x < model.extents.x && sides[2].x >= 0 && sides[2].y < model.extents.y && sides[2].y >= 0 && sides[2].z < model.extents.z && sides[2].z >= 0)
			? (model.voxels[ConvertIndex(sides[2], model.extents)].solid) : false;

		if(ocSide1 && ocSide2) {
			return OCCLUSION_CURVE[0];
		}

		return OCCLUSION_CURVE[3 - (ocSide1 + ocSide2 + ocCorner)];
	}

	inline glm::float4 CalculateFaceAmbientOcclusion(const VisibleFaces face, const glm::ivec3 position, const MagicaVoxelModel& model) {
		auto ambientOcclusionOffsets = GetAmbientOcclusionOffsets(face);

		return {
			CalculateVertexOcclusion({ambientOcclusionOffsets[0][0] + position, ambientOcclusionOffsets[0][1] + position, ambientOcclusionOffsets[0][2] + position}, model),
			CalculateVertexOcclusion({ambientOcclusionOffsets[1][0] + position, ambientOcclusionOffsets[1][1] + position, ambientOcclusionOffsets[1][2] + position}, model),
			CalculateVertexOcclusion({ambientOcclusionOffsets[2][0] + position, ambientOcclusionOffsets[2][1] + position, ambientOcclusionOffsets[2][2] + position}, model),
			CalculateVertexOcclusion({ambientOcclusionOffsets[3][0] + position, ambientOcclusionOffsets[3][1] + position, ambientOcclusionOffsets[3][2] + position}, model)
		};
	}

	inline glm::int2x3 GetAxisEndpoints(glm::int2x3 dirs, glm::int3 voxelPosition, const MagicaVoxelModel& model) {
		glm::int2x3 axisVectors{ voxelPosition, voxelPosition };

		if(dirs[0].x == 1) axisVectors[0].x = model.extents.x;
		else if(dirs[0].y == 1) axisVectors[0].y = model.extents.y;
		else if(dirs[0].z == 1) axisVectors[0].z = model.extents.z;
		if(dirs[1].x == 1) axisVectors[1].x = model.extents.x;
		else if(dirs[1].y == 1) axisVectors[1].y = model.extents.y;
		else if(dirs[1].z == 1) axisVectors[1].z = model.extents.z;

		return axisVectors;
	}

	inline glm::int4x3 GetPointsForFaces(const VisibleFaces face, const glm::int4x3 points) {
		glm::int3 idx3D = points[0];
		glm::int3 firstDest = points[1];
		glm::int3 secondDest = points[2];
		glm::int3 combinedDest = points[3];

		switch(face) {
		case VisibleFaces::Top:
			return { firstDest + glm::int3(0, 1, 0), idx3D + glm::int3(0, 1, 0), secondDest + glm::int3(0, 1, 0), combinedDest + glm::int3(0, 1, 0) };
		case VisibleFaces::North:
			return { firstDest + glm::int3(0, 0, 1), combinedDest + glm::int3(0, 0, 1), secondDest + glm::int3(0, 0, 1), idx3D + glm::int3(0, 0, 1) };
		case VisibleFaces::East:
			return { idx3D + glm::int3(1, 0, 0), firstDest + glm::int3(1, 0, 0), combinedDest + glm::int3(1, 0, 0), secondDest + glm::int3(1, 0, 0) };
		case VisibleFaces::Bottom:
			return { secondDest, idx3D, firstDest, combinedDest };
		case VisibleFaces::South:
			return { firstDest, idx3D, secondDest, combinedDest };
		case VisibleFaces::West:
			return { idx3D, secondDest, combinedDest, firstDest };
		default:
			return { idx3D, idx3D, idx3D, idx3D };
		}
	}

	inline void AddQuad(const VisibleFaces face, const glm::int4x3 points, const glm::vec3 colour, glm::float4 ambientOcclusion,
		std::vector<VoxelVertex>& vertices, std::vector<uint32_t>& indices) {
		glm::int4x3 translatedPoints = GetPointsForFaces(face, points);
		for(int i = 0; i < 4; i++) {
			vertices.push_back({ translatedPoints[i], colour * ambientOcclusion[i] });
		}
		auto vertexCount = static_cast<uint32_t>(vertices.size());

		const bool flip = (ambientOcclusion.x + ambientOcclusion.z < ambientOcclusion.y + ambientOcclusion.w);
		indices.push_back(vertexCount - 4 + 0);
		indices.push_back(vertexCount - 4 + 1);
		flip ? indices.push_back(vertexCount - 4 + 3) : indices.push_back(vertexCount - 4 + 2);

		flip ? indices.push_back(vertexCount - 4 + 1) : indices.push_back(vertexCount - 4 + 0);
		indices.push_back(vertexCount - 4 + 2);
		indices.push_back(vertexCount - 4 + 3);
	}

	inline glm::float3 GetVoxelColour(MagicaVoxelModel& model, uint32_t index) {
		if(model.customPalette) return glm::float3{
				static_cast<float>(model.palette[model.voxels[index].colourIndex].r),
				static_cast<float>(model.palette[model.voxels[index].colourIndex].g),
				static_cast<float>(model.palette[model.voxels[index].colourIndex].b) } / 255.0f;
		else return ExtractRGBA(model.voxels[index].colourIndex) / 255.0f;
	}
	
	inline MagicaVoxelMesh VoxelModelMeshify(MagicaVoxelModel& model) {
		MagicaVoxelMesh mesh;

		// First set visible faces
		for(auto z = 0; z < model.extents.z; z++) {
			for(auto y = 0; y < model.extents.y; y++) {
				for(auto x = 0; x < model.extents.x; x++) {
					auto& voxel = model.voxels[ConvertIndex({ x,y,z }, model.extents)];

					// Skip transparent voxels
					if(!voxel.solid) continue;

					// Set all visible faces by default
					voxel.visibleFaces = (VisibleFaces::East | VisibleFaces::West | VisibleFaces::Top | VisibleFaces::Bottom | VisibleFaces::North | VisibleFaces::South);

					// Unset the faces that have a solid block next to them
					if(x + 1 < model.extents.x && model.voxels[ConvertIndex({ x + 1, y, z }, model.extents)].solid) voxel.visibleFaces &= ~VisibleFaces::East;
					if(x - 1 >= 0 && model.voxels[ConvertIndex({ x  - 1, y, z }, model.extents)].solid) voxel.visibleFaces &= ~VisibleFaces::West;
					if(y + 1 < model.extents.y && model.voxels[ConvertIndex({ x, y + 1, z }, model.extents)].solid) voxel.visibleFaces &= ~VisibleFaces::Top;
					if(y - 1 >= 0 && model.voxels[ConvertIndex({ x, y - 1, z }, model.extents)].solid) voxel.visibleFaces &= ~VisibleFaces::Bottom;
					if(z + 1 < model.extents.z && model.voxels[ConvertIndex({ x, y, z + 1 }, model.extents)].solid) voxel.visibleFaces &= ~VisibleFaces::North;
					if(z - 1 >= 0 && model.voxels[ConvertIndex({ x, y, z - 1 }, model.extents)].solid) voxel.visibleFaces &= ~VisibleFaces::South;
				}
			}
		}

		std::vector<bool> visited;

		uint64_t voxelBlockSize = static_cast<uint64_t>(model.extents.x) * static_cast<uint64_t>(model.extents.y) * static_cast<uint64_t>(model.extents.z);

		visited.resize(voxelBlockSize);

		for(uint32_t face = static_cast<uint32_t>(VisibleFaces::North); face <= static_cast<uint32_t>(VisibleFaces::Bottom); face *= 2) {
			visited.assign(voxelBlockSize, false);
			glm::int2x3 dirVectors = GetDirectionVectors(static_cast<VisibleFaces>(face));

			for(auto z = 0; z < model.extents.z; z++) {
				for(auto y = 0; y < model.extents.y; y++) {
					for(auto x = 0; x < model.extents.x; x++) {
						glm::int3 idx3D = { x, y, z };
						uint32_t idx1D = ConvertIndex(idx3D, model.extents);

						// Skip if face isn't visible
						if(static_cast<uint32_t>(model.voxels[idx1D].visibleFaces & static_cast<VisibleFaces>(face)) == 0) continue;
						// Skip if filltype is empty
						if(!model.voxels[idx1D].solid) continue;
						// Skip if voxel has been visited
						if(visited[idx1D]) continue;

						// Calculate ambient occlusion
						glm::float4 ao = CalculateFaceAmbientOcclusion(static_cast<VisibleFaces>(face), idx3D, model);

						// Calculate colour
						glm::vec3 colour = GetVoxelColour(model, idx1D);

						// Get axis endpoints
						glm::int2x3 axesEnds = GetAxisEndpoints(dirVectors, idx3D, model);

						// Run along the first axis until the endpoint is reached
						glm::int3 firstDest;
						for(firstDest = idx3D; firstDest != axesEnds[0]; firstDest += dirVectors[0]) {
							auto idx = ConvertIndex(firstDest, model.extents);
							// Make sure the voxel is visible and filled, and not visited previously, and that the AO values are the same as the current voxel
							if(static_cast<uint32_t>(model.voxels[idx].visibleFaces & static_cast<VisibleFaces>(face)) == 0 ||
								!model.voxels[idx].solid || visited[idx] ||
								ao != CalculateFaceAmbientOcclusion(static_cast<VisibleFaces>(face), firstDest, model) ||
								colour != GetVoxelColour(model, idx1D)) {
								break;
							}
							visited[idx] = true;
						}

						// Run along the second axis
						glm::int3 secondDest;
						glm::int3 combinedDest = firstDest + dirVectors[1];
						for(secondDest = idx3D + dirVectors[1]; secondDest != axesEnds[1]; secondDest += dirVectors[1], combinedDest += dirVectors[1]) {
							glm::int3 firstDirSubRun;

							for(firstDirSubRun = secondDest; firstDirSubRun != combinedDest; firstDirSubRun += dirVectors[0]) {
								auto idx = ConvertIndex(firstDirSubRun, model.extents);
								// Make sure the voxel is visible and filled, and not visited previously, and that the AO values are the same as the current voxel
								if(static_cast<uint32_t>(model.voxels[idx].visibleFaces & static_cast<VisibleFaces>(face)) == 0 ||
									!model.voxels[idx].solid || visited[idx] ||
									ao != CalculateFaceAmbientOcclusion(static_cast<VisibleFaces>(face), firstDirSubRun, model) ||
									colour != GetVoxelColour(model, idx1D)) {
									break;
								}
							}

							// If the sub run didn't end we don't have to add any visited voxels
							// Otherwise we have to add them
							if(firstDirSubRun != combinedDest) break;
							else {
								for(firstDirSubRun = secondDest; firstDirSubRun != combinedDest; firstDirSubRun += dirVectors[0]) {
									visited[ConvertIndex(firstDirSubRun, model.extents)] = true;
								}
							}
						}

						//glm::vec3 colour = glm::vec3(0.4f + (heightCoeff * 0.4f), 0.7f - (heightCoeff * 0.2f), 0.1f);
						AddQuad(static_cast<VisibleFaces>(face), { idx3D, firstDest, secondDest, combinedDest }, colour, ao, mesh.vertices, mesh.indices);
					}
				}
			}
		}

		return mesh;
	}
}