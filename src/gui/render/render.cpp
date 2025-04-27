#include "render.h"

#include <imgui.h>
#include <backends/imgui_impl_sdl3.h>
#include <backends/imgui_impl_opengl3.h>
#include <misc/freetype/imgui_freetype.h>

#include "../fonts/dejavu_sans.h"
#include "../fonts/eb_garamond.h"

#ifndef M_PI
#	define M_PI 3.1415926535897932384626433832
#endif

template<typename T>
static constexpr T rad_to_deg(T radian) {
	return radian * (180.f / M_PI);
}

template<typename T>
static constexpr T deg_to_rad(T degree) {
	return static_cast<T>(degree * (M_PI / 180.f));
}

gfx::Color interpolate_color(const std::vector<gfx::Color>& colors, const std::vector<float>& positions, float t) {
	// Find the segment containing t
	size_t i = 0;
	for (; i < positions.size() - 1; i++) {
		if (t >= positions[i] && t <= positions[i + 1])
			break;
	}

	// If t is outside the range, clamp to the nearest color
	if (i >= positions.size() - 1) {
		if (t < positions[0])
			return colors[0];
		return colors[colors.size() - 1];
	}

	// Normalize t to the segment
	float segment_t = (t - positions[i]) / (positions[i + 1] - positions[i]);

	// Linear interpolation between colors
	const gfx::Color& c1 = colors[i];
	const gfx::Color& c2 = colors[i + 1];

	return gfx::Color(
		c1.r + segment_t * (c2.r - c1.r),
		c1.g + segment_t * (c2.g - c1.g),
		c1.b + segment_t * (c2.b - c1.b),
		c1.a + segment_t * (c2.a - c1.a)
	);
}

bool render::ImGuiWrap::init(SDL_Window* window, const SDL_GLContext& context) {
	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ctx = ImGui::CreateContext();
	io = &ImGui::GetIO();
	io->IniFilename = nullptr;
	io->LogFilename = nullptr;

	// Setup Dear ImGui style
	ImGui::StyleColorsDark();

	// Setup Platform/Renderer backends
	// Decide GL+GLSL versions
#if defined(IMGUI_IMPL_OPENGL_ES2)
	// GL ES 2.0 + GLSL 100 (WebGL 1.0)
	const char* glsl_version = "#version 100";
#elif defined(IMGUI_IMPL_OPENGL_ES3)
	// GL ES 3.0 + GLSL 300 es (WebGL 2.0)
	const char* glsl_version = "#version 300 es";
#elif defined(__APPLE__)
	// GL 3.2 Core + GLSL 150
	const char* glsl_version = "#version 150";
#else
	// GL 3.0 + GLSL 130
	const char* glsl_version = "#version 130";
#endif

	ImGui_ImplSDL3_InitForOpenGL(window, context);
	ImGui_ImplOpenGL3_Init(glsl_version);

	return true;
}

bool render::init(SDL_Window* window, const SDL_GLContext& context) {
	if (!imgui.init(window, context))
		return false;

	// Get the window display scale to adjust font sizes and styling
	dpi_scale = SDL_GetWindowDisplayScale(window);

	// Setup ImGui style scaling (only needed for ImGui widgets)
	ImGui::StyleColorsDark();
	ImGui::GetStyle().ScaleAllSizes(dpi_scale);

	// The rest of your init code...
	const auto* glyph_ranges = imgui.io->Fonts->GetGlyphRangesDefault();

	ImFontConfig font_cfg;
	// Use the window's pixel density directly
	font_cfg.RasterizerDensity = SDL_GetWindowPixelDensity(window);

	// Init fonts with the correct scaling - maintain your original sizes here
	float base_font_size = 13.0f;
	float header_font_size = 30.0f;
	float smaller_header_font_size = 18.0f;

	// Fonts still need DPI scaling as they're rendered at a fixed size
	if (!fonts::dejavu.init(
			DejaVuSans_compressed_data, DejaVuSans_compressed_size, base_font_size * dpi_scale, &font_cfg, glyph_ranges
		))
		return false;

	if (!fonts::header_font.init(
			EbGaramond_compressed_data,
			EbGaramond_compressed_size,
			header_font_size * dpi_scale,
			&font_cfg,
			glyph_ranges
		))
		return false;

	if (!fonts::smaller_header_font.init(
			EbGaramond_compressed_data,
			EbGaramond_compressed_size,
			smaller_header_font_size * dpi_scale,
			&font_cfg,
			glyph_ranges
		))
		return false;

	return true;
}

