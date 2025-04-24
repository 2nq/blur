
#pragma once

namespace gfx {
	class Point;
	class Rect;

	class Size {
	public:
		int w = 0;
		int h = 0;

		constexpr Size() = default;

		constexpr Size(int w, int h) : w(w), h(h) {}

		// Core operations
		[[nodiscard]] constexpr Size expand(int amount) const {
			return { w + amount, h + amount };
		}

		[[nodiscard]] constexpr Size shrink(int amount) const {
			return expand(-amount);
		}

		[[nodiscard]] constexpr bool is_zero() const {
			return w == 0 && h == 0;
		}

		[[nodiscard]] constexpr Point to_point() const;

		[[nodiscard]] constexpr float aspect_ratio() const {
			return static_cast<float>(w) / h;
		}

		[[nodiscard]] constexpr int area() const {
			return w * h;
		}

		// Operators
		constexpr bool operator==(const Size& other) const {
			return w == other.w && h == other.h;
		}

		constexpr bool operator!=(const Size& other) const {
			return !(*this == other);
		}

		constexpr Size& operator+=(const Size& other) {
			w += other.w;
			h += other.h;
			return *this;
		}

		constexpr Size operator+(const Size& other) const {
			Size result = *this;
			result += other;
			return result;
		}

		constexpr Size& operator-=(const Size& other) {
			w -= other.w;
			h -= other.h;
			return *this;
		}

		constexpr Size operator-(const Size& other) const {
			Size result = *this;
			result -= other;
			return result;
		}

		constexpr Size operator-() const {
			return { -w, -h };
		}

		constexpr Size& operator*=(float scale) {
			w = static_cast<int>(w * scale);
			h = static_cast<int>(h * scale);
			return *this;
		}

		constexpr Size operator*(float scale) const {
			Size result = *this;
			result *= scale;
			return result;
		}

		constexpr Size& operator/=(float divisor) {
			w = static_cast<int>(w / divisor);
			h = static_cast<int>(h / divisor);
			return *this;
		}

		constexpr Size operator/(float divisor) const {
			Size result = *this;
			result /= divisor;
			return result;
		}
	};
}
