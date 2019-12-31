#pragma once

#include <glm/glm.hpp>

namespace Quadbit {
	inline glm::mat4 MakeFrustumProjection(float fovy, float aspectRatio, float zNear, float zFar) {
		float g = 1.0f / tan(glm::radians(fovy) * 0.5f);
		float k = zFar / (zFar - zNear);
		return glm::mat4{
			{ g / aspectRatio, 0.0f,	0.0f,		 0.0f },
			{ 0.0f,			   g,		0.0f,		 0.0f},
			{ 0.0f,			   0.0f,	k,			 1.0f},
			{ 0.0f,			   0.0f,	-zNear * k,	 0.0f}
		};
	}

	inline glm::mat4 MakeInfiniteProjection(float fovy, float aspectRatio, float zNear, float e) {
		float g = 1.0f / tan(glm::radians(fovy) * 0.5f);
		e = 1.0f - e;
		return glm::mat4{
			{ g / aspectRatio, 0.0f,	0.0f,		 0.0f },
			{ 0.0f,			   g,		0.0f,		 0.0f},
			{ 0.0f,			   0.0f,	e,			 1.0f},
			{ 0.0f,			   0.0f,	-zNear * e,	 0.0f}
		};
	}

	inline glm::mat4 MakeRevInfiniteProjection(float fovy, float aspectRatio, float zNear, float e) {
		float g = 1.0f / tan(glm::radians(fovy) * 0.5f);
		return glm::mat4{
			{ g / aspectRatio, 0.0f,	0.0f,					 0.0f },
			{ 0.0f,			   g,		0.0f,					 0.0f},
			{ 0.0f,			   0.0f,	e,					     1.0f},
			{ 0.0f,			   0.0f,	-zNear * (1.0f - e),	 0.0f}
		};
	}

}