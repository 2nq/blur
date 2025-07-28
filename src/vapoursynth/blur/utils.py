import vapoursynth as vs
from vapoursynth import core

from pathlib import Path
from fractions import Fraction


class BlurException(Exception):
    pass


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


def with_format(
    video: vs.VideoNode, is_full_color_range: bool, target_format, process_func
):
    orig_format = video.format
    needs_conversion = orig_format.id != target_format

    matrix = video.get_frame(0).props.get("_Matrix", None)
    transfer = video.get_frame(0).props.get("_Transfer", None)
    primaries = video.get_frame(0).props.get("_Primaries", None)
    # color_range = video.get_frame(0).props.get("_ColorRange", None)

    if needs_conversion:
        video = core.resize.Point(
            video,
            format=target_format,
            range=is_full_color_range,
        )

    video = process_func(video)

    if needs_conversion:
        video = core.resize.Point(
            video,
            format=orig_format.id,
            range=is_full_color_range,
            matrix=matrix,
            transfer=transfer,
            primaries=primaries,
        )
    return video
