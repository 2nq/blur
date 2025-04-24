#pragma once

#include <common/common_pch.h>

#include <boost/process.hpp>
#include <boost/asio.hpp>
#ifdef _WIN32
#	include <boost/process/v1/windows.hpp>
#endif

// NOLINTEND(misc-include-cleaner)

#include "render/primitives/color.h"
#include "render/primitives/point.h"
#include "render/primitives/rect.h"
#include "render/primitives/size.h"
#include "render/primitives/primitives_impl.h"

#include <glad/glad.h>

#include <SDL3/SDL.h>
// #include <SDL3/SDL_opengl.h>

#define IMGUI_USER_CONFIG "gui/render/imconfig.h"
