#pragma once

namespace keys {
	inline gfx::Point mouse_pos;
	inline std::unordered_set<std::uint8_t> pressed_mouse_keys;
	inline std::unordered_set<std::uint8_t> dragging_mouse_keys; // this feels wrong but it works
	inline std::unordered_set<std::uint8_t> pressing_keys;

	struct KeyPress {
		// os::KeyScancode scancode;
		// os::KeyModifiers modifiers;
		char unicode_char;
		int repeat;
	};

	inline std::vector<KeyPress> pressed_keys;

	inline float scroll_delta = 0.f;
	inline float scroll_delta_precise = 0.f;

	bool process_event(const SDL_Event& event);

	void on_mouse_press_handled(std::uint8_t button);

	bool is_rect_pressed(const gfx::Rect& rect, std::uint8_t button);
	bool is_mouse_down(std::uint8_t button = SDL_BUTTON_LEFT);
	bool is_mouse_dragging(std::uint8_t button = SDL_BUTTON_LEFT);
}
