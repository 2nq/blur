#include <algorithm>

#include <algorithm>

#include "../ui.h"
#include "../../render/render.h"
#include "../keys.h"

#define STB_TEXTEDIT_CHARTYPE       char
#define STB_TEXTEDIT_POSITIONTYPE   int
#define STB_TEXTEDIT_UNDOSTATECOUNT 99
#define STB_TEXTEDIT_UNDOCHARCOUNT  999

#define STB_TEXTEDIT_STRING          ui::TextInputElementData                  // Use the forward-declared type
#define STB_TEXTEDIT_STRINGLEN(obj)  ((obj)->text ? (obj)->text->length() : 0) // Add safety check
#define STB_TEXTEDIT_LAYOUTROW       textedit_layoutrow
#define STB_TEXTEDIT_GETWIDTH        textedit_getwidth
#define STB_TEXTEDIT_KEYTOTEXT       textedit_keytotext
#define STB_TEXTEDIT_GETCHAR(obj, i) ((*(obj)->text)[(i)]) // Assumes text is valid if called
#define STB_TEXTEDIT_NEWLINE         '\n'
#define STB_TEXTEDIT_DELETECHARS     textedit_deletechars
#define STB_TEXTEDIT_INSERTCHARS(text_data, pos, newtext, newtextlen)                                                  \
	textedit_insertchars(text_data, pos, newtext, newtextlen)

// Key mapping (Use SDL_SCANCODE_, combine modifiers in event handler)
#define STB_TEXTEDIT_K_SHIFT     (1 << 30) // Use a high bit for Shift flag internally if needed by STB
#define STB_TEXTEDIT_K_CTRL      (1 << 29) // Use a high bit for Ctrl flag internally if needed by STB
#define STB_TEXTEDIT_K_LEFT      SDL_SCANCODE_LEFT
#define STB_TEXTEDIT_K_RIGHT     SDL_SCANCODE_RIGHT
#define STB_TEXTEDIT_K_UP        SDL_SCANCODE_UP
#define STB_TEXTEDIT_K_DOWN      SDL_SCANCODE_DOWN
#define STB_TEXTEDIT_K_PGUP      SDL_SCANCODE_PAGEUP
#define STB_TEXTEDIT_K_PGDOWN    SDL_SCANCODE_PAGEDOWN
#define STB_TEXTEDIT_K_LINESTART SDL_SCANCODE_HOME
#define STB_TEXTEDIT_K_LINEEND   SDL_SCANCODE_END
#define STB_TEXTEDIT_K_TEXTSTART (SDL_SCANCODE_HOME | STB_TEXTEDIT_K_CTRL) // Use the flag bit
#define STB_TEXTEDIT_K_TEXTEND   (SDL_SCANCODE_END | STB_TEXTEDIT_K_CTRL)  // Use the flag bit
#define STB_TEXTEDIT_K_DELETE    SDL_SCANCODE_DELETE
#define STB_TEXTEDIT_K_BACKSPACE SDL_SCANCODE_BACKSPACE
#define STB_TEXTEDIT_K_UNDO      (SDL_SCANCODE_Z | STB_TEXTEDIT_K_CTRL) // Use the flag bit
#define STB_TEXTEDIT_K_REDO      (SDL_SCANCODE_Y | STB_TEXTEDIT_K_CTRL) // Use the flag bit

// #define STB_TEXTEDIT_K_INSERT    SDL_SCANCODE_INSERT
#define STB_TEXTEDIT_IS_SPACE(ch) std::isspace(static_cast<unsigned char>(ch))

#define STB_TEXTEDIT_K_WORDLEFT  (SDL_SCANCODE_LEFT | STB_TEXTEDIT_K_CTRL)  // Use the flag bit
#define STB_TEXTEDIT_K_WORDRIGHT (SDL_SCANCODE_RIGHT | STB_TEXTEDIT_K_CTRL) // Use the flag bit

#include "stb_textedit.h"

const int LABEL_GAP = 10;
const float TEXT_INPUT_ROUNDING = 2.5f;
const int SCROLL_CURSOR_RIGHT_GAP = 15;
const gfx::Size TEXT_INPUT_PADDING(10, 5);
const gfx::Color BORDER_COLOR(70, 70, 70, 255);
const gfx::Color ACTIVE_BORDER_COLOR(90, 90, 90, 255);
const gfx::Color BG_COLOR(20, 20, 20, 200);
const gfx::Color TEXT_COLOR(255, 255, 255, 255);
const gfx::Color PLACEHOLDER_COLOR(150, 150, 150, 180);
const gfx::Color COMPOSITION_TEXT_COLOR(200, 200, 255, 255); // For IME
const gfx::Color COMPOSITION_BG_COLOR(60, 60, 60, 150);      // For IME
const gfx::Color SELECTION_COLOR(100, 100, 200, 100);

