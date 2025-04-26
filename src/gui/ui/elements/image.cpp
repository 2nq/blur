#include "../ui.h"
#include "../../render/render.h"

static const float IMAGE_ROUNDING = 5.f;

// Cache for textures to avoid reloading
namespace {
	std::unordered_map<std::string, std::shared_ptr<render::Texture>> texture_cache;

	std::shared_ptr<render::Texture> get_or_load_texture(const std::filesystem::path& path, const std::string& id) {
		// Use a combined key of path and id for caching
		std::string cache_key = path.string() + "_" + id;

		auto it = texture_cache.find(cache_key);
		if (it != texture_cache.end()) {
			return it->second;
		}

		// Load new texture
		auto texture = std::make_shared<render::Texture>();
		if (texture->load_from_file(path.string())) {
			texture_cache[cache_key] = texture;
			return texture;
		}

		return nullptr;
	}
}

// Updated ImageElementData to use our texture system
struct ImageElementData {
	std::filesystem::path image_path;
	std::shared_ptr<render::Texture> texture;
	std::string image_id;
	gfx::Color image_color;
};

void ui::render_image(const Container& container, const AnimatedElement& element) {
	const auto& image_data = std::get<ImageElementData>(element.element->data);
	float anim = element.animations.at(hasher("main")).current;

	int alpha = anim * 255;
	int stroke_alpha = anim * 125;

	// Create tinted color with animation alpha
	gfx::Color tint_color = image_data.image_color.adjust_alpha(anim);

	// Base rectangle
	gfx::Rect image_rect = element.element->rect;

	// Draw the image with borders
	render::image_with_borders(
		image_rect,
		*image_data.texture,
		IMAGE_ROUNDING,
		gfx::Color(155, 155, 155, stroke_alpha),
		1.0f,
		ROUNDING_ALL,
		tint_color
	);
}

std::optional<ui::Element*> ui::add_image(
	const std::string& id,
	Container& container,
	const std::filesystem::path& image_path,
	const gfx::Size& max_size,
	std::string image_id,
	gfx::Color image_color
) {
	std::shared_ptr<render::Texture> texture;
	std::shared_ptr<render::Texture> last_texture;

	// Check if we already have this element
	if (container.elements.contains(id)) {
		Element& cached_element = *container.elements[id].element;
		auto& image_data = std::get<ImageElementData>(cached_element.data);

		if (image_data.image_id == image_id) {
			// Reuse the existing texture if ID matches
			texture = image_data.texture;
		}
		else {
			// Keep the last texture as fallback
			last_texture = image_data.texture;
		}
	}

	// Load image if new
	if (!texture) {
		texture = get_or_load_texture(image_path, image_id);

		if (!texture) {
			u::log("{} failed to load image (id: {})", id, image_id);
			if (last_texture) {
				// Use last image as fallback
				texture = last_texture;
			}
			else {
				return {};
			}
		}

		u::log("{} loaded image (id: {})", id, image_id);
	}

	gfx::Rect image_rect(container.current_position, max_size);

	// Calculate aspect ratio and adjust dimensions
	float aspect_ratio = texture->width() / static_cast<float>(texture->height());

	float target_width = image_rect.h * aspect_ratio;
	float target_height = image_rect.w / aspect_ratio;

	if (target_width <= image_rect.w) {
		image_rect.w = static_cast<int>(target_width);
	}
	else {
		image_rect.h = static_cast<int>(target_height);
	}

	if (image_rect.h > max_size.h) {
		image_rect.h = max_size.h;
		image_rect.w = static_cast<int>(max_size.h * aspect_ratio);
	}

	if (image_rect.w > max_size.w) {
		image_rect.w = max_size.w;
		image_rect.h = static_cast<int>(max_size.w / aspect_ratio);
	}

	Element element(
		id,
		ElementType::IMAGE,
		image_rect,
		ImageElementData{
			.image_path = image_path,
			.texture = texture,
			.image_id = image_id,
			.image_color = image_color,
		},
		render_image
	);

	return add_element(container, std::move(element), container.element_gap);
}
