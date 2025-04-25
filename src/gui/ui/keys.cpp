#include "keys.h"

bool keys::process_event(const SDL_Event& event) {
	switch (event.type) {
		case SDL_EVENT_WINDOW_MOUSE_LEAVE: {
			mouse_pos = { -1, -1 };
			pressed_mouse_keys.clear(
			); // fix mouseup not being registered when left the window todo: handle this properly
			dragging_mouse_keys.clear();
			return true;
		}

		case SDL_EVENT_MOUSE_MOTION: {
			mouse_pos = { static_cast<int>(event.motion.x), static_cast<int>(event.motion.y) };
			return true;
		}

		case SDL_EVENT_MOUSE_BUTTON_DOWN: {
			// mouse_pos = position; // TODO: assuming this is inaccurate too
			pressed_mouse_keys.insert(event.button.button);
			return true;
		}

		case SDL_EVENT_MOUSE_BUTTON_UP: {
			// mouse_pos = position; // TODO: this is inaccurate? if you press open file button move cursor off screen
			// then close the picker there'll be a mouseup event with mouse pos still on the button
			pressed_mouse_keys.erase(event.button.button);
			dragging_mouse_keys.erase(event.button.button);
			return true;
		}

		case SDL_EVENT_KEY_DOWN: {
			pressing_keys.insert(event.key.scancode);
			break;
		}
		case SDL_EVENT_KEY_UP: {
			pressing_keys.erase(event.key.scancode);
			break;
		}

		case SDL_EVENT_MOUSE_WHEEL: {
			// TODO:
			// if (event.wheel.type()) // trackpad
			// 	scroll_delta_precise = event.wheelDelta().y;
			// else // mouse
			scroll_delta = -event.wheel.y * 500.f;

			return true;
		}

		case SDL_EVENT_TEXT_EDITING: {
			if (text_input_active) {
				text_input_composition = event.edit.text;
				text_input_cursor = event.edit.start;
				text_input_selection_start = event.edit.start;
				text_input_selection_length = event.edit.length;
				return true;
			}
			break;
		}

		case SDL_EVENT_TEXT_EDITING_EXT: {
			if (text_input_active) {
				text_input_composition = event.editExt.text;
				text_input_cursor = event.editExt.start;
				text_input_selection_start = event.editExt.start;
				text_input_selection_length = event.editExt.length;
				return true;
			}
			break;
		}

		case SDL_EVENT_TEXT_INPUT: {
			if (text_input_active) {
				last_text_input = event.text.text;
				return true;
			}
			break;
		}

		default:
			return false;
	}

	return false;
}

void keys::on_frame_start() {
	// Clear the handled keys set at the beginning of each frame
	// This should be called at the start of each frame update
	handled_keys.clear();

	// Reset scroll values for the next frame
	scroll_delta = 0.f;
	scroll_delta_precise = 0.f;
}

void keys::on_mouse_press_handled(std::uint8_t button) { // TODO:
	// somethings been pressed, count it as we're not pressing the button anymore
	// todo: is this naive and stupid? it seems kinda elegant, i cant think of a situation
	// where you'd want to press two things with one click
	pressed_mouse_keys.erase(button);
	dragging_mouse_keys.insert(button);
}

void keys::on_key_press_handled(std::uint8_t scancode) {
	// Mark the key as handled for this frame
	handled_keys.insert(scancode);
}

bool keys::is_rect_pressed(const gfx::Rect& rect, std::uint8_t button) {
	return rect.contains(mouse_pos) && is_mouse_down(button);
}

bool keys::is_mouse_down(std::uint8_t button) {
	return pressed_mouse_keys.contains(button);
}

bool keys::is_mouse_dragging(std::uint8_t button) {
	return pressed_mouse_keys.contains(button) || dragging_mouse_keys.contains(button);
}

bool keys::is_key_down(std::uint8_t scancode) {
	return pressing_keys.contains(scancode);
}

bool keys::is_key_pressed(std::uint8_t scancode) {
	// Check if the key is in the pressing set but not in the handled set
	return pressing_keys.contains(scancode) && !handled_keys.contains(scancode);
}

void keys::start_text_input(const SDL_Rect* rect) {
	SDL_StartTextInput();
	if (rect) {
		SDL_SetTextInputRect(rect);
	}
	text_input_active = true;
}

void keys::stop_text_input() {
	SDL_StopTextInput();
	text_input_active = false;
	text_input_composition.clear();
}

bool keys::is_text_input_active() {
	return text_input_active;
}

const std::string& keys::get_composition_text() {
	return text_input_composition;
}

int keys::get_composition_cursor() {
	return text_input_cursor;
}

int keys::get_composition_selection_start() {
	return text_input_selection_start;
}

int keys::get_composition_selection_length() {
	return text_input_selection_length;
}

void keys::clear_composition() {
	text_input_composition.clear();
	text_input_cursor = 0;
	text_input_selection_start = 0;
	text_input_selection_length = 0;
}
