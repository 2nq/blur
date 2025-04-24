#include "tasks.h"

#if defined(WIN32) || defined(__APPLE__)
// dont need launcher
#else
#	include "base/launcher.h"
#endif

#include <common/rendering.h>
#include "gui.h"
#include "gui/renderer.h"
#include "gui/ui/ui.h"

void tasks::run(const std::vector<std::string>& arguments) {
	auto res = blur.initialise(false, true);

	gui::initialisation_res = res;

	rendering.set_progress_callback([] {
		std::optional<Render*> current_render_opt = rendering.get_current_render();
		if (current_render_opt) {
			Render& current_render = **current_render_opt;

			RenderStatus status = current_render.get_status();
			if (status.finished) {
				u::log("current render is finished, copying it so its final state can be displayed once by gui");

				// its about to be deleted, store a copy to be rendered at least once
				gui::renderer::current_render_copy = current_render;
				gui::to_render = true;
			}
		}

		// idk what you're supposed to do to trigger a redraw in a separate thread!!! I dont do gui!!! this works
		// tho :  ) todo: revisit this
		// TODO PORT:
	});

	rendering.set_render_finished_callback([](Render* render, const RenderResult& result) {
		gui::renderer::on_render_finished(render, result);
	});

	auto update_res = Blur::check_updates();
	if (update_res.success && !update_res.is_latest) {
		static const auto update_notification_duration = std::chrono::duration<float>(15.f);

#if defined(WIN32) || defined(__APPLE__)
		gui::renderer::add_notification(
			std::format("There's a newer version ({}) available! Click to run the installer.", update_res.latest_tag),
			ui::NotificationType::INFO,
			[&] {
				const static std::string update_notification_id = "update progress notification";

				gui::renderer::add_notification(
					update_notification_id, "Downloading update...", ui::NotificationType::INFO
				);

				std::thread([update_res] {
					Blur::update(update_res.latest_tag, [](const std::string& text) {
						gui::renderer::add_notification(update_notification_id, text, ui::NotificationType::INFO);
					});

					gui::stop = true;
				}).detach();
			},
			update_notification_duration
		);
#else
		gui::renderer::add_notification(
			std::format(
				"There's a newer version ({}) available! Click to go to the download page.", update_res.latest_tag
			),
			ui::NotificationType::INFO,
			[&] {
				base::launcher::open_url(update_res.latest_tag_url);
			},
			update_notification_duration
		);
#endif
	}

	std::vector<std::wstring> wargs;
	for (const auto argument : arguments) {
		wargs.push_back(u::towstring(argument));
	}

	add_files(wargs); // todo: mac packaged app support (& linux? does it work?)

	while (!gui::stop) {
		rendering.render_videos();
	}
}

void tasks::add_files(const std::vector<std::wstring>& path_strs) {
	for (const std::wstring& path_str : path_strs) {
		std::filesystem::path path = std::filesystem::canonical(path_str);
		if (path.empty() || !std::filesystem::exists(path))
			continue;

		auto video_info = u::get_video_info(path);
		if (!video_info.has_video_stream) {
			gui::renderer::add_notification(
				std::format("File is not a valid video or is unreadable: {}", u::tostring(path.wstring())),
				ui::NotificationType::NOTIF_ERROR
			);
			continue;
		}

		u::log(L"queueing {}", path.wstring());

		Render render(path, video_info);

		if (gui::renderer::screen != gui::renderer::Screens::MAIN) {
			gui::renderer::add_notification(
				std::format("Queued '{}' for rendering", u::tostring(render.get_video_name())),
				ui::NotificationType::INFO
			);
		}

		rendering.queue_render(std::move(render));
	}
}

void tasks::add_sample_video(const std::wstring& path_str) {
	std::filesystem::path path = std::filesystem::canonical(path_str);
	if (path.empty() || !std::filesystem::exists(path))
		return;

	auto sample_video_path = blur.settings_path / "sample_video.mp4";

	// todo: reencode?
	std::filesystem::copy(path, sample_video_path);

	gui::renderer::add_notification("Added sample video", ui::NotificationType::SUCCESS);

	gui::renderer::just_added_sample_video = true;
}
