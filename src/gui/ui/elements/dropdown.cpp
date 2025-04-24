#include "../ui.h"
#include "../../render/render.h"

#include "../keys.h"

const float DROPDOWN_ROUNDING = 5.f;
const gfx::Size DROPDOWN_PADDING(10, 5);
const float OPTION_LINE_HEIGHT_ADD = 11;
const float DROPDOWN_ARROW_SIZE = 10.0f;
const int LABEL_GAP = 10;
const int OPTIONS_GAP = 3;
const gfx::Size OPTIONS_PADDING(10, 3);

namespace {
	struct Positions {
		int font_height;
		gfx::Point label_pos;
		gfx::Rect dropdown_rect;
		gfx::Point selected_text_pos;
		gfx::Rect options_rect;
		float option_line_height;
	};

	Positions get_positions(
		const ui::Container& container,
		const ui::AnimatedElement& element,
		const ui::DropdownElementData& dropdown_data,
		float anim = 1.f
	) {
		gfx::Point label_pos = element.element->rect.origin();

		gfx::Rect dropdown_rect = element.element->rect;
		dropdown_rect.y = label_pos.y + dropdown_data.font->height() + LABEL_GAP;
		dropdown_rect.h -= dropdown_rect.y - element.element->rect.y;

		gfx::Point selected_text_pos = dropdown_rect.origin();
		selected_text_pos.x += DROPDOWN_PADDING.w;
		selected_text_pos.y = dropdown_rect.center().y;

		float option_line_height = dropdown_data.font->height() + OPTION_LINE_HEIGHT_ADD;

		gfx::Rect options_rect = element.element->rect;
		options_rect.y = options_rect.y2() + OPTIONS_GAP;
		options_rect.h = option_line_height * dropdown_data.options.size() + OPTIONS_PADDING.h * 2;

		if (options_rect.y + options_rect.h + OPTIONS_GAP > container.get_usable_rect().y2() &&
		    options_rect.y - options_rect.h - OPTIONS_GAP > 0)
		{
			// open upwards
			options_rect.h *= anim;
			options_rect.y = dropdown_rect.y - options_rect.h - OPTIONS_GAP;
		}
		else {
			options_rect.h *= anim;
		}

		return {
			.label_pos = label_pos,
			.dropdown_rect = dropdown_rect,
			.selected_text_pos = selected_text_pos,
			.options_rect = options_rect,
			.option_line_height = option_line_height,
		};
	}
}

void ui::render_dropdown(const Container& container, const AnimatedElement& element) {
	const auto& dropdown_data = std::get<DropdownElementData>(element.element->data);

	float anim = element.animations.at(hasher("main")).current;
	float hover_anim = element.animations.at(hasher("hover")).current;
	float expand_anim = element.animations.at(hasher("expand")).current;

	auto pos = get_positions(container, element, dropdown_data, expand_anim);

	// Background color
	int background_shade = 15 + (10 * hover_anim);
	int border_shade = 70;

	gfx::Color adjusted_color(background_shade, background_shade, background_shade, anim * 255);
	gfx::Color text_color(255, 255, 255, anim * 255);
	gfx::Color selected_text_color(255, 100, 100, anim * 255);
	gfx::Color border_color(border_shade, border_shade, border_shade, anim * 255);

	render::text(pos.label_pos, text_color, dropdown_data.label, *dropdown_data.font);

	// Render dropdown main area
	render::rounded_rect_filled(pos.dropdown_rect, adjusted_color, DROPDOWN_ROUNDING);
	render::rounded_rect_stroke(pos.dropdown_rect, border_color, DROPDOWN_ROUNDING);

	// Get currently selected option text
	render::text(pos.selected_text_pos, text_color, *dropdown_data.selected, *dropdown_data.font, FONT_CENTERED_Y);

	// // Render dropdown arrow
	// gfx::Point arrow_pos(dropdown_rect.x2() - dropdown_arrow_size - 10, dropdown_rect.center().y);
	// std::vector<gfx::Point> arrow_points = {
	//     {arrow_pos.x, arrow_pos.y - dropdown_arrow_size/2},
	//     {arrow_pos.x + dropdown_arrow_size, arrow_pos.y},
	//     {arrow_pos.x, arrow_pos.y + dropdown_arrow_size/2}
	// };
	// render::polygon(arrow_points, text_color);

	// Render dropdown options
	if (expand_anim > 0.01f) {
		gfx::Color option_color(15, 15, 15, anim * 255);
		gfx::Color option_border_color(border_shade, border_shade, border_shade, anim * 255);
		render::rounded_rect_filled(pos.options_rect, option_color, DROPDOWN_ROUNDING);
		render::rounded_rect_stroke(pos.options_rect, option_border_color, DROPDOWN_ROUNDING);

		render::push_clip_rect(pos.options_rect);

		// Render options
		gfx::Point option_text_pos = pos.options_rect.origin();
		option_text_pos.y += OPTIONS_PADDING.h + pos.font_height + OPTION_LINE_HEIGHT_ADD / 2 - 1;
		option_text_pos.x = pos.options_rect.origin().x + OPTIONS_PADDING.w;

		for (const auto& option : dropdown_data.options) {
			bool selected = option == *dropdown_data.selected;

			render::text(option_text_pos, selected ? selected_text_color : text_color, option, *dropdown_data.font);

			option_text_pos.y += pos.option_line_height;
		}

		render::pop_clip_rect();
	}

	// render::rect_stroke(element.element->rect, gfx::Color(255, 0, 0, 100));
}

