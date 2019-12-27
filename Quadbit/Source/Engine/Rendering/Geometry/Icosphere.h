#pragma once
#include <cstdint>

#include <EASTL/hash_map.h>
#include <EASTL/array.h>
#include <EASTL/vector.h>

#include <glm/glm.hpp>

namespace Quadbit {
	constexpr float S = 0.5257311121191336f;
	constexpr float T = 0.85065080835204f;
	constexpr float U = 0.0f;

	struct IcosphereVertex {
		glm::vec3 position;
	};

	struct PairHash {
	public:
		template <typename T, typename U>
		size_t operator()(const eastl::pair<T, U>& x) const
		{
			auto firstHash = eastl::hash<T>()(x.first);
			return  (firstHash << 1) + firstHash + eastl::hash<U>()(x.second);
		}
	};

	struct Icosphere {
	public:
		Icosphere(uint32_t subdivisions) {
			auto subdivide = [this]() {
				eastl::hash_map<eastl::pair<uint32_t, uint32_t>, uint32_t, PairHash> edgeLookup;

				auto divideVertex = [&](uint32_t v1, uint32_t v2) {
					eastl::pair<uint32_t, uint32_t> vertexPair = { v1, v2 };
					if (vertexPair.first > vertexPair.second) {
						eastl::swap(vertexPair.first, vertexPair.second);
					}
					auto inserted = edgeLookup.insert({ vertexPair, static_cast<uint32_t>(vertices_.size()) });
					if (inserted.second) {
						vertices_.push_back({ glm::normalize(vertices_[v1].position + vertices_[v2].position) });
					}

					return inserted.first->second;
				};

				auto size = indices_.size();
				for (auto i = 0; i < size; i += 3) {
					uint32_t mid1 = divideVertex(indices_[i + 0], indices_[i + 1]);
					uint32_t mid2 = divideVertex(indices_[i + 1], indices_[i + 2]);
					uint32_t mid3 = divideVertex(indices_[i + 2], indices_[i + 0]);

					indices_.insert(indices_.end(), { indices_[i + 0], mid1, mid3 });
					indices_.insert(indices_.end(), { indices_[i + 1], mid2, mid1 });
					indices_.insert(indices_.end(), { indices_[i + 2], mid3, mid2 });
					indices_.insert(indices_.end(), { mid1, mid2, mid3 });
				}
			};

			for (uint32_t i = 0; i < subdivisions; i++) {
				subdivide();
			}
		}

		eastl::vector<IcosphereVertex> vertices_ = {
			{{-S,  U,  T}}, {{ S,  U,  T}}, {{-S,  U, -T}}, {{ S,  U, -T}},
			{{ U,  T,  S}}, {{ U,  T, -S}}, {{ U, -T,  S}}, {{ U, -T, -S}},
			{{ T,  S,  U}}, {{-T,  S,  U}}, {{ T, -S,  U}}, {{-T, -S,  U}}
		};

		eastl::vector<uint32_t> indices_ = {
			0, 4, 1,  0, 9, 4,  9, 5, 4,  4, 5, 8,  4, 8, 1,
			8, 10, 1, 8, 3, 10, 5, 3, 8,  5, 2, 3,  2, 7, 3,
			7, 10, 3, 7, 6, 10, 7, 11, 6, 11, 0, 6, 0, 1, 6,
			6, 1, 10, 9, 0, 11, 9, 11, 2, 9, 2, 5,  7, 2, 11
		};
	};
}