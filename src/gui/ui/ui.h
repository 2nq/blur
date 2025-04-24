#pragma once

#include "../render/render.h"

namespace ui {
	struct Padding {
		int top = 0;
		int right = 0;
		int bottom = 0;
		int left = 0;

		Padding(int t, int r, int b, int l) : top(t), right(r), bottom(b), left(l) {}

		Padding(int t, int lr) : top(t), right(lr), bottom(t), left(lr) {}

		Padding(int all) : top(all), right(all), bottom(all), left(all) {}
	};

	enum class ElementType : std::uint8_t {
		BAR,
		TEXT,
		BUTTON,
		NOTIFICATION,
		SLIDER,
		TEXT_INPUT,
		CHECKBOX,
		DROPDOWN,
		SEPARATOR
	};

	struct BarElementData {
		float percent_fill;
		gfx::Color background_color;
		gfx::Color fill_color;
		std::optional<std::string> bar_text;
		std::optional<gfx::Color> text_color;
		std::optional<const render::Font*> font;

		bool operator==(const BarElementData& other) const {
			return percent_fill == other.percent_fill && background_color == other.background_color &&
			       fill_color == other.fill_color && bar_text == other.bar_text && text_color == other.text_color &&
			       font == other.font;
		}
	};

	struct TextElementData {
		std::vector<std::string> lines;
		gfx::Color color;
		const render::Font* font;
		unsigned int flags;

		bool operator==(const TextElementData& other) const {
			return lines == other.lines && color == other.color && font == other.font && flags == other.flags;
		}
	};

	enum class SeparatorStyle : std::uint8_t {
		NORMAL,
		FADE_RIGHT,
		FADE_LEFT,
		FADE_BOTH
	};

	struct SeparatorElementData {
		SeparatorStyle style;

		bool operator==(const SeparatorElementData& other) const {
			return style == other.style;
		}
	};

	struct ButtonElementData {
		std::string text;
		const render::Font* font;
		std::optional<std::function<void()>> on_press;

		bool operator==(const ButtonElementData& other) const {
			return text == other.text && font == other.font;
		}
	};

	enum class NotificationType : uint8_t {
		INFO,
		SUCCESS,
		NOTIF_ERROR
	};

	struct NotificationElementData {
		std::vector<std::string> lines;
		NotificationType type;
		const render::Font* font;
		int line_height;
		std::optional<std::function<void()>> on_click;

		bool operator==(const NotificationElementData& other) const {
			return lines == other.lines && type == other.type && font == other.font && line_height == other.line_height;
		}
	};

	struct SliderElementData {
		std::variant<int, float> min_value;
		std::variant<int, float> max_value;
		std::variant<int*, float*> current_value;
		std::string label_format;
		const render::Font* font;
		std::optional<std::function<void(const std::variant<int*, float*>&)>> on_change;
		float precision;
		std::string tooltip;

		bool operator==(const SliderElementData& other) const {
			return min_value == other.min_value && max_value == other.max_value &&
			       current_value == other.current_value && label_format == other.label_format && font == other.font &&
			       precision == other.precision && tooltip == other.tooltip;
		}
	};

	struct TextInputElementData {
		std::string* text;
		std::string placeholder;
		const render::Font* font;
		std::optional<std::function<void(const std::string&)>> on_change;

		bool operator==(const TextInputElementData& other) const {
			return text == other.text && placeholder == other.placeholder && font == other.font;
		}
	};

	struct CheckboxElementData {
		std::string label;
		bool* checked;
		const render::Font* font;
		std::optional<std::function<void(bool)>> on_change;

		bool operator==(const CheckboxElementData& other) const {
			return label == other.label && checked == other.checked && font == other.font;
		}
	};

	struct DropdownElementData {
		std::string label;
		std::vector<std::string> options;
		std::string* selected;
		const render::Font* font;
		std::optional<std::function<void(std::string*)>> on_change;

		bool operator==(const DropdownElementData& other) const {
			return label == other.label && options == other.options && selected == other.selected && font == other.font;
		}
	};

	using ElementData = std::variant<
		BarElementData,
		TextElementData,
		ButtonElementData,
		NotificationElementData,
		SliderElementData,
		TextInputElementData,
		CheckboxElementData,
		DropdownElementData,
		SeparatorElementData>;