void render::destroy() {
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplSDL3_Shutdown();
	ImGui::DestroyContext();
}

void render::update_window_size(SDL_Window* window) {
	SDL_GetWindowSize(window, &window_size.w, &window_size.h);
}

void render::ImGuiWrap::begin(SDL_Window* window) {
	// Start the Dear ImGui frame
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplSDL3_NewFrame();
	ImGui::NewFrame();

	drawlist = ImGui::GetForegroundDrawList();

	// calculate frametime
	auto now = std::chrono::high_resolution_clock::now();
	static auto last = now;

	frametime = std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(now - last).count() / 1000.f;

	last = now;
}

void render::ImGuiWrap::end(SDL_Window* window) {
	ImVec4 clear_colour = ImVec4(0.f, 0.f, 0.f, 1.f);

	ImGui::Render();

	int drawable_width = 0;
	int drawable_height = 0;
	SDL_GetWindowSizeInPixels(window, &drawable_width, &drawable_height);

	glViewport(0, 0, drawable_width, drawable_height);

	glClearColor(
		clear_colour.x * clear_colour.w,
		clear_colour.y * clear_colour.w,
		clear_colour.z * clear_colour.w,
		clear_colour.w
	);
	glClear(GL_COLOR_BUFFER_BIT);
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
	SDL_GL_SwapWindow(window);
}

void render::line(
	const gfx::Point& pos1, const gfx::Point& pos2, const gfx::Color& col, bool anti_aliased, float thickness
) {
	// Scale points and thickness
	gfx::Point p1 = scale_point(pos1);
	gfx::Point p2 = scale_point(pos2);
	imgui.drawlist->AddLine(p1, p2, col.to_imgui(), thickness * dpi_scale);
}

void render::rect_filled(const gfx::Rect& rect, const gfx::Color& col) {
	gfx::Rect scaled_rect = scale_rect(rect);
	imgui.drawlist->AddRectFilled(scaled_rect.origin(), scaled_rect.max(), col.to_imgui());
}

void render::rect_stroke(const gfx::Rect& rect, const gfx::Color& col, float thickness) {
	gfx::Rect scaled_rect = scale_rect(rect);
	imgui.drawlist->AddRect(scaled_rect.origin(), scaled_rect.max(), col.to_imgui(), 0.f, 0, thickness * dpi_scale);
}

void render::rounded_rect_filled(
	const gfx::Rect& rect, const gfx::Color& col, float rounding, unsigned int rounding_flags
) {
	gfx::Rect scaled_rect = scale_rect(rect);
	imgui.drawlist->AddRectFilled(
		scaled_rect.origin(), scaled_rect.max(), col.to_imgui(), rounding * dpi_scale, rounding_flags
	);
}

void render::rounded_rect_stroke(
	const gfx::Rect& rect, const gfx::Color& col, float rounding, unsigned int rounding_flags, float thickness
) {
	gfx::Rect scaled_rect = scale_rect(rect);
	imgui.drawlist->AddRect(
		scaled_rect.origin(),
		scaled_rect.max(),
		col.to_imgui(),
		rounding * dpi_scale,
		rounding_flags,
		thickness * dpi_scale
	);
}

void render::rect_filled_gradient(
	const gfx::Rect& rect,
	const std::vector<gfx::Point>& gradient_direction,
	const std::vector<gfx::Color>& colors,
	const std::vector<float>& positions
) {
	// Validate inputs
	if (colors.size() < 2 || colors.size() != positions.size() || gradient_direction.size() != 2) {
		return; // Invalid input parameters
	}

	// Scale the rectangle and direction points
	gfx::Rect scaled_rect = scale_rect(rect);
	std::vector<gfx::Point> scaled_direction = { scale_point(gradient_direction[0]),
		                                         scale_point(gradient_direction[1]) };

	// For custom gradient direction, we need to split the rectangle into multiple triangles
	ImVec2 origin = scaled_rect.origin();
	ImVec2 max = scaled_rect.max();

	// Calculate direction vector
	ImVec2 dir_vec(scaled_direction[1].x - scaled_direction[0].x, scaled_direction[1].y - scaled_direction[0].y);
	float dir_length = sqrtf(dir_vec.x * dir_vec.x + dir_vec.y * dir_vec.y);

	if (dir_length < 0.0001f) {
		return; // Invalid direction
	}

	// Normalize direction vector
	dir_vec.x /= dir_length;
	dir_vec.y /= dir_length;

	// Calculate perpendicular vector
	ImVec2 perp_vec(-dir_vec.y, dir_vec.x);

	// Create gradient mesh using triangles
	const int segments = 32; // Number of segments for smooth gradient

	for (int i = 0; i < segments; i++) {
		float t1 = static_cast<float>(i) / segments;
		float t2 = static_cast<float>(i + 1) / segments;

		// Find color at position t1 and t2
		ImU32 col1 = interpolate_color(colors, positions, t1).to_imgui();
		ImU32 col2 = interpolate_color(colors, positions, t2).to_imgui();

		// Calculate points for this segment
		ImVec2 p1 = ImVec2(
			origin.x + dir_vec.x * t1 * (max.x - origin.x) + perp_vec.x * origin.y,
			origin.y + dir_vec.y * t1 * (max.y - origin.y) + perp_vec.y * origin.x
		);
		ImVec2 p2 = ImVec2(
			origin.x + dir_vec.x * t2 * (max.x - origin.x) + perp_vec.x * origin.y,
			origin.y + dir_vec.y * t2 * (max.y - origin.y) + perp_vec.y * origin.x
		);
		ImVec2 p3 = ImVec2(
			max.x + dir_vec.x * t1 * (max.x - origin.x) + perp_vec.x * max.y,
			max.y + dir_vec.y * t1 * (max.y - origin.y) + perp_vec.y * max.x
		);
		ImVec2 p4 = ImVec2(
			max.x + dir_vec.x * t2 * (max.x - origin.x) + perp_vec.x * max.y,
			max.y + dir_vec.y * t2 * (max.y - origin.y) + perp_vec.y * max.x
		);

		// Draw triangles
		imgui.drawlist->AddTriangleFilled(p1, p2, p3, col1);
		imgui.drawlist->AddTriangleFilled(p2, p3, p4, col2);
	}
}

