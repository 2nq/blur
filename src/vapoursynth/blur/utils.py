import vapoursynth as vs
from vapoursynth import core

from pathlib import Path
from fractions import Fraction
from dataclasses import dataclass


class BlurException(Exception):
    pass


@dataclass
class VideoInfo:
    is_full_color_range: bool
    orig_width: int
    orig_height: int
    resize_chromaloc: str | None
    resize_upscale_factor: float


def load_plugins(extension: str):
    plugin_dir = Path("../vapoursynth-plugins")
    ignored = {
        f"libbestsource{extension}",
    }

    for plugin in plugin_dir.glob(f"*{extension}"):
        if plugin.name not in ignored:
            print("Loading", plugin.name)
            try:
                core.std.LoadPlugin(path=str(plugin))
            except Exception as e:
                print(f"Failed to load plugin {plugin.name}: {e}")


def safe_int(value):
    try:
        return int(value)
    except (TypeError, ValueError):
        return None


def coalesce(value, fallback):
    return fallback if value is None else value


def check_model_path(rife_model_path: str):
    path = Path(rife_model_path)

    if not path.exists():
        raise BlurException(f"RIFE model not found at {path}")


def assume_scaled_fps(clip, timescale):
    new_fps = clip.fps * timescale
    fps_frac = Fraction(new_fps).limit_denominator()

    return core.std.AssumeFPS(
        clip, fpsnum=fps_frac.numerator, fpsden=fps_frac.denominator
    )


def get_scale_factor_for_format(format_id) -> int:
    format_info = core.get_video_format(format_id)

    if format_info.color_family == vs.RGB:
        return 1

    if format_info.color_family == vs.YUV:
        return (2**format_info.subsampling_w) * (2**format_info.subsampling_h)

    return 1


def with_format(
    video: vs.VideoNode,
    video_info: VideoInfo,
    target_format,
    process_func,
):
    orig_format = video.format
    needs_conversion = orig_format.id != target_format

    old_width = None
    old_height = None
    use_point = True

    if needs_conversion:
        ideal_scale_factor = get_scale_factor_for_format(target_format)
        needs_upscale = ideal_scale_factor != 1

        if needs_upscale:
            ideal_width = int(video_info.orig_width * ideal_scale_factor)
            ideal_height = int(video_info.orig_height * ideal_scale_factor)

            if video_info.resize_upscale_factor != 0:
                # upscale to avoid chroma loss
                user_scale_factor = (
                    ideal_scale_factor * video_info.resize_upscale_factor
                )
                target_width = int(video_info.orig_width * user_scale_factor)
                target_height = int(video_info.orig_height * user_scale_factor)

                # ensure image dimensions must be divisible by subsampling factor (fails otherwise)
                format_info = core.get_video_format(target_format)
                subsampling_w = 2**format_info.subsampling_w
                subsampling_h = 2**format_info.subsampling_h
                target_width = (target_width // subsampling_w) * subsampling_w
                target_height = (target_height // subsampling_h) * subsampling_h

                if video.width < target_width or video.height < target_height:
                    old_width = video.width
                    old_height = video.height

                    video = core.resize.Point(
                        video,
                        width=target_width,
                        height=target_height,
                    )

        convert_kwargs = {
            "format": target_format,
            "range_in": video_info.is_full_color_range,
            "range": video_info.is_full_color_range,
        }

        if target_format == vs.RGBS and orig_format.color_family == vs.YUV:
            convert_kwargs["matrix_in_s"] = "709"

        if video_info.resize_chromaloc is not None:
            convert_kwargs["chromaloc_s"] = video_info.resize_chromaloc

        # we can use point resizing if there'll be no chroma loss
        # otherwise use bicubic so it doesnt look super wrong
        use_point = not needs_upscale or (
            video.width == ideal_width and video.height == ideal_height
        )

        if use_point:
            video = core.resize.Point(video, **convert_kwargs)
        else:
            video = core.resize.Bicubic(video, **convert_kwargs)

    video = process_func(video)

    if needs_conversion:
        convert_back_kwargs = {
            "format": orig_format.id,
            "range_in": video_info.is_full_color_range,
            "range": video_info.is_full_color_range,
        }

        if target_format == vs.RGBS and orig_format.color_family == vs.YUV:
            convert_back_kwargs["matrix_s"] = "709"

        if use_point:
            video = core.resize.Point(video, **convert_back_kwargs)
        else:
            video = core.resize.Bicubic(video, **convert_back_kwargs)

        if old_width is not None and old_height is not None:
            video = core.resize.Point(
                video,
                width=old_width,
                height=old_height,
            )

    return video