namespace {
	struct TextInputStateInternal {
		STB_TexteditState edit_state{};
		bool active = false;
		std::string composition;   // For IME
		int ime_cursor = 0;        // Cursor within IME composition
		int ime_selection_len = 0; // Selection length within IME composition
	};

	std::unordered_map<std::string, TextInputStateInternal> text_input_map;

	struct Positions {
		gfx::Point label_pos;
		gfx::Rect input_rect;
		gfx::Point text_pos;
	};

	Positions get_positions(
		const ui::Container& container, const ui::TextInputElementData& input_data, const ui::AnimatedElement& element
	) {
		gfx::Point label_pos = element.element->rect.origin();
		auto input_rect = element.element->rect;
		// Assuming element rect is the input field directly
		gfx::Point text_pos(input_rect.x + TEXT_INPUT_PADDING.w, input_rect.y + TEXT_INPUT_PADDING.h);

		return {
			.label_pos = label_pos,
			.input_rect = input_rect,
			.text_pos = text_pos,
		};
	}

	float get_cursor_x(const ui::TextInputElementData& input_data, int cursor_pos, const gfx::Point& text_start_pos) {
		if (!input_data.text || !input_data.font)
			return text_start_pos.x;

		cursor_pos = std::clamp(cursor_pos, 0, static_cast<int>(input_data.text->length()));
		auto text_before_cursor = input_data.text->substr(0, cursor_pos);

		return text_start_pos.x + input_data.font->calc_size(text_before_cursor).w;
	}

	void textedit_layoutrow(StbTexteditRow* r, ui::TextInputElementData* str, int n) {
		if (!str || !str->font || !str->text) { // Safety checks
			r->num_chars = 0;
			r->x0 = r->x1 = 0;
			r->baseline_y_delta = 1.0f; // Avoid division by zero later maybe
			r->ymin = 0;
			r->ymax = 1.0f;
			return;
		}
		const render::Font& font = *str->font;
		const std::string& text = *str->text;
		int text_len = text.length();

		// Basic single-line implementation: find next newline or end of string
		int start_char = n;
		start_char = std::max(start_char, 0);
		start_char = std::min(start_char, text_len);

		int end_char = text_len; // Assume rest of string initially
		for (int i = start_char; i < text_len; ++i) {
			if (text[i] == '\n') {
				end_char = i + 1; // Include the newline in this row's count
				break;
			}
		}
		r->num_chars = end_char - start_char;
		r->num_chars = std::max(r->num_chars, 0);

		// Calculate width
		std::string sub;
		if (r->num_chars > 0) {
			// Make sure substr args are valid
			int count = r->num_chars;
			if (start_char + count > text_len) {
				count = text_len - start_char;
			}
			if (count > 0) {
				sub = text.substr(start_char, count);
				// Trim trailing newline for width calculation if it exists
				if (!sub.empty() && sub.back() == '\n') {
					sub.pop_back();
				}
			}
		}
		gfx::Size text_size = font.calc_size(sub);

		r->x0 = 0; // Relative to the text starting position
		r->x1 = static_cast<float>(text_size.w);
		r->baseline_y_delta = static_cast<float>(font.height());
		r->ymin = 0;
		r->ymax = static_cast<float>(font.height());
	}

	float textedit_getwidth(ui::TextInputElementData* str, int n, int i) {
		if (!str || !str->font || !str->text)
			return 0;

		// bounds check
		if (n + i < 0 || n + i >= str->text->length())
			return 0;

		std::array<char, 2> c_str = { (*(str->text))[n + i], 0 };
		return static_cast<float>(str->font->calc_size(c_str.data()).w);
	}

	void textedit_deletechars(ui::TextInputElementData* str, int i, int n) {
		if (!str || !str->text)
			return; // Safety check

		int current_len = str->text->length();
		i = std::max(i, 0);

		if (i >= current_len) {
			return;
		} // Cannot delete past end

		if (i + n > current_len) {
			n = current_len - i; // Adjust count if it goes past end
		}

		if (n <= 0) {
			return;
		}

		str->text->erase(i, n);
		if (str->on_change) {
			(*str->on_change)(*str->text);
		}
	}

