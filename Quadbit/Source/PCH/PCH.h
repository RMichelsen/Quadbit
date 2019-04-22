#pragma once

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <Windowsx.h>

#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <ImGui/imgui.h>

#include "../Engine/Application/Logging.h"

#include <vector>
#include <deque>
#include <forward_list>
#include <array>
#include <chrono>
#include <algorithm>
#include <execution>