#include "tasks.h"
#include "gui.h"

#include <SDL3/SDL_main.h>

int main(int argc, char* argv[]) {
	std::vector<std::string> arguments(argv + 1, argv + argc);

	std::thread(tasks::run, arguments).detach();

	gui::run();

	return 0;
}