	int textedit_insertchars(ui::TextInputElementData* str, int i, const STB_TEXTEDIT_CHARTYPE* c, int n) {
		if (!str || !str->text)
			return 0;

		int current_len = str->text->length();
		i = std::clamp(i, 0, current_len);
		if (n <= 0) {
			return 1;
		}

		// Insert `n` characters starting from `c`
		str->text->insert(i, std::string(c, n));

		if (str->on_change) {
			(*str->on_change)(*str->text);
		}
		return 1;
	}

	int textedit_keytotext(int key) {
		// This function remains largely unused as we handle input via SDL events
		return -1;
	}

	int is_word_boundary(char c) {
		return std::isspace(c) || std::ispunct(c);
	}

	int move_word_left(STB_TEXTEDIT_STRING* str, int c) {
		if (c <= 0)
			return 0;

		// Skip spaces backwards
		while (c > 0 && is_word_boundary((*str->text)[c - 1]))
			c--;

		// Skip non-spaces backwards
		while (c > 0 && !is_word_boundary((*str->text)[c - 1]))
			c--;

		return c;
	}

	int move_word_right(STB_TEXTEDIT_STRING* str, int c) {
		int len = STB_TEXTEDIT_STRINGLEN(str);
		if (c >= len)
			return len;

		// Skip spaces forward
		while (c < len && is_word_boundary((*str->text)[c]))
			c++;

		// Skip non-spaces forward
		while (c < len && !is_word_boundary((*str->text)[c]))
			c++;

		return c;
	}
}

#define STB_TEXTEDIT_MOVEWORDRIGHT move_word_right
#define STB_TEXTEDIT_MOVEWORDLEFT  move_word_left

#define STB_TEXTEDIT_IMPLEMENTATION
#include "stb_textedit.h"