// NOLINTBEGIN(readability-function-cognitive-complexity)
bool ui::update_dropdown(const Container& container, AnimatedElement& element) {
	const auto& dropdown_data = std::get<DropdownElementData>(element.element->data);

	auto& hover_anim = element.animations.at(hasher("hover"));
	auto& expand_anim = element.animations.at(hasher("expand"));

	auto pos = get_positions(container, element, dropdown_data, expand_anim.current);

	bool hovered = pos.dropdown_rect.contains(keys::mouse_pos) && set_hovered_element(element);
	hover_anim.set_goal(hovered ? 1.f : 0.f);

	bool active = active_element == &element;

	auto toggle_active = [&] {
		if (active) {
			active_element = nullptr;
			active = false;
		}
		else {
			active_element = &element;
			active = true;
		}

		expand_anim.set_goal(active ? 1.f : 0.f);
	};

	bool activated = false;

	if (hovered) {
		set_cursor(SDL_SYSTEM_CURSOR_POINTER);

		if (keys::is_mouse_down()) {
			// toggle dropdown
			toggle_active();
			keys::on_mouse_press_handled(SDL_BUTTON_LEFT);

			activated = true;
		}
	}

	if (!activated && active) {
		if (pos.options_rect.contains(keys::mouse_pos)) {
			set_cursor(SDL_SYSTEM_CURSOR_POINTER);
		}

		// clicking options
		if (keys::is_mouse_down()) {
			if (pos.options_rect.contains(keys::mouse_pos)) {
				size_t clicked_option_index = (keys::mouse_pos.y - pos.options_rect.y) / pos.option_line_height;

				// eat all inputs cause otherwise itll click stuff behind
				keys::on_mouse_press_handled(SDL_BUTTON_LEFT);

				if (clicked_option_index >= 0 && clicked_option_index < dropdown_data.options.size()) {
					std::string new_selected = dropdown_data.options[clicked_option_index];
					if (new_selected != *dropdown_data.selected) {
						*dropdown_data.selected = new_selected;
						toggle_active();

						if (dropdown_data.on_change)
							(*dropdown_data.on_change)(dropdown_data.selected);

						activated = true;
					}
				}
			}
			else {
				toggle_active();
			}
		}
	}

	// update z index
	int z_index = 0;
	if (active)
		z_index = 2;
	else if (expand_anim.current > 0.01f)
		z_index = 1;
	element.z_index = z_index;

	return activated;
}

// NOLINTEND(readability-function-cognitive-complexity)

ui::Element& ui::add_dropdown(
	const std::string& id,
	Container& container,
	const std::string& label,
	const std::vector<std::string>& options,
	std::string& selected,
	const render::Font& font,
	std::optional<std::function<void(std::string*)>> on_change
) {
	// gfx::Size max_text_size(0, font.getSize());

	// // Find max text size for width calculation
	// for (const auto& option : options) {
	// 	gfx::Size text_size = render::get_text_size(option, font);
	// 	max_text_size.w = std::max(max_text_size.w, text_size.w);
	// }

	gfx::Size total_size(200, font.height() + LABEL_GAP + font.height() + (DROPDOWN_PADDING.h * 2));

	Element element(
		id,
		ElementType::DROPDOWN,
		gfx::Rect(container.current_position, total_size),
		DropdownElementData{
			.label = label,
			.options = options,
			.selected = &selected,
			.font = &font,
			.on_change = std::move(on_change),
		},
		render_dropdown,
		update_dropdown
	);

	return *add_element(
		container,
		std::move(element),
		container.element_gap,
		{
			{ hasher("main"), { .speed = 25.f } },
			{ hasher("hover"), { .speed = 80.f } },
			{ hasher("expand"), { .speed = 30.f } },
		}
	);
}
