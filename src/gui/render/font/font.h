#pragma once

#include <imgui.h>

namespace render {
	struct Font {
	private:
		ImFont* m_font{};
		float m_size{};
		bool m_initialized = false;
		int m_height = 0;

	public:
		bool init(
			unsigned char* data, size_t data_size, float size, ImFontConfig* font_cfg, const ImWchar* glyph_ranges
		);

		[[nodiscard]] gfx::Size calc_size(const std::string& text) const;

		[[nodiscard]] ImFont* im_font() const {
			return m_font;
		}

		[[nodiscard]] float size() const {
			return m_size;
		}

		operator bool() const {
			return m_initialized;
		}

		[[nodiscard]] int height() const {
			return m_height;
		}
	};
}