namespace {
	void handle_text_input_event(
		ui::TextInputElementData& input_data, TextInputStateInternal& state, const SDL_Event& event
	) {
		switch (event.type) {
			case SDL_EVENT_TEXT_INPUT: {
				// Character input - pass directly to stb_textedit_key
				// STB expects int, char is fine but cast for clarity/safety if needed
				// Handle multi-byte UTF-8 chars in the string correctly
				const char* text_input = event.text.text;
				stb_textedit_paste(
					&input_data, &state.edit_state, text_input, strlen(text_input)
				); // Use paste for multi-char input potentially
				   // OR loop through bytes if stb_textedit_key expects single chars
				   // for (int i = 0; text_input[i] != '\0'; ++i) {
				   //    stb_textedit_key(&input_data, &state.edit_state, (STB_TEXTEDIT_KEYTYPE)text_input[i]);
				   // }
				break;
			}

			case SDL_EVENT_KEY_DOWN: {
				SDL_Scancode scan = event.key.scancode;
				SDL_Keymod mod = SDL_GetModState();

#ifdef __APPLE__
				bool is_cmd = (mod & SDL_KMOD_GUI) != 0u;
				bool is_shortcut = (mod & SDL_KMOD_GUI) != 0u; // Use Cmd for shortcuts
#else
				bool is_cmd = (mod & SDL_KMOD_CTRL) != 0u;
				bool is_shortcut = (mod & SDL_KMOD_CTRL) != 0u; // Use Ctrl for shortcuts
#endif
				bool is_ctrl = (mod & SDL_KMOD_CTRL) != 0u; // true Ctrl key
				bool is_shift = (mod & SDL_KMOD_SHIFT) != 0u;
				bool is_alt = (mod & SDL_KMOD_ALT) != 0u;

				int stb_key = 0;

				// Handle special movement (word, line jump)
#ifdef __APPLE__
				if (scan == SDL_SCANCODE_LEFT) {
					if (is_alt)
						stb_key = STB_TEXTEDIT_K_WORDLEFT;
					else if (is_cmd)
						stb_key = STB_TEXTEDIT_K_LINESTART;
				}
				else if (scan == SDL_SCANCODE_RIGHT) {
					if (is_alt)
						stb_key = STB_TEXTEDIT_K_WORDRIGHT;
					else if (is_cmd)
						stb_key = STB_TEXTEDIT_K_LINEEND;
				}
				else if (scan == SDL_SCANCODE_UP) {
					if (is_cmd)
						stb_key = STB_TEXTEDIT_K_TEXTSTART;
				}
				else if (scan == SDL_SCANCODE_DOWN) {
					if (is_cmd)
						stb_key = STB_TEXTEDIT_K_TEXTEND;
				}
#else
				if (scan == SDL_SCANCODE_LEFT && is_ctrl)
					stb_key = STB_TEXTEDIT_K_WORDLEFT;
				else if (scan == SDL_SCANCODE_RIGHT && is_ctrl)
					stb_key = STB_TEXTEDIT_K_WORDRIGHT;
				else if (scan == SDL_SCANCODE_UP && is_ctrl)
					stb_key = STB_TEXTEDIT_K_TEXTSTART;
				else if (scan == SDL_SCANCODE_DOWN && is_ctrl)
					stb_key = STB_TEXTEDIT_K_TEXTEND;
#endif

				if (stb_key == 0) {
					// No special movement: normal keys
					stb_key = scan;
				}

				// If Shift is held, apply shift modifier now
				if (is_shift) {
					stb_key |= STB_TEXTEDIT_K_SHIFT;
				}

				if (is_shortcut) {
					switch (scan) {
						case SDL_SCANCODE_X:   // cut
						case SDL_SCANCODE_C: { // copy
							if (STB_TEXT_HAS_SELECTION(&state.edit_state)) {
								int start = state.edit_state.select_start;
								int end = state.edit_state.select_end;
								if (start > end)
									std::swap(start, end);

								std::string selection = input_data.text->substr(start, end - start);
								SDL_SetClipboardText(selection.c_str());

								if (scan == SDL_SCANCODE_X) {
									stb_textedit_cut(&input_data, &state.edit_state);
								}
							}
							break;
						}
						case SDL_SCANCODE_V: { // paste
							if (SDL_HasClipboardText()) {
								if (char* clipboard_text = SDL_GetClipboardText()) {
									stb_textedit_paste(
										&input_data, &state.edit_state, clipboard_text, strlen(clipboard_text)
									);
									SDL_free(clipboard_text);
								}
							}
							break;
						}
						case SDL_SCANCODE_A: { // select all (Ctrl + A / Cmd + A)
							if (is_ctrl || is_cmd) {
								int text_length = input_data.text->length();
								state.edit_state.select_start = 0;
								state.edit_state.select_end = text_length;

								state.edit_state.cursor = state.edit_state.select_start;
							}
							break;
						}
						default:
							// should never reach
							stb_textedit_key(&input_data, &state.edit_state, stb_key);
							break;
					}

					keys::on_key_press_handled(scan);
				}
				else {
					stb_textedit_key(&input_data, &state.edit_state, stb_key);
					keys::on_key_press_handled(scan);
				}
				break;
			}

			case SDL_EVENT_TEXT_EDITING: {
				state.composition = event.edit.text;
				state.ime_cursor = event.edit.start; // Use the state's ime_cursor
				state.ime_selection_len =
					event.edit.length; // Use the state's ime_selection_len
				                       // You might want to adjust the main cursor (state.edit_state.cursor) here
				                       // E.g., place it at the beginning or end of the composition temporarily.
				                       // For now, we leave it where text input placed it.
				break;
			}

			default:
				break;
		}

		stb_textedit_clamp(&input_data, &state.edit_state);
	}
}

