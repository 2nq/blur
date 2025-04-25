#include <utility>

#include "../ui.h"
#include "../../render/render.h"

#include "../keys.h"

const gfx::Size NOTIFICATION_TEXT_PADDING = { 10, 7 };
static const float NOTIFICATION_ROUNDING = 7.f;

void ui::render_notification(const Container& container, const AnimatedElement& element) {
	const auto& notification_data = std::get<NotificationElementData>(element.element->data);
	float anim = element.animations.at(hasher("main")).current;
	float hover_anim = element.animations.at(hasher("hover")).current;

	gfx::Color notification_color = 0;
	gfx::Color border_color = 0;
	gfx::Color text_color = 0;

	switch (notification_data.type) {
		case NotificationType::SUCCESS: {
			notification_color = gfx::Color(0, 35, 0, 255);
			border_color = gfx::Color(80, 100, 80, 255);
			text_color = gfx::Color(100, 255, 100, 255);
			break;
		}
		case NotificationType::NOTIF_ERROR: {
			notification_color = gfx::Color(35, 0, 0, 255);
			border_color = gfx::Color(100, 80, 80, 255);
			text_color = gfx::Color(255, 100, 100, 255);
			break;
		}
		case NotificationType::INFO:
		default: {
			notification_color = gfx::Color(35, 35, 35, 255);
			border_color = gfx::Color(100, 100, 100, 255);
			text_color = gfx::Color(255, 255, 255, 255);
			break;
		}
	};

	anim *= 1.f - (hover_anim * 0.2);

	notification_color = notification_color.adjust_alpha(anim);
	border_color = border_color.adjust_alpha(anim);
	text_color = text_color.adjust_alpha(anim);

	// fill
	render::rounded_rect_filled(element.element->rect, notification_color, NOTIFICATION_ROUNDING);

	// border
	render::rounded_rect_stroke(element.element->rect, border_color, NOTIFICATION_ROUNDING);

	gfx::Point text_pos = element.element->rect.origin();
	text_pos.x += NOTIFICATION_TEXT_PADDING.w;
	text_pos.y += NOTIFICATION_TEXT_PADDING.h;

	for (const auto& line : notification_data.lines) {
		render::text(text_pos, text_color, line, *notification_data.font); // TODO: align properly
		text_pos.y += notification_data.line_height;
	}
}

bool ui::update_notification(const Container& container, AnimatedElement& element) {
	const auto& notification_data = std::get<NotificationElementData>(element.element->data);

	if (notification_data.on_click) {
		bool hovered = element.element->rect.contains(keys::mouse_pos) && set_hovered_element(element);

		auto& anim = element.animations.at(hasher("hover"));
		anim.set_goal(hovered ? 1.f : 0.f);

		if (hovered) {
			set_cursor(SDL_SYSTEM_CURSOR_POINTER);

			if (keys::is_mouse_down()) {
				(*notification_data.on_click)();
				keys::on_mouse_press_handled(SDL_BUTTON_LEFT);

				return true;
			}
		}
	}

	return false;
}

ui::Element& ui::add_notification(
	const std::string& id,
	Container& container,
	const std::string& text,
	ui::NotificationType type,
	const render::Font& font,
	std::optional<std::function<void()>> on_click
) {
	gfx::Size notification_size = { 230, 100 }; // height is a maximum to start with

	const int line_height = font.height() + 5;

	std::vector<std::string> lines =
		render::wrap_text(text, notification_size - (NOTIFICATION_TEXT_PADDING * 2), font, line_height);

	notification_size.h = lines.size() * line_height;
	notification_size.h += NOTIFICATION_TEXT_PADDING.h * 2;

	// now that we've calculated the notification size, add text padding

	Element element(
		id,
		ElementType::NOTIFICATION,
		gfx::Rect(container.current_position, notification_size),
		NotificationElementData{
			.lines = lines,
			.type = type,
			.font = &font,
			.line_height = line_height,
			.on_click = std::move(on_click),
		},
		render_notification,
		update_notification
	);

	return *add_element(
		container,
		std::move(element),
		container.element_gap,
		{
			{ hasher("main"), { .speed = 5.f } },
			{ hasher("hover"), { .speed = 80.f } },
		}
	);
}
