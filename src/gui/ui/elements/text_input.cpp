#include "../ui.h"
#include "../../render/render.h"
#include "../keys.h"
#include "gui/sdl.h"

const int LABEL_GAP = 10;
const gfx::Size TEXT_INPUT_PADDING(10, 5);
const int CURSOR_BLINK_RATE = 500; // milliseconds
const gfx::Color BORDER_COLOR(120, 120, 150, 255);
const gfx::Color ACTIVE_BORDER_COLOR(150, 150, 255, 255);
const gfx::Color BG_COLOR(40, 40, 60, 200);
const gfx::Color TEXT_COLOR(255, 255, 255, 255);
const gfx::Color PLACEHOLDER_COLOR(150, 150, 150, 180);
const gfx::Color COMPOSITION_TEXT_COLOR(200, 200, 255, 255);
const gfx::Color COMPOSITION_BG_COLOR(60, 60, 100, 150);
const gfx::Color SELECTION_COLOR(100, 100, 200, 100);

namespace {
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
		input_rect.y = label_pos.y + input_data.font->height() + LABEL_GAP;
		input_rect.h = input_data.font->height() + (TEXT_INPUT_PADDING.h * 2);

		gfx::Point text_pos(input_rect.x + TEXT_INPUT_PADDING.w, input_rect.y + TEXT_INPUT_PADDING.h);

		return {
			.label_pos = label_pos,
			.input_rect = input_rect,
			.text_pos = text_pos,
		};
	}

	bool should_show_cursor(const ui::AnimatedElement& element, const ui::TextInputElementData& input_data) {
		if (element.animations.at(ui::hasher("focus")).current < 0.1f) {
			return false;
		}

		// // Show cursor constantly during composition
		// if (!input_data.composition_text.empty()) {
		// 	return true;
		// }

		uint32_t ticks = SDL_GetTicks();
		return (ticks % (CURSOR_BLINK_RATE * 2)) < CURSOR_BLINK_RATE;
	}

	// Calculate text width up to a certain position
	int text_width_to_pos(const std::string& text, int pos, const render::Font& font) {
		if (pos <= 0)
			return 0;
		if (pos >= static_cast<int>(text.length()))
			return font.calc_size(text).w;

		return font.calc_size(text.substr(0, pos)).w;
	}

	// Find character position closest to given x coordinate
	int find_cursor_pos(const std::string& text, int x, int text_start_x, const render::Font& font) {
		if (text.empty() || x <= text_start_x)
			return 0;

		int best_pos = 0;
		int best_diff = INT_MAX;

		for (int i = 0; i <= static_cast<int>(text.length()); i++) {
			int pos_x = text_start_x + text_width_to_pos(text, i, font);
			int diff = std::abs(pos_x - x);

			if (diff < best_diff) {
				best_diff = diff;
				best_pos = i;
			}
		}

		return best_pos;
	}

	// Get visible text range (for scrolling when text is too long)
	std::pair<int, int> get_visible_text_range(
		const std::string& text, int cursor_pos, int max_width, const render::Font& font
	) {
		if (text.empty())
			return { 0, 0 };
		if (font.calc_size(text).w <= max_width)
			return { 0, static_cast<int>(text.length()) };

		// Ensure cursor is always visible
		int start = 0;
		int end = static_cast<int>(text.length());

		// Find valid range that includes cursor and fits within max_width
		int cursor_width = text_width_to_pos(text, cursor_pos, font);

		// If cursor is too far right, adjust start
		if (cursor_width > max_width) {
			// Find a start position that shows the cursor
			int possible_width = 0;
			for (int i = cursor_pos; i >= 0; i--) {
				possible_width = font.calc_size(text.substr(i, cursor_pos - i)).w;
				if (possible_width <= max_width) {
					start = i;
					break;
				}
			}
		}

		// Find maximum visible end
		int visible_width = 0;
		for (int i = start; i <= static_cast<int>(text.length()); i++) {
			visible_width = font.calc_size(text.substr(start, i - start)).w;
			if (visible_width > max_width) {
				end = i - 1;
				break;
			}
		}

		return { start, end };
	}
}

struct TextInputState {
	int cursor_position = 0;
	int selection_start = 0;
	int selection_end = 0;
	bool has_selection = false;
	int text_scroll_offset = 0;
	std::string composition_text;
	int composition_cursor = 0;
	int composition_selection_start = 0;
	int composition_selection_length = 0;
};