void render::rect_filled_gradient(
	const gfx::Rect& rect,
	GradientDirection direction,
	const std::vector<gfx::Color>& colors,
	const std::vector<float>& positions
) {
	// Validate inputs
	if (colors.size() < 2 || colors.size() != positions.size()) {
		return; // Invalid input parameters
	}

	gfx::Rect scaled_rect = scale_rect(rect);
	ImVec2 origin = scaled_rect.origin();
	ImVec2 max = scaled_rect.max();

	// For predefined directions, we can use the ImGui built-in functions
	if (colors.size() == 2) {
		ImU32 col1 = colors[0].to_imgui();
		ImU32 col2 = colors[1].to_imgui();

		switch (direction) {
			case GradientDirection::GRADIENT_RIGHT:
				imgui.drawlist->AddRectFilledMultiColor(origin, max, col1, col2, col2, col1);
				break;
			case GradientDirection::GRADIENT_LEFT:
				imgui.drawlist->AddRectFilledMultiColor(origin, max, col2, col1, col1, col2);
				break;
			case GradientDirection::GRADIENT_DOWN:
				imgui.drawlist->AddRectFilledMultiColor(origin, max, col1, col1, col2, col2);
				break;
			case GradientDirection::GRADIENT_UP:
				imgui.drawlist->AddRectFilledMultiColor(origin, max, col2, col2, col1, col1);
				break;
			case GradientDirection::GRADIENT_DOWN_RIGHT:
				imgui.drawlist->AddRectFilledMultiColor(origin, max, col1, col2, col2, col1);
				break;
			case GradientDirection::GRADIENT_UP_LEFT:
				imgui.drawlist->AddRectFilledMultiColor(origin, max, col2, col1, col1, col2);
				break;
			case GradientDirection::GRADIENT_DOWN_LEFT:
				imgui.drawlist->AddRectFilledMultiColor(origin, max, col2, col1, col2, col1);
				break;
			case GradientDirection::GRADIENT_UP_RIGHT:
				imgui.drawlist->AddRectFilledMultiColor(origin, max, col1, col2, col1, col2);
				break;
		}
	}
	else {
		// For more than 2 colors, we need a more complex approach
		// Convert direction to points for the general implementation
		std::vector<gfx::Point> gradient_direction;

		// These are in logical coordinates, will be scaled in rect_filled_gradient
		switch (direction) {
			case GradientDirection::GRADIENT_RIGHT:
				gradient_direction = { gfx::Point(rect.origin().x, rect.origin().y),
					                   gfx::Point(rect.max().x, rect.origin().y) };
				break;
			case GradientDirection::GRADIENT_LEFT:
				gradient_direction = { gfx::Point(rect.max().x, rect.origin().y),
					                   gfx::Point(rect.origin().x, rect.origin().y) };
				break;
			case GradientDirection::GRADIENT_DOWN:
				gradient_direction = { gfx::Point(rect.origin().x, rect.origin().y),
					                   gfx::Point(rect.origin().x, rect.max().y) };
				break;
			case GradientDirection::GRADIENT_UP:
				gradient_direction = { gfx::Point(rect.origin().x, rect.max().y),
					                   gfx::Point(rect.origin().x, rect.origin().y) };
				break;
			case GradientDirection::GRADIENT_DOWN_RIGHT:
				gradient_direction = { gfx::Point(rect.origin().x, rect.origin().y),
					                   gfx::Point(rect.max().x, rect.max().y) };
				break;
			case GradientDirection::GRADIENT_UP_LEFT:
				gradient_direction = { gfx::Point(rect.max().x, rect.max().y),
					                   gfx::Point(rect.origin().x, rect.origin().y) };
				break;
			case GradientDirection::GRADIENT_DOWN_LEFT:
				gradient_direction = { gfx::Point(rect.max().x, rect.origin().y),
					                   gfx::Point(rect.origin().x, rect.max().y) };
				break;
			case GradientDirection::GRADIENT_UP_RIGHT:
				gradient_direction = { gfx::Point(rect.origin().x, rect.max().y),
					                   gfx::Point(rect.max().x, rect.origin().y) };
				break;
		}

		// Use the general implementation
		rect_filled_gradient(rect, gradient_direction, colors, positions);
	}
}

void render::rect_gradient_multi_filled(
	const gfx::Rect& rect,
	const gfx::Color& col_top_left,
	const gfx::Color& col_top_right,
	const gfx::Color& col_bottom_left,
	const gfx::Color& col_bottom_right
) {
	gfx::Rect scaled_rect = scale_rect(rect);

	// Stroke is just for debugging, keeping it here
	imgui.drawlist->AddRect(
		scaled_rect.top_left(), scaled_rect.bottom_right(), col_top_left.to_imgui(), 0.0f, 0, dpi_scale
	);

	imgui.drawlist->AddRectFilledMultiColor(
		scaled_rect.top_left(),
		scaled_rect.bottom_right(),
		col_top_left.to_imgui(),
		col_top_right.to_imgui(),
		col_bottom_right.to_imgui(),
		col_bottom_left.to_imgui()
	);
}

