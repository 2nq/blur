#pragma once

namespace gui {
	inline std::optional<Blur::InitialisationResponse> initialisation_res;

	inline bool stop = false;
	inline bool to_render = true;

	inline bool dragging = false;

	void event_loop();
	int run();
}
