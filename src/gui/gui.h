#pragma once

namespace gui {
	inline std::optional<Blur::InitialisationResponse> initialisation_res;

	inline bool stop = false;
	inline bool to_render = true;

	void event_loop();
	int run();
}