void render::quadrilateral_filled(
	const gfx::Point& bottom_left,
	const gfx::Point& bottom_right,
	const gfx::Point& top_left,
	const gfx::Point& top_right,
	const gfx::Color& col
) {
	ImVec2 positions[5] = { scale_point(top_left),
		                    scale_point(bottom_left),
		                    scale_point(bottom_right),
		                    scale_point(top_right),
		                    scale_point(top_left) };

	imgui.drawlist->AddConvexPolyFilled(positions, 5, col.to_imgui());
}

void render::quadrilateral_stroke(
	const gfx::Point& bottom_left,
	const gfx::Point& bottom_right,
	const gfx::Point& top_left,
	const gfx::Point& top_right,
	const gfx::Color& col,
	float thickness
) {
	ImVec2 positions[5] = { scale_point(top_left),
		                    scale_point(bottom_left),
		                    scale_point(bottom_right),
		                    scale_point(top_right),
		                    scale_point(top_left) };

	imgui.drawlist->AddPolyline(positions, 5, col.to_imgui(), ImDrawFlags_Closed, thickness * dpi_scale);
}

void render::triangle_filled(
	const gfx::Point& p1,
	const gfx::Point& p2,
	const gfx::Point& p3,
	const gfx::Color& col,
	float thickness,
	bool anti_aliased
) {
	imgui.drawlist->AddTriangleFilled(scale_point(p1), scale_point(p2), scale_point(p3), col.to_imgui());
}

void render::triangle_stroke(
	const gfx::Point& p1,
	const gfx::Point& p2,
	const gfx::Point& p3,
	const gfx::Color& col,
	float thickness,
	bool anti_aliased
) {
	imgui.drawlist->AddTriangle(
		scale_point(p1), scale_point(p2), scale_point(p3), col.to_imgui(), thickness * dpi_scale
	);
}

void render::circle_filled(
	const gfx::Point& pos,
	float radius,
	const gfx::Color& colour,
	float thickness,
	int parts,
	float degrees,
	float start_degree,
	bool antialiased
) {
	gfx::Point scaled_pos = scale_point(pos);
	float scaled_radius = radius * dpi_scale;

	imgui.drawlist->AddCircleFilled(scaled_pos, scaled_radius, colour.to_imgui(), parts);
}

void render::circle_stroke(
	const gfx::Point& pos,
	float radius,
	const gfx::Color& colour,
	float thickness,
	int parts,
	float degrees,
	float start_degree,
	bool antialiased
) {
	gfx::Point scaled_pos = scale_point(pos);
	float scaled_radius = radius * dpi_scale;

	if (!(degrees == 360.f && start_degree == 0.f)) {
		auto min_rad = deg_to_rad(start_degree);
		auto max_rad = deg_to_rad(degrees);
		imgui.drawlist->PathArcTo(scaled_pos, scaled_radius, min_rad, max_rad, parts);
		imgui.drawlist->PathStroke(colour.to_imgui(), 0, thickness * dpi_scale);
		return;
	}

	imgui.drawlist->AddCircle(scaled_pos, scaled_radius, colour.to_imgui(), parts, thickness * dpi_scale);
}

void render::text(
	gfx::Point pos, const gfx::Color& colour, const std::string& text, const Font& font, unsigned int flags
) {
	if (!font)
		return;

	const auto* text_char = text.data();
	bool outline = false;

	// Scale position
	gfx::Point scaled_pos = scale_point(pos);

	ImGui::PushFont(font.im_font());

	if (flags) {
		const auto size = font.calc_size(text);

		if (flags & FONT_CENTERED_X)
			scaled_pos.x -= int(size.w * 0.5f * dpi_scale);

		if (flags & FONT_CENTERED_Y)
			scaled_pos.y -= int(size.h * 0.5f * dpi_scale);

		if (flags & FONT_RIGHT_ALIGN)
			scaled_pos.x -= size.w * dpi_scale;

		if (flags & FONT_BOTTOM_ALIGN)
			scaled_pos.y -= size.h * dpi_scale;

		if (flags & FONT_OUTLINE)
			outline = true;

		if (flags & FONT_DROPSHADOW) {
			const gfx::Color dropshadow_colour(0, 0, 0, colour.a * 0.6f);
			const float shift_amount = (outline ? 2 : 1) * dpi_scale;

			imgui.drawlist->AddText(
				font.im_font(),
				font.size(),
				scaled_pos.offset(shift_amount, shift_amount),
				dropshadow_colour.to_imgui(),
				text.data()
			);
		}
	}

	imgui.drawlist->AddText(font.im_font(), font.size(), scaled_pos, colour.to_imgui(), text.data());

	ImGui::PopFont();
}

