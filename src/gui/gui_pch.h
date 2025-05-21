#pragma once

// NOLINTEND(misc-include-cleaner)
#include <common/common_pch.h>

#include "render/primitives/color.h"
#include "render/primitives/point.h"
#include "render/primitives/rect.h"
#include "render/primitives/size.h"
#include "render/primitives/primitives_impl.h"

#include <glad/glad.h>

#define IMGUI_USER_CONFIG "gui/render/imconfig.h"
#define IMGUI_DEFINE_PLACEMENT_NEW
#define IMGUI_DEFINE_MATH_OPERATORS

#include <imgui.h>
#include <backends/imgui_impl_sdl3.h>
#include <backends/imgui_impl_opengl3.h>
#include <misc/freetype/imgui_freetype.h>

#include <SDL3/SDL.h>
#if defined(IMGUI_IMPL_OPENGL_ES2)
#	include <SDL3/SDL_opengles2.h>
#else
#	include <SDL3/SDL_opengl.h>
#endif

// #include <SDL3/SDL_opengl.h>
#include <SDL3_image/SDL_image.h>