void ui::render_text_input(const Container& container, const AnimatedElement& element) {
	const auto& input_data = std::get<TextInputElementData>(element.element->data);

	float anim = element.animations.at(hasher("main")).current;
	float hover_anim = element.animations.at(hasher("hover")).current;
	float focus_anim = element.animations.at(hasher("focus")).current;

	auto pos = get_positions(container, input_data, element);
	auto& state = text_input_map.at(element.element->id);

	// --- Render Background and Border ---
	gfx::Color current_bg_color = BG_COLOR.adjust_alpha(anim);
	render::rounded_rect_filled(pos.input_rect, current_bg_color, TEXT_INPUT_ROUNDING);

	gfx::Color current_border_color =
		gfx::Color::lerp(BORDER_COLOR, ACTIVE_BORDER_COLOR, focus_anim).adjust_alpha(anim);
	render::rounded_rect_stroke(pos.input_rect, current_border_color, TEXT_INPUT_ROUNDING);

	// --- Clipping ---
	gfx::Rect clip_rect = pos.input_rect;
	clip_rect.x += TEXT_INPUT_PADDING.w;
	clip_rect.y += TEXT_INPUT_PADDING.h;
	clip_rect.w -= TEXT_INPUT_PADDING.w * 2;
	clip_rect.h -= TEXT_INPUT_PADDING.h * 2;
	clip_rect.w = std::max(clip_rect.w, 0);
	clip_rect.h = std::max(clip_rect.h, 0);
	render::push_clip_rect(clip_rect, true);

	// --- Render Text / Placeholder ---
	const std::string& display_text = *input_data.text; // Assumes text ptr is valid
	gfx::Color current_text_color = TEXT_COLOR.adjust_alpha(anim);
	gfx::Point current_text_pos = pos.text_pos;

	// Calculate the total width of the text to determine overflow
	float text_width = input_data.font->calc_size(display_text).w;

	// Horizontal Scrolling Logic: Shift text position if it's too wide
	if (text_width > clip_rect.w) {
		int cursor_x = get_cursor_x(input_data, state.edit_state.cursor, gfx::Point(0, 0));
		int scroll_offset = std::max(0, cursor_x - (clip_rect.w - SCROLL_CURSOR_RIGHT_GAP));
		current_text_pos.x = current_text_pos.x - scroll_offset;
	}

	// Handle vertical scrolling if needed (optional, depending on requirements)
	// If text overflows vertically (height of text exceeds clip_rect height), we could add vertical scrolling logic

	if (display_text.empty() && !state.active && !input_data.placeholder.empty()) {
		gfx::Color current_placeholder_color = PLACEHOLDER_COLOR.adjust_alpha(anim);
		render::text(current_text_pos, current_placeholder_color, input_data.placeholder, *input_data.font);
	}
	else {
		// --- Render Selection ---
		if (STB_TEXT_HAS_SELECTION(&state.edit_state)) {
			int sel_start = state.edit_state.select_start;
			int sel_end = state.edit_state.select_end;
			if (sel_start > sel_end)
				std::swap(sel_start, sel_end);

			// Use helper from anonymous namespace
			float x1 = get_cursor_x(input_data, sel_start, current_text_pos);
			float x2 = get_cursor_x(input_data, sel_end, current_text_pos);

			gfx::Rect selection_rect(
				static_cast<int>(x1), current_text_pos.y, static_cast<int>(x2 - x1), input_data.font->height()
			);

			if (selection_rect.w > 0 && selection_rect.h > 0) {
				render::rect_filled(selection_rect, SELECTION_COLOR.adjust_alpha(anim));
			}
		}

		// Render actual text
		render::text(current_text_pos, current_text_color, display_text, *input_data.font);

		// --- Render IME Composition --- TODO: NEED TO TEST
		if (state.active && !state.composition.empty()) {
			float base_comp_x = get_cursor_x(input_data, state.edit_state.cursor, current_text_pos);
			gfx::Point comp_pos = { (int)base_comp_x, current_text_pos.y };
			gfx::Size comp_size = input_data.font->calc_size(state.composition);
			gfx::Rect comp_bg_rect(comp_pos.x, comp_pos.y, comp_size.w, comp_size.h);

			render::rect_filled(comp_bg_rect, COMPOSITION_BG_COLOR.adjust_alpha(anim));
			render::text(comp_pos, COMPOSITION_TEXT_COLOR.adjust_alpha(anim), state.composition, *input_data.font);
			render::line(
				{ comp_pos.x, comp_pos.y + comp_size.h },
				{ comp_pos.x + comp_size.w, comp_pos.y + comp_size.h },
				COMPOSITION_TEXT_COLOR.adjust_alpha(anim)
			);

			if (state.ime_cursor >= 0) {
				std::string before_ime_cursor = state.composition.substr(0, state.ime_cursor);
				float ime_cursor_x_offset = input_data.font->calc_size(before_ime_cursor).w;
				render::line(
					{ comp_pos.x + (int)ime_cursor_x_offset, comp_pos.y },
					{ comp_pos.x + (int)ime_cursor_x_offset, comp_pos.y + input_data.font->height() },
					COMPOSITION_TEXT_COLOR.adjust_alpha(anim),
					false,
					1.f
				);
			}
		}

		// --- Render Cursor ---
		if (state.active) {
			float cursor_x = get_cursor_x(input_data, state.edit_state.cursor, current_text_pos);
			gfx::Point p1(static_cast<int>(cursor_x), current_text_pos.y);
			gfx::Point p2(static_cast<int>(cursor_x), current_text_pos.y + input_data.font->height());
			// Ensure cursor is drawn within clip rect
			if (p1.x >= clip_rect.x && p1.x <= clip_rect.x + clip_rect.w) {
				render::line(p1, p2, current_text_color, false, 1.f);
			}
		}
	}
	render::pop_clip_rect();
}

