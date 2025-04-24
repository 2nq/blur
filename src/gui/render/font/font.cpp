#include "font.h"

#include <imgui.h>
#include <misc/freetype/imgui_freetype.h>

bool render::Font::init(
	unsigned char* data, size_t data_size, float size, ImFontConfig* font_cfg, const ImWchar* glyph_ranges
) {
	ImGuiIO* io = &ImGui::GetIO();
	m_size = size;

	ImFontConfig cfg = ImFontConfig();
	if (!font_cfg)
		font_cfg = &cfg;

	font_cfg->FontDataOwnedByAtlas = false;

	m_font = io->Fonts->AddFontFromMemoryCompressedTTF(data, data_size, m_size, font_cfg, glyph_ranges);

	m_height = calc_size("Q").h;

	m_initialized = m_font != nullptr;
	return m_initialized;
}

gfx::Size render::Font::calc_size(const std::string& text) const {
	auto size = m_font->CalcTextSizeA(m_size, FLT_MAX, 0.f, text.c_str());
	return { (int)size.x, (int)size.y };
}