std::unordered_map<std::string, TextInputState> input_map;

void ui::render_text_input(const Container& container, const AnimatedElement& element) {
	const auto& input_data = std::get<TextInputElementData>(element.element->data);

	float anim = element.animations.at(hasher("main")).current;
	float hover_anim = element.animations.at(hasher("hover")).current;
	float focus_anim = element.animations.at(hasher("focus")).current;

	auto pos = get_positions(container, input_data, element);

	auto& input = input_map.at(element.element->id);

	// Render label
	render::text(pos.label_pos, gfx::Color(255, 255, 255, anim * 255), input_data.placeholder, *input_data.font);

	// Render input background
	render::rect_filled(pos.input_rect, gfx::Color(BG_COLOR.r, BG_COLOR.g, BG_COLOR.b, BG_COLOR.a * anim));

	// Blend between normal and active border colors based on focus animation
	gfx::Color border = gfx::Color::lerp(BORDER_COLOR, ACTIVE_BORDER_COLOR, focus_anim);
	border.a *= anim;

	// Apply hover effect
	if (hover_anim > 0.f) {
		border = gfx::Color::lerp(border, gfx::Color(200, 200, 255, border.a), hover_anim * 0.3f);
	}

	// Render border
	render::rect_stroke(pos.input_rect, border, 1);

	const int max_text_width = pos.input_rect.w - (TEXT_INPUT_PADDING.w * 2);

	// Render text or placeholder
	if (input_data.text->empty() && input.composition_text.empty() && focus_anim < 0.5f) {
		// Show placeholder text
		gfx::Color placeholder_color = PLACEHOLDER_COLOR;
		placeholder_color.a *= anim * (1.0f - focus_anim);
		render::text(pos.text_pos, placeholder_color, input_data.placeholder, *input_data.font);
	}
	else {
		// Calculate visible text range for scrolling
		auto [start_idx, end_idx] =
			get_visible_text_range(*input_data.text, input.cursor_position, max_text_width, *input_data.font);

		std::string visible_text;
		if (end_idx > start_idx) {
			visible_text = input_data.text->substr(start_idx, end_idx - start_idx);
		}

		// Calculate text position with scrolling adjustment
		gfx::Point adjusted_text_pos = pos.text_pos;

		// Render visible text
		if (!visible_text.empty()) {
			gfx::Color text_color = TEXT_COLOR;
			text_color.a *= anim;
			render::text(adjusted_text_pos, text_color, visible_text, *input_data.font);
		}

		// Render selection if it exists
		if (input.has_selection && focus_anim > 0.1f) {
			int sel_start = std::max(input.selection_start, start_idx) - start_idx;
			int sel_end = std::min(input.selection_end, end_idx) - start_idx;

			if (sel_start < sel_end) {
				int sel_start_x = adjusted_text_pos.x + text_width_to_pos(visible_text, sel_start, *input_data.font);
				int sel_end_x = adjusted_text_pos.x + text_width_to_pos(visible_text, sel_end, *input_data.font);

				gfx::Rect selection_rect(
					sel_start_x, adjusted_text_pos.y, sel_end_x - sel_start_x, input_data.font->height()
				);

				gfx::Color sel_color = SELECTION_COLOR;
				sel_color.a *= anim * focus_anim;
				render::rect_filled(selection_rect, sel_color);
			}
		}

		// Render composition text if it exists
		if (!input.composition_text.empty()) {
			// Position composition text at cursor
			int cursor_x = adjusted_text_pos.x;
			if (start_idx < input.cursor_position) {
				cursor_x += text_width_to_pos(
					visible_text, std::min(input.cursor_position, end_idx) - start_idx, *input_data.font
				);
			}

			gfx::Point comp_pos(cursor_x, adjusted_text_pos.y);

			// Draw composition background
			int comp_width = input_data.font->calc_size(input.composition_text).w;
			gfx::Rect comp_rect(comp_pos.x, comp_pos.y, comp_width, input_data.font->height());

			gfx::Color comp_bg = COMPOSITION_BG_COLOR;
			comp_bg.a *= anim * focus_anim;
			render::rect_filled(comp_rect, comp_bg);

			// Draw composition text
			gfx::Color comp_color = COMPOSITION_TEXT_COLOR;
			comp_color.a *= anim * focus_anim;
			render::text(comp_pos, comp_color, input.composition_text, *input_data.font);

			// Draw composition selection/cursor
			if (input.composition_selection_length > 0) {
				int sel_start_x =
					comp_pos.x +
					text_width_to_pos(input.composition_text, input.composition_selection_start, *input_data.font);

				int sel_end_x = comp_pos.x + text_width_to_pos(
												 input.composition_text,
												 input.composition_selection_start + input.composition_selection_length,
												 *input_data.font
											 );

				gfx::Rect comp_sel_rect(sel_start_x, comp_pos.y, sel_end_x - sel_start_x, input_data.font->height());

				gfx::Color comp_sel_color(150, 150, 255, 180 * anim * focus_anim);
				render::rect_filled(comp_sel_rect, comp_sel_color);
			}
		}

		// Render cursor when focused
		if (should_show_cursor(element, input_data)) {
			int cursor_x = adjusted_text_pos.x;

			// If there's composition text, position at composition cursor
			if (!input.composition_text.empty()) {
				cursor_x += text_width_to_pos(input.composition_text, input.composition_cursor, *input_data.font);
			}
			// Otherwise position at text cursor
			else if (start_idx < input.cursor_position) {
				cursor_x += text_width_to_pos(
					visible_text, std::min(input.cursor_position, end_idx) - start_idx, *input_data.font
				);
			}

			gfx::Point cursor_top(cursor_x, adjusted_text_pos.y);
			gfx::Point cursor_bottom(cursor_x, adjusted_text_pos.y + input_data.font->height());

			render::line(cursor_top, cursor_bottom, gfx::Color(255, 255, 255, 255 * focus_anim), 1);
		}
	}
}