render::Texture::~Texture() {
	destroy();
}

bool render::Texture::load_from_file(const std::string& path) {
	// Use SDL to load the image
	SDL_Surface* surface = IMG_Load(path.c_str());

	if (!surface) {
		u::log_error("Failed to load image: {}", SDL_GetError());
		return false;
	}

	// Convert to RGBA format if needed
	SDL_Surface* rgb_surface = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_RGBA32);
	SDL_DestroySurface(surface);

	if (!rgb_surface) {
		u::log_error("Failed to convert surface: {}", SDL_GetError());
		return false;
	}

	bool result = load_from_surface(rgb_surface);
	SDL_DestroySurface(rgb_surface);

	return result;
}

bool render::Texture::load_from_surface(SDL_Surface* surface) {
	if (!surface)
		return false;

	// Delete old texture if exists
	destroy();

	// Create a new texture
	glGenTextures(1, &m_id);
	glBindTexture(GL_TEXTURE_2D, m_id);

	// Setup texture parameters
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	// Upload pixels into texture
	GLenum format = GL_RGBA;
	glTexImage2D(GL_TEXTURE_2D, 0, format, surface->w, surface->h, 0, format, GL_UNSIGNED_BYTE, surface->pixels);

	m_width = surface->w;
	m_height = surface->h;

	glBindTexture(GL_TEXTURE_2D, 0);

	return true;
}

void render::Texture::destroy() {
	if (m_id) {
		glDeleteTextures(1, &m_id);
		m_id = 0;
		m_width = 0;
		m_height = 0;
	}
}

void render::Texture::bind() const {
	glBindTexture(GL_TEXTURE_2D, m_id);
}

void render::Texture::unbind() const {
	glBindTexture(GL_TEXTURE_2D, 0);
}

// Image rendering functions
void render::image(const gfx::Rect& rect, const Texture& texture, const gfx::Color& tint_color) {
	if (!texture.is_valid())
		return;

	gfx::Rect scaled_rect = scale_rect(rect);

	imgui.drawlist->AddImage(
		texture.get_id(), scaled_rect.origin(), scaled_rect.max(), ImVec2(0, 0), ImVec2(1, 1), tint_color.to_imgui()
	);
}

void render::image_rounded(
	const gfx::Rect& rect,
	const Texture& texture,
	float rounding,
	unsigned int rounding_flags,
	const gfx::Color& tint_color
) {
	if (!texture.is_valid())
		return;

	gfx::Rect scaled_rect = scale_rect(rect);

	imgui.drawlist->AddImageRounded(
		texture.get_id(),
		scaled_rect.origin(),
		scaled_rect.max(),
		ImVec2(0, 0),
		ImVec2(1, 1),
		tint_color.to_imgui(),
		rounding * dpi_scale,
		rounding_flags
	);
}

void render::image_with_borders(
	const gfx::Rect& rect,
	const Texture& texture,
	float rounding,
	const gfx::Color& border_color,
	float border_thickness,
	unsigned int rounding_flags,
	const gfx::Color& tint_color
) {
	if (!texture.is_valid())
		return;

	gfx::Rect inner_rect = rect.shrink(1);
	gfx::Rect scaled_inner_rect = scale_rect(inner_rect);
	gfx::Rect scaled_rect = scale_rect(rect);

	image_rounded(inner_rect, texture, rounding, rounding_flags, tint_color);

	rounded_rect_stroke(rect, border_color, rounding, rounding_flags, border_thickness);
}

