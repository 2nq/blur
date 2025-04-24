#include "../ui.h"
#include "../../render/render.h"
#include "../keys.h"

const int LABEL_GAP = 10;
const gfx::Size TEXT_INPUT_PADDING(10, 5);

namespace {
	struct Positions {
		gfx::Point label_pos;
		gfx::Rect input_rect;
	};

	Positions get_positions(
		const ui::Container& container, const ui::TextInputElementData& input_data, const ui::AnimatedElement& element
	) {
		gfx::Point label_pos = element.element->rect.origin();

		auto input_rect = element.element->rect;
		input_rect.y = label_pos.y + LABEL_GAP;
		input_rect.h -= input_rect.y - element.element->rect.y;

		return {
			.label_pos = label_pos,
			.input_rect = input_rect,
		};
	}
}

void ui::render_text_input(const Container& container, const AnimatedElement& element) {
	const auto& input_data = std::get<TextInputElementData>(element.element->data);

	float anim = element.animations.at(hasher("main")).current;
	float hover_anim = element.animations.at(hasher("hover")).current;
	float focus_anim = element.animations.at(hasher("focus")).current;

	auto pos = get_positions(container, input_data, element);

	render::text(pos.label_pos, gfx::Color(255, 255, 255, anim * 255), input_data.placeholder, *input_data.font);

	// TODO: use sdl stuff for text input and port back rendering
	render::rect_filled(element.element->rect, gfx::Color::red(100));
}

bool ui::update_text_input(const Container& container, AnimatedElement& element) {
	auto& input_data = std::get<TextInputElementData>(element.element->data);
	auto& hover_anim = element.animations.at(hasher("hover"));
	auto& focus_anim = element.animations.at(hasher("focus"));

	auto pos = get_positions(container, input_data, element);

	bool hovered = pos.input_rect.contains(keys::mouse_pos) && set_hovered_element(element);

	hover_anim.set_goal(hovered ? 1.f : 0.f);

	if (hovered)
		set_cursor(SDL_SYSTEM_CURSOR_TEXT);

	// Handle mouse click to focus/unfocus
	if (hovered && keys::is_mouse_down()) {
		active_element = &element;
		focus_anim.set_goal(1.f);
	}
	else if (keys::is_mouse_down() && !hovered) {
		active_element = nullptr;
		focus_anim.set_goal(0.f);
	}

	bool active = active_element == &element;

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
	const gfx::Size input_size(200, font.height() + LABEL_GAP + font.height() + (TEXT_INPUT_PADDING.h * 2));

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