	struct AnimationState {
		float speed;
		float current = 0.f;
		float goal = 0.f;
		bool complete = false;

		AnimationState(float speed, float value) : speed(speed), current(value), goal(value) {}

		// delete default constructor since we always need a duration
		AnimationState() = delete;

		void set_goal(float goal) {
			this->goal = goal;
		}

		bool update(float delta_time) {
			float old_current = current;
			current = std::clamp(u::lerp(current, goal, speed * delta_time, 0.001f), 0.f, 1.f);

			complete = current == goal;

			return current != old_current;
		}
	};

	struct AnimationInitialisation {
		float speed;
		float value = 0.f;
	};

	struct AnimatedElement;

	struct Container;

	struct Element {
		std::string id;
		ElementType type;
		gfx::Rect rect;
		ElementData data;
		std::function<void(const Container&, const AnimatedElement&)> render_fn;
		std::optional<std::function<bool(const Container&, AnimatedElement&)>> update_fn;
		bool fixed = false;
		gfx::Rect orig_rect;

		Element(
			std::string id,
			ElementType type,
			const gfx::Rect& rect,
			ElementData data,
			std::function<void(const Container&, const AnimatedElement&)> render_fn,
			std::optional<std::function<bool(const Container&, AnimatedElement&)>> update_fn = std::nullopt,
			bool fixed = false
		)
			: id(std::move(id)), type(type), rect(rect), data(std::move(data)), render_fn(std::move(render_fn)),
			  update_fn(std::move(update_fn)), fixed(fixed), orig_rect(rect) {}
	};

	struct AnimatedElement {
		std::unique_ptr<Element> element;
		std::unordered_map<size_t, AnimationState> animations;
		int z_index = 0;
	};

	const inline AnimationInitialisation DEFAULT_ANIMATION = { .speed = 25.f };

	struct Container {
		gfx::Rect rect;
		std::optional<gfx::Color> background_color;
		std::unordered_map<std::string, AnimatedElement> elements;
		std::vector<std::string> current_element_ids;

		int element_gap = 15;
		float line_height = 1.2f;

		gfx::Point current_position;
		std::optional<Padding> padding;
		bool updated = false;
		int last_margin_bottom = 0;

		float scroll_y = 0.f;
		float scroll_speed_y = 0.f;

		[[nodiscard]] gfx::Rect get_usable_rect() const {
			gfx::Rect usable = rect;
			if (padding) {
				usable.x += padding->left;
				usable.y += padding->top;
				usable.w -= padding->left + padding->right;
				usable.h -= padding->top + padding->bottom;
			}
			return usable;
		}

		std::stack<int> element_gaps;

		void push_element_gap(int new_element_gap) {
			element_gaps.push(element_gap);
			element_gap = new_element_gap;
		}

		void pop_element_gap() {
			int new_element_gap = element_gaps.top();
			element_gap = new_element_gap;
			element_gaps.pop();
		}
	};

	inline auto hasher = std::hash<std::string>{};

	inline AnimatedElement* active_element = nullptr;

	inline const auto HIGHLIGHT_COLOR = gfx::Color(133, 24, 16, 255);
	inline const int TYPE_SWITCH_PADDING = 5;

	void render_bar(const Container& container, const AnimatedElement& element);

	void render_text(const Container& container, const AnimatedElement& element);

	void render_button(const Container& container, const AnimatedElement& element);
	bool update_button(const Container& container, AnimatedElement& element);

	void render_notification(const Container& container, const AnimatedElement& element);
	bool update_notification(const Container& container, AnimatedElement& element);

	void render_slider(const Container& container, const AnimatedElement& element);
	bool update_slider(const Container& container, AnimatedElement& element);

	void render_text_input(const Container& container, const AnimatedElement& element);
	bool update_text_input(const Container& container, AnimatedElement& element);

	void render_checkbox(const Container& container, const AnimatedElement& element);
	bool update_checkbox(const Container& container, AnimatedElement& element);

	void render_dropdown(const Container& container, const AnimatedElement& element);
	bool update_dropdown(const Container& container, AnimatedElement& element);