void render::push_clip_rect(const gfx::Rect& rect, bool intersect_clip_rect) {
	gfx::Rect scaled_rect = scale_rect(rect);
	imgui.drawlist->PushClipRect(scaled_rect.origin(), scaled_rect.max(), intersect_clip_rect);
}

void render::push_fullscreen_clip_rect() {
	push_clip_rect(gfx::Rect(gfx::Point(0, 0), window_size), false);
}

gfx::Rect render::pop_clip_rect() {
	// Get current before popping
	ImVec2 min = imgui.drawlist->GetClipRectMin();
	ImVec2 max = imgui.drawlist->GetClipRectMax();
	imgui.drawlist->PopClipRect();

	// Return unscaled rect
	return gfx::Rect(unscale_point(gfx::Point(min.x, min.y)), unscale_point(gfx::Point(max.x, max.y)));
}

gfx::Rect render::get_clip_rect() {
	ImVec2 min = imgui.drawlist->GetClipRectMin();
	ImVec2 max = imgui.drawlist->GetClipRectMax();

	// Return unscaled rect
	return gfx::Rect(unscale_point(gfx::Point(min.x, min.y)), unscale_point(gfx::Point(max.x, max.y)));
}

bool render::clip_string(std::string& text, const Font& font, int max_width, int min_chars) {
	// Convert max_width to scaled width for font calculations
	int scaled_max_width = max_width * dpi_scale;

	int size = font.calc_size(text).w;
	if (size <= scaled_max_width)
		return true;

	int dots_size = font.calc_size("...").w;

	const int num = static_cast<int>(text.size()) - min_chars;
	for (int i = 0; i < num; i++) {
		text.pop_back();

		if (font.calc_size(text).w + dots_size <= scaled_max_width) {
			text += "...";
			return true;
		}
	}

	text += "...";
	return false;
}

std::vector<std::string> render::wrap_text(
	const std::string& text, const gfx::Size& dimensions, const Font& font, int line_height
) {
	std::vector<std::string> lines;
	if (!font)
		return lines;

	// Scale dimensions for font calculations
	gfx::Size scaled_dimensions = scale_size(dimensions);

	std::istringstream iss(text);
	std::string word;
	std::string current_line;

	while (iss >> word) {
		std::string test_line = current_line.empty() ? word : current_line + " " + word;
		if (font.calc_size(test_line).w > scaled_dimensions.w) {
			if (!current_line.empty()) {
				lines.push_back(current_line);
				current_line = word;
			}
			else {
				// Word itself is too long, hard break
				std::string sub_word;
				for (char c : word) {
					sub_word += c;
					if (font.calc_size(sub_word).w > scaled_dimensions.w) {
						if (sub_word.length() > 1) {
							lines.push_back(sub_word.substr(0, sub_word.length() - 1));
							sub_word = sub_word.back();
						}
					}
				}
				current_line = sub_word;
			}
		}
		else {
			current_line = test_line;
		}
	}

	if (!current_line.empty()) {
		lines.push_back(current_line);
	}

	return lines;
}

// // Handle DPI changes during runtime
// void render::handle_dpi_changed(SDL_Window* window) {
// 	float old_scale = dpi_scale;
// 	float new_scale = SDL_GetWindowDisplayScale(window);

// 	if (fabs(old_scale - new_scale) < 0.001f) {
// 		return; // No significant change
// 	}

// 	// Update the scale factor
// 	set_dpi_scale(new_scale);

// 	// You may need to rebuild fonts if the scale change is significant
// 	const float scale_ratio = new_scale / old_scale;

// 	// Update ImGui style
// 	ImGui::GetStyle().ScaleAllSizes(scale_ratio);

// 	// If the scale change is significant, you might want to rebuild fonts
// 	if (fabs(scale_ratio - 1.0f) > 0.1f) {
// 		// You would need to implement a font rebuild mechanism here
// 		// This might involve clearing the font atlas and rebuilding with new sizes
// 	}
// }
