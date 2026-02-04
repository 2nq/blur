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

    if needs_conversion:
        convert_kwargs = {
            "format": target_format,
            "range_in": video_info.is_full_color_range,
            "range": video_info.is_full_color_range,
        }

        # upscale to avoid chroma loss
        scale_factor = get_scale_factor_for_format(target_format)
        if scale_factor > 1:
            target_width = video_info.orig_width * scale_factor
            target_height = video_info.orig_height * scale_factor

            if video.width < target_width or video.height < target_height:
                old_width = video.width
                old_height = video.height
                convert_kwargs["width"] = target_width
                convert_kwargs["height"] = target_height

        if target_format == vs.RGBS and orig_format.color_family == vs.YUV:
            convert_kwargs["matrix_in_s"] = "709"

        if video_info.resize_chromaloc is not None:
            convert_kwargs["chromaloc_s"] = video_info.resize_chromaloc

        video = core.resize.Point(video, **convert_kwargs)

    video = process_func(video)

    if needs_conversion:
        convert_back_kwargs = {
            "format": orig_format.id,
            "range_in": video_info.is_full_color_range,
            "range": video_info.is_full_color_range,
        }

        if old_width is not None and old_height is not None:
            convert_back_kwargs["width"] = old_width
            convert_back_kwargs["height"] = old_height

        if target_format == vs.RGBS and orig_format.color_family == vs.YUV:
            convert_back_kwargs["matrix_s"] = "709"

        video = core.resize.Point(video, **convert_back_kwargs)

    return video