bool ui::update_text_input(const Container& container, AnimatedElement& element) {
	auto& input_data = std::get<TextInputElementData>(element.element->data);
	auto& hover_anim = element.animations.at(hasher("hover"));
	auto& focus_anim = element.animations.at(hasher("focus"));

	auto pos = get_positions(container, input_data, element);
	auto& state = text_input_map.at(element.element->id);

	bool hovered = pos.input_rect.contains(keys::mouse_pos) && set_hovered_element(element);
	hover_anim.set_goal(hovered ? 1.f : 0.f);

	if (hovered) {
		set_cursor(SDL_SYSTEM_CURSOR_TEXT);
	}

	// Handle focus/unfocus with mouse click
	if (keys::is_mouse_pressed()) {
		if (hovered) {
			if (!state.active) {
				keys::on_mouse_press_handled(SDL_BUTTON_LEFT);
				active_element = &element;
				state.active = true;
				focus_anim.set_goal(1.f);

				SDL_Rect input_rect = { pos.input_rect.x, pos.input_rect.y, pos.input_rect.w, pos.input_rect.h };
				SDL_StartTextInput(container.window);
				SDL_SetTextInputArea(container.window, &input_rect, 10);
			}

			// Use stb_textedit_click to set cursor position (relative to text start)
			stb_textedit_click(
				&input_data, &state.edit_state, keys::mouse_pos.x - pos.text_pos.x, keys::mouse_pos.y - pos.text_pos.y
			);
		}
		else if (active_element == &element) {
			active_element = nullptr;
		}
	}

	// Handle deactivation
	if (state.active && active_element != &element) {
		state.active = false;
		focus_anim.set_goal(0.f);

		// Stop SDL text input if not in another text input
		if (active_element && active_element->element->type != ElementType::TEXT_INPUT) {
			if (SDL_TextInputActive(container.window)) {
				SDL_StopTextInput(container.window);
			}
		}
	}

	bool active = active_element == &element;

	if (active) {
		while (!text_event_queue.empty()) {
			auto& event = text_event_queue.front();
			handle_text_input_event(input_data, state, event);
			text_event_queue.erase(text_event_queue.begin());
		}

		// --- Handle Mouse Drag ---
		if (state.active && keys::is_mouse_dragging(SDL_BUTTON_LEFT)) {
			// Check if drag started on this element (requires keys::drag_start_element_id or similar)
			// if (keys::drag_start_element_id == element.element->id) {
			stb_textedit_drag(
				&input_data, &state.edit_state, keys::mouse_pos.x - pos.text_pos.x, keys::mouse_pos.y - pos.text_pos.y
			);
		}
	}

	// Clamp cursor/selection just in case
	stb_textedit_clamp(&input_data, &state.edit_state);

	return active;
}

ui::Element& ui::add_text_input(
	const std::string& id,
	Container& container,
	std::string& text, // Reference to the external string
	const std::string& placeholder,
	const render::Font& font,
	std::optional<std::function<void(const std::string&)>> on_change
) {
	const gfx::Size input_size(200, font.height() + (TEXT_INPUT_PADDING.h * 2));

	if (!text_input_map.contains(id)) {
		TextInputStateInternal new_state;
		stb_textedit_initialize_state(&new_state.edit_state, 1);
		new_state.edit_state.single_line = 1;
		text_input_map.emplace(id, std::move(new_state));
	}

	Element element(
		id,
		ElementType::TEXT_INPUT,
		gfx::Rect(container.current_position, input_size),
		TextInputElementData{
			.text = &text,
			.placeholder = placeholder,
			.font = &font,
			.on_change = std::move(on_change),
		},
		render_text_input,
		update_text_input
	);

	return *add_element(
		container,
		std::move(element),
		container.element_gap,
		{
			{ hasher("main"), { .speed = 25.f } },
			{ hasher("hover"), { .speed = 80.f } },
			{ hasher("focus"), { .speed = 25.f } },
		}
	);
}