	void render_separator(const Container& container, const AnimatedElement& element);

	void reset_container(
		Container& container,
		const gfx::Rect& rect,
		int element_gap,
		const std::optional<Padding>& padding = {},
		float line_height = 1.2f,
		std::optional<gfx::Color> background_color = {}
	);

	Element* add_element(
		Container& container,
		Element&& _element,
		int margin_bottom,
		const std::unordered_map<size_t, AnimationInitialisation>& animations = { { hasher("main"),
	                                                                                DEFAULT_ANIMATION } }
	);
	Element* add_element(
		Container& container,
		Element&& _element,
		const std::unordered_map<size_t, AnimationInitialisation>& animations = { { hasher("main"),
	                                                                                DEFAULT_ANIMATION } }
	);

	// elements
	Element& add_bar(
		const std::string& id,
		Container& container,
		float percent_fill,
		gfx::Color background_color,
		gfx::Color fill_color,
		int bar_width,
		std::optional<std::string> bar_text = {},
		std::optional<gfx::Color> text_color = {},
		std::optional<const render::Font*> font = {}
	);

	Element& add_text(
		const std::string& id,
		Container& container,
		const std::string& text,
		gfx::Color color,
		const render::Font& font,
		unsigned int flags = EFontFlags::FONT_NONE
	);

	Element& add_text(
		const std::string& id,
		Container& container,
		std::vector<std::string> lines,
		gfx::Color color,
		const render::Font& font,
		unsigned int flags = EFontFlags::FONT_NONE
	);

	Element& add_text_fixed(
		const std::string& id,
		Container& container,
		const gfx::Point& position,
		const std::string& text,
		gfx::Color color,
		const render::Font& font,
		unsigned int flags = EFontFlags::FONT_NONE
	);

	Element& add_text_fixed(
		const std::string& id,
		Container& container,
		const gfx::Point& position,
		std::vector<std::string> lines,
		gfx::Color color,
		const render::Font& font,
		unsigned int flags = EFontFlags::FONT_NONE
	);

	Element& add_button(
		const std::string& id,
		Container& container,
		const std::string& text,
		const render::Font& font,
		std::optional<std::function<void()>> on_press = {}
	);

	Element& add_notification(
		const std::string& id,
		Container& container,
		const std::string& text,
		NotificationType type,
		const render::Font& font,
		std::optional<std::function<void()>> on_click = {}
	);

	Element& add_slider(
		const std::string& id,
		Container& container,
		const std::variant<int, float>& min_value,
		const std::variant<int, float>& max_value,
		std::variant<int*, float*> value,
		const std::string& label_format,
		const render::Font& font,
		std::optional<std::function<void(const std::variant<int*, float*>&)>> on_change = {},
		float precision = 0.f,
		const std::string& tooltip = ""
	);

	Element& add_text_input(
		const std::string& id,
		Container& container,
		std::string& text,
		const std::string& placeholder,
		const render::Font& font,
		std::optional<std::function<void(const std::string&)>> on_change = {}
	);

	Element& add_checkbox(
		const std::string& id,
		Container& container,
		const std::string& label,
		bool& checked,
		const render::Font& font,
		std::optional<std::function<void(bool)>> on_change = {}
	);

	Element& add_dropdown(
		const std::string& id,
		Container& container,
		const std::string& label,
		const std::vector<std::string>& options,
		std::string& selected,
		const render::Font& font,
		std::optional<std::function<void(std::string*)>> on_change = {}
	);

	Element& add_separator(const std::string& id, Container& container, SeparatorStyle style);

	void add_spacing(Container& container, int spacing);

	void set_next_same_line(Container& container);

	void center_elements_in_container(Container& container, bool horizontal = true, bool vertical = true);

	std::vector<decltype(Container::elements)::iterator> get_sorted_container_elements(Container& container);

	void set_cursor(SDL_SystemCursor cursor);

	bool set_hovered_element(AnimatedElement& element);
	std::string get_hovered_id();

	bool update_container_input(Container& container);
	void on_update_input_start();
	void on_update_input_end();

	bool update_container_frame(Container& container, float delta_time);
	void on_update_frame_end();

	void render_container(Container& container);

	void on_frame_start();
}
