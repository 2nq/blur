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


def scale_luminance(video: vs.VideoNode, upscale: bool, y_scale_w: int, y_scale_h: int):
    if y_scale_w == 1 and y_scale_h == 1:
        return video

    y = core.std.ShufflePlanes(video, planes=0, colorfamily=vs.GRAY)
    u = core.std.ShufflePlanes(video, planes=1, colorfamily=vs.GRAY)
    v = core.std.ShufflePlanes(video, planes=2, colorfamily=vs.GRAY)

    if upscale:
        y = core.resize.Point(y, width=y.width * y_scale_w, height=y.height * y_scale_h)
    else:  # downscale
        y = core.resize.Point(y, width=y.width / y_scale_w, height=y.height / y_scale_h)

    video = core.std.ShufflePlanes(
        clips=[y, u, v], planes=[0, 0, 0], colorfamily=vs.YUV
    )

    return video


def with_scaled_luminance(
    video: vs.VideoNode,
    target_format,
    process_func,
):
    in_format_info = core.get_video_format(video.format)
    target_format_info = core.get_video_format(target_format)

    scale_w = 1
    scale_h = 1

    if video.format.color_family == vs.ColorFamily.YUV:
        subsampling_diff_w = (
            target_format_info.subsampling_w - in_format_info.subsampling_w
        )
        subsampling_diff_h = (
            target_format_info.subsampling_h - in_format_info.subsampling_h
        )

        scale_w = (
            # for each increase in subsampling level, chroma resolution halves
            # subsampling=0 -> full res, subsampling=1 -> half res, subsampling=2 -> quarter res
            2**subsampling_diff_w
        )
        scale_h = 2**subsampling_diff_h

        video = scale_luminance(video, True, scale_w, scale_h)

    video = process_func(video)

    if scale_w != 1 or scale_h != 1:
        video = scale_luminance(video, False, scale_w, scale_h)

    return video


def with_format(
    video: vs.VideoNode,
    video_info: VideoInfo,
    target_format,
    process_func,
):
    orig_format = video.format
    needs_conversion = orig_format.id != target_format

    if needs_conversion:
        convert_kwargs = {
            "format": target_format,
            "range_in": video_info.is_full_color_range,
            "range": video_info.is_full_color_range,
        }

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

        if target_format == vs.RGBS and orig_format.color_family == vs.YUV:
            convert_back_kwargs["matrix_s"] = "709"

        video = core.resize.Point(video, **convert_back_kwargs)

    return video
