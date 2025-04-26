#include "../ui.h"
#include "../../render/render.h"
#include "../keys.h"

const float SLIDER_ROUNDING = 4.0f;
const float HANDLE_WIDTH = 10;
const int TRACK_HEIGHT = 4;
const int LINE_HEIGHT_ADD = 7;
const int TRACK_LABEL_GAP = 10;
const int TOOLTIP_GAP = 4;

namespace {
	struct SliderPositions {
		gfx::Point label_pos;
		gfx::Point tooltip_pos;
		gfx::Rect track_rect;
	};

	SliderPositions get_slider_positions(
		const ui::Container& container, const ui::AnimatedElement& element, const ui::SliderElementData& slider_data
	) {
		gfx::Rect track_rect = element.element->rect;
		track_rect.h = TRACK_HEIGHT;

		gfx::Point label_pos = track_rect.origin();
		gfx::Point tooltip_pos = track_rect.origin();

		if (!slider_data.tooltip.empty()) {
			tooltip_pos.y += slider_data.font->height() + TOOLTIP_GAP;
			track_rect.y += slider_data.font->height() + TOOLTIP_GAP;
		}

		track_rect.y += slider_data.font->height() + TRACK_LABEL_GAP;

		return {
			.label_pos = label_pos,
			.tooltip_pos = tooltip_pos,
			.track_rect = track_rect,
		};
	}
}

void ui::render_slider(const Container& container, const AnimatedElement& element) {
	const auto& slider_data = std::get<SliderElementData>(element.element->data);
	float anim = element.animations.at(hasher("main")).current;
	float hover_anim = element.animations.at(hasher("hover")).current;

	// Get current value as float
	float current_val = std::visit(
		[](auto&& arg) {
			return static_cast<float>(*arg);
		},
		slider_data.current_value
	);
	float min_val = std::visit(
		[](auto&& arg) {
			return static_cast<float>(arg);
		},
		slider_data.min_value
	);
	float max_val = std::visit(
		[](auto&& arg) {
			return static_cast<float>(arg);
		},
		slider_data.max_value
	);

	// Normalize progress
	float progress = std::clamp((current_val - min_val) / (max_val - min_val), 0.f, 1.f);

	int track_shade = 40 + (20 * hover_anim);
	gfx::Color track_color(track_shade, track_shade, track_shade, anim * 255);
	gfx::Color filled_color = HIGHLIGHT_COLOR.adjust_alpha(anim);
	gfx::Color handle_border_color(0, 0, 0, anim * 50);
	gfx::Color text_color(255, 255, 255, anim * 255);
	gfx::Color tooltip_color(125, 125, 125, anim * 255);

	auto positions = get_slider_positions(container, element, slider_data);

	// Render label with formatted value
	std::string label;
	std::visit(
		[&](auto&& arg) {
			label = std::vformat(slider_data.label_format, std::make_format_args(*arg));
		},
		slider_data.current_value
	);
	render::text(positions.label_pos, text_color, label, *slider_data.font);

	// Render tooltip if provided
	if (!slider_data.tooltip.empty()) {
		render::text(positions.tooltip_pos, tooltip_color, slider_data.tooltip, *slider_data.font);
	}

	// Render track and filled portion
	render::rect_filled(positions.track_rect, track_color);

	gfx::Rect filled_rect = positions.track_rect;
	filled_rect.w *= progress;
	render::rect_filled(filled_rect, filled_color);

	// Render handle
	if (progress >= 0.f && progress <= 1.f) {
		float handle_x = positions.track_rect.x + (positions.track_rect.w * progress) - (HANDLE_WIDTH / 2);
		gfx::Rect handle_rect(
			handle_x, positions.track_rect.y - ((HANDLE_WIDTH - TRACK_HEIGHT) / 2), HANDLE_WIDTH, HANDLE_WIDTH
		);

		render::rounded_rect_filled(handle_rect.expand(1), handle_border_color, HANDLE_WIDTH);
		render::rounded_rect_filled(handle_rect, filled_color, HANDLE_WIDTH);
	}
}

bool ui::update_slider(const Container& container, AnimatedElement& element) {
	auto& slider_data = std::get<SliderElementData>(element.element->data);
	auto& hover_anim = element.animations.at(hasher("hover"));
	auto positions = get_slider_positions(container, element, slider_data);

	// Create slightly larger clickable area
	auto clickable_rect = element.element->rect;
	int extra = (HANDLE_WIDTH / 2) - (positions.track_rect.h / 2);
	if (extra > 0)
		clickable_rect.h += extra;

	bool hovered = clickable_rect.contains(keys::mouse_pos) && set_hovered_element(element);
	bool active = active_element == &element;

	// Get min/max values
	float min_val = std::visit(
		[](auto&& arg) {
			return static_cast<float>(arg);
		},
		slider_data.min_value
	);
	float max_val = std::visit(
		[](auto&& arg) {
			return static_cast<float>(arg);
		},
		slider_data.max_value
	);

	if (hovered) {
		set_cursor(SDL_SYSTEM_CURSOR_POINTER);
		if (keys::is_mouse_down())
			active_element = &element;
	}

	hover_anim.set_goal(hovered || active ? 1.f : 0.f);

	if (active_element == &element && keys::is_mouse_down()) {
		float mouse_progress =
			positions.track_rect.mouse_percent_x(keys::pressing_keys.contains(SDL_Scancode::SDL_SCANCODE_LSHIFT));

		float new_val = min_val + (mouse_progress * (max_val - min_val));

		// Apply snapping precision
		if (slider_data.precision > 0.0f) {
			new_val = std::round(new_val / slider_data.precision) * slider_data.precision;
		}

		// Update value based on its type
		if (auto* ptr = std::get_if<int*>(&slider_data.current_value)) {
			**ptr = static_cast<int>(new_val);
		}
		else if (auto* ptr = std::get_if<float*>(&slider_data.current_value)) {
			**ptr = new_val;
		}

		// Call on_change callback if provided
		if (slider_data.on_change) {
			(*slider_data.on_change)(slider_data.current_value);
		}

		return true;
	}
	else if (active_element == &element) {
		active_element = nullptr;
	}

	return false;
}

ui::Element& ui::add_slider(
	const std::string& id,
	Container& container,
	const std::variant<int, float>& min_value,
	const std::variant<int, float>& max_value,
	std::variant<int*, float*> value,
	const std::string& label_format,
	const render::Font& font,
	std::optional<std::function<void(const std::variant<int*, float*>&)>> on_change,
	float precision,
	const std::string& tooltip
) {
	gfx::Size slider_size(200, font.height() + TRACK_LABEL_GAP + TRACK_HEIGHT);
	if (!tooltip.empty())
		slider_size.h += LINE_HEIGHT_ADD + font.height();

	Element element(
		id,
		ElementType::SLIDER,
		gfx::Rect(container.current_position, slider_size),
		SliderElementData{
			.min_value = min_value,
			.max_value = max_value,
			.current_value = value,
			.label_format = label_format,
			.font = &font,
			.on_change = std::move(on_change),
			.precision = precision,
			.tooltip = tooltip,
		},
		render_slider,
		update_slider
	);

	return *add_element(
		container,
		std::move(element),
		container.element_gap,
		{
			{ hasher("main"), { .speed = 25.f } },
			{ hasher("hover"), { .speed = 80.f } },
		}
	);
}
