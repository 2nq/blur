#include <utility>

#include "../ui.h"
#include "../../render/render.h"

void ui::render_separator(const Container& container, const AnimatedElement& element) {
	const auto& separator_data = std::get<SeparatorElementData>(element.element->data);
	float anim = element.animations.at(hasher("main")).current;

	gfx::Color color = gfx::Color(255, 255, 255, anim * 50);

	gfx::Rect separator_rect = element.element->rect;
	separator_rect.y = separator_rect.center().y;
	separator_rect.h = 1;

	switch (separator_data.style) {
		case SeparatorStyle::NORMAL: {
			render::rect_filled(separator_rect, color);
			break;
		}
		case SeparatorStyle::FADE_RIGHT: {
			render::rect_filled_gradient(
				separator_rect, render::GradientDirection::GRADIENT_RIGHT, { color, color.adjust_alpha(0.f) }
			);
			break;
		}
		case SeparatorStyle::FADE_LEFT: {
			render::rect_filled_gradient(
				separator_rect, render::GradientDirection::GRADIENT_LEFT, { color, color.adjust_alpha(0.f) }
			);
			break;
		}
		case SeparatorStyle::FADE_BOTH: {
			auto left_rect = separator_rect;
			left_rect.w /= 2;

			auto right_rect = left_rect;
			right_rect.x += right_rect.w;

			render::rect_filled_gradient(
				left_rect, render::GradientDirection::GRADIENT_LEFT, { color, color.adjust_alpha(0.f) }
			);
			render::rect_filled_gradient(
				right_rect, render::GradientDirection::GRADIENT_RIGHT, { color, color.adjust_alpha(0.f) }
			);
			break;
		}
	}
}

ui::Element& ui::add_separator(const std::string& id, Container& container, SeparatorStyle style) {
	Element element(
		id,
		ElementType::SEPARATOR,
		gfx::Rect(container.current_position, gfx::Size(200, container.element_gap)),
		SeparatorElementData{
			.style = style,
		},
		render_separator
	);

	return *add_element(container, std::move(element), container.element_gap);
}