bool ui::update_text_input(const Container& container, AnimatedElement& element) {
	auto& input_data = std::get<TextInputElementData>(element.element->data);
	auto& hover_anim = element.animations.at(hasher("hover"));
	auto& focus_anim = element.animations.at(hasher("focus"));

	auto pos = get_positions(container, input_data, element);

	auto& input = input_map.at(element.element->id);

	bool hovered = pos.input_rect.contains(keys::mouse_pos) && set_hovered_element(element);

	hover_anim.set_goal(hovered ? 1.f : 0.f);

	if (hovered) {
		set_cursor(SDL_SYSTEM_CURSOR_TEXT);
	}

	// Handle mouse click to focus/unfocus
	if (hovered && keys::is_mouse_down()) {
		active_element = &element;
		focus_anim.set_goal(1.f);

		// Set input rect position for SDL
		SDL_Rect input_rect = { pos.input_rect.x, pos.input_rect.y, pos.input_rect.w, pos.input_rect.h };

		// Start SDL text input
		SDL_StartTextInput(container.window);
		SDL_SetTextInputArea(container.window, &input_rect, 10);

		// Set cursor position based on click position
		if (!input_data.text->empty()) {
			int click_x = keys::mouse_pos.x;
			input.cursor_position = find_cursor_pos(*input_data.text, click_x, pos.text_pos.x, *input_data.font);
			input.has_selection = false;
			input.selection_start = input.cursor_position;
			input.selection_end = input.cursor_position;
		}
	}
	else if (keys::is_mouse_down() && !hovered) {
		if (active_element == &element) {
			active_element = nullptr;
			focus_anim.set_goal(0.f);

			// Stop SDL text input
			SDL_StopTextInput(container.window);
			input.composition_text.clear();
		}
	}

	bool active = active_element == &element;

	// Handle text editing and input events
	if (active) {
		SDL_Event event;

		// Handle key events first (cursor movement, deletion, etc.)
		if (keys::is_key_pressed(SDL_SCANCODE_LEFT)) {
			if (keys::is_key_down(SDL_SCANCODE_LSHIFT) || keys::is_key_down(SDL_SCANCODE_RSHIFT)) {
				// Extend selection
				if (!input.has_selection) {
					input.selection_start = input.cursor_position;
					input.selection_end = input.cursor_position;
					input.has_selection = true;
				}

				if (input.cursor_position > 0) {
					input.cursor_position--;
					input.selection_end = input.cursor_position;
				}
			}
			else {
				// Clear selection and move cursor
				if (input.has_selection) {
					input.cursor_position = input.selection_start;
					input.has_selection = false;
				}
				else if (input.cursor_position > 0) {
					input.cursor_position--;
				}
			}
		}
		else if (keys::is_key_pressed(SDL_SCANCODE_RIGHT)) {
			if (keys::is_key_down(SDL_SCANCODE_LSHIFT) || keys::is_key_down(SDL_SCANCODE_RSHIFT)) {
				// Extend selection
				if (!input.has_selection) {
					input.selection_start = input.cursor_position;
					input.selection_end = input.cursor_position;
					input.has_selection = true;
				}

				if (input.cursor_position < static_cast<int>(input_data.text->length())) {
					input.cursor_position++;
					input.selection_end = input.cursor_position;
				}
			}
			else {
				// Clear selection and move cursor
				if (input.has_selection) {
					input.cursor_position = input.selection_end;
					input.has_selection = false;
				}
				else if (input.cursor_position < static_cast<int>(input_data.text->length())) {
					input.cursor_position++;
				}
			}
		}
		else if (keys::is_key_pressed(SDL_SCANCODE_HOME)) {
			if (keys::is_key_down(SDL_SCANCODE_LSHIFT) || keys::is_key_down(SDL_SCANCODE_RSHIFT)) {
				if (!input.has_selection) {
					input.selection_end = input.cursor_position;
					input.has_selection = true;
				}
				input.cursor_position = 0;
				input.selection_start = 0;
			}
			else {
				input.cursor_position = 0;
				input.has_selection = false;
			}
		}
		else if (keys::is_key_pressed(SDL_SCANCODE_END)) {
			if (keys::is_key_down(SDL_SCANCODE_LSHIFT) || keys::is_key_down(SDL_SCANCODE_RSHIFT)) {
				if (!input.has_selection) {
					input.selection_start = input.cursor_position;
					input.has_selection = true;
				}
				input.cursor_position = static_cast<int>(input_data.text->length());
				input.selection_end = input.cursor_position;
			}
			else {
				input.cursor_position = static_cast<int>(input_data.text->length());
				input.has_selection = false;
			}
		}
		else if (keys::is_key_pressed(SDL_SCANCODE_DELETE)) {
			if (input.has_selection) {
				// Delete selection
				int sel_start = std::min(input.selection_start, input.selection_end);
				int sel_end = std::max(input.selection_start, input.selection_end);
				input_data.text->erase(sel_start, sel_end - sel_start);
				input.cursor_position = sel_start;
				input.has_selection = false;

				if (input_data.on_change) {
					(*input_data.on_change)(*input_data.text);
				}
			}
			else if (input.cursor_position < static_cast<int>(input_data.text->length())) {
				// Delete character after cursor
				input_data.text->erase(input.cursor_position, 1);

				if (input_data.on_change) {
					(*input_data.on_change)(*input_data.text);
				}
			}
		}
		else if (keys::is_key_pressed(SDL_SCANCODE_BACKSPACE)) {
			if (input.composition_text.empty()) {
				if (input.has_selection) {
					// Delete selection
					int sel_start = std::min(input.selection_start, input.selection_end);
					int sel_end = std::max(input.selection_start, input.selection_end);
					input_data.text->erase(sel_start, sel_end - sel_start);
					input.cursor_position = sel_start;
					input.has_selection = false;

					if (input_data.on_change) {
						(*input_data.on_change)(*input_data.text);
					}
				}
				else if (input.cursor_position > 0) {
					// Delete character before cursor
					input_data.text->erase(input.cursor_position - 1, 1);
					input.cursor_position--;

					if (input_data.on_change) {
						(*input_data.on_change)(*input_data.text);
					}
				}
			}
		}

		// Handle copy/paste
		if (keys::is_key_down(SDL_SCANCODE_LCTRL) || keys::is_key_down(SDL_SCANCODE_RCTRL)) {
			// Select All (Ctrl+A)
			if (keys::is_key_pressed(SDL_SCANCODE_A)) {
				input.selection_start = 0;
				input.selection_end = static_cast<int>(input_data.text->length());
				input.cursor_position = input.selection_end;
				input.has_selection = true;
			}
			// Copy (Ctrl+C)
			else if (keys::is_key_pressed(SDL_SCANCODE_C) && input.has_selection) {
				int sel_start = std::min(input.selection_start, input.selection_end);
				int sel_end = std::max(input.selection_start, input.selection_end);
				std::string selected_text = input_data.text->substr(sel_start, sel_end - sel_start);
				SDL_SetClipboardText(selected_text.c_str());
			}
			// Cut (Ctrl+X)
			else if (keys::is_key_pressed(SDL_SCANCODE_X) && input.has_selection) {
				int sel_start = std::min(input.selection_start, input.selection_end);
				int sel_end = std::max(input.selection_start, input.selection_end);
				std::string selected_text = input_data.text->substr(sel_start, sel_end - sel_start);
				SDL_SetClipboardText(selected_text.c_str());

				input_data.text->erase(sel_start, sel_end - sel_start);
				input.cursor_position = sel_start;
				input.has_selection = false;

				if (input_data.on_change) {
					(*input_data.on_change)(*input_data.text);
				}
			}
			// Paste (Ctrl+V)
			else if (keys::is_key_pressed(SDL_SCANCODE_V)) {
				// Delete selection first if exists
				if (input.has_selection) {
					int sel_start = std::min(input.selection_start, input.selection_end);
					int sel_end = std::max(input.selection_start, input.selection_end);
					input_data.text->erase(sel_start, sel_end - sel_start);
					input.cursor_position = sel_start;
					input.has_selection = false;
				}

				// Paste clipboard text
				char* clipboard_text = SDL_GetClipboardText();
				if (clipboard_text) {
					std::string paste_text(clipboard_text);
					input_data.text->insert(input.cursor_position, paste_text);
					input.cursor_position += static_cast<int>(paste_text.length());
					SDL_free(clipboard_text);

					if (input_data.on_change) {
						(*input_data.on_change)(*input_data.text);
					}
				}
			}
		}

		// Handle Enter key to confirm input and remove focus
		if (keys::is_key_pressed(SDL_SCANCODE_RETURN) || keys::is_key_pressed(SDL_SCANCODE_KP_ENTER)) {
			active_element = nullptr;
			focus_anim.set_goal(0.f);
			SDL_StopTextInput(container.window);
			input.composition_text.clear();
		}

		// Process SDL text input events
		while (SDL_PeepEvents(&event, 1, SDL_GETEVENT, SDL_EVENT_TEXT_EDITING, SDL_EVENT_TEXT_INPUT) > 0) {
			if (event.type == SDL_EVENT_TEXT_EDITING) {
				// Handle text editing (composition)
				input.composition_text = event.edit.text;
				input.composition_cursor = event.edit.start;
				input.composition_selection_start = event.edit.start;
				input.composition_selection_length = event.edit.length;
			}
			// else if (event.type == SDL_EVENT_TEXT_EDITING_EXT) { TODO: does this even exist
			// 	// Extended composition information (for complex IME)
			// 	input.composition_text = event.editExt.text;
			// 	input.composition_cursor = event.editExt.start;
			// 	input.composition_selection_start = event.editExt.start;
			// 	input.composition_selection_length = event.editExt.length;
			// }
			else if (event.type == SDL_EVENT_TEXT_INPUT) {
				// Text input (committed text)
				// Delete any selected text first
				if (input.has_selection) {
					int sel_start = std::min(input.selection_start, input.selection_end);
					int sel_end = std::max(input.selection_start, input.selection_end);
					input_data.text->erase(sel_start, sel_end - sel_start);
					input.cursor_position = sel_start;
					input.has_selection = false;
				}

				// Insert the committed text at cursor position
				std::string input_text = event.text.text;
				input_data.text->insert(input.cursor_position, input_text);
				input.cursor_position += static_cast<int>(input_text.length());

				// Clear composition text as it's now committed
				input.composition_text.clear();

				if (input_data.on_change) {
					(*input_data.on_change)(*input_data.text);
				}
			}
		}
	}

	return active;
}

ui::Element& ui::add_text_input(
	const std::string& id,
	Container& container,
	std::string& text,
	const std::string& placeholder,
	const render::Font& font,
	std::optional<std::function<void(const std::string&)>> on_change
) {
	// Calculate proper height based on font height and padding
	const gfx::Size input_size(200, font.height() + LABEL_GAP + font.height() + (TEXT_INPUT_PADDING.h * 2));

	if (!input_map.contains(id))
		input_map.emplace(id, TextInputState{});

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
