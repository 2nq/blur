import vapoursynth as vs
from vapoursynth import core

import sys
import json
from pathlib import Path
from fractions import Fraction

# add blur.py folder to path so it can reference scripts
sys.path.insert(1, str(Path(__file__).parent))

import blur.blending
import blur.deduplicate
import blur.interpolate
import blur.weighting
import blur.utils as u

if vars().get("macos_bundled") == "true":
    u.load_plugins(".dylib")
elif vars().get("linux_bundled") == "true":
    u.load_plugins(".so")

video_path = Path(vars().get("video_path", ""))

settings = json.loads(vars().get("settings", "{}"))

fps_num = vars().get("fps_num", -1)
fps_den = vars().get("fps_den", -1)
color_range = vars().get("color_range", "")

has_audio = vars().get("has_audio", "true") == "true"

# validate some settings
svp_interpolation_algorithm = u.coalesce(
    u.safe_int(settings["svp_interpolation_algorithm"]),
    blur.interpolate.DEFAULT_ALGORITHM,
)

interpolation_blocksize = u.coalesce(
    u.safe_int(settings["interpolation_blocksize"]),
    blur.interpolate.DEFAULT_BLOCKSIZE,
)

interpolation_mask_area = u.coalesce(
    u.safe_int(settings["interpolation_mask_area"]),
    blur.interpolate.DEFAULT_MASKING,
)

resize_chromaloc = settings["resize_chromaloc"]
if resize_chromaloc == "default":
    resize_chromaloc = None

resize_upscale_factor = (
    settings["resize_upscale_factor"] if settings["resize_upscale"] else 0
)

rife_gpu_index = settings["rife_gpu_index"]
if rife_gpu_index == -1:  # haven't benchmarked yet..?
    rife_gpu_index = 0

if vars().get("enable_lsmash") == "true":
    video = core.lsmas.LWLibavSource(
        source=video_path,
        cache=0,
        prefer_hw=3 if settings["gpu_decoding"] else 0,
        fpsnum=fps_num if fps_num != -1 else None,
        fpsden=fps_den if fps_den != -1 else None,
    )
else:
    video = core.bs.VideoSource(
        source=video_path,
        cachemode=0,
        fpsnum=fps_num if fps_num != -1 else None,
        fpsden=fps_den if fps_den != -1 else None,
    )

video_info = u.VideoInfo(
    is_full_color_range=color_range == "pc",
    orig_width=video.width,
    orig_height=video.height,
    resize_chromaloc=resize_chromaloc,
    resize_upscale_factor=resize_upscale_factor,
)

# upscaling (to 4K)
# note: it's good to do this before anything else because it avoids issues with chroma subsampling conversion required for svp etc. higher res = issues with conversion (glow etc) are less noticeable
if settings["upscale"] and video.height < 2160:
    HEIGHT_4K = 2160

    scale_factor = HEIGHT_4K / video.height
    video = core.resize.Point(
        video,
        width=int(round(video.width * scale_factor)),
        height=HEIGHT_4K,
    )

# input timescale
if settings["timescale"]:
    input_timescale = float(settings["input_timescale"])
    if settings["input_timescale"] != 1:
        video = u.assume_scaled_fps(video, 1 / input_timescale)

if settings["deduplicate"] and settings["deduplicate_range"] != 0:
    deduplicate_range: int | None = int(settings["deduplicate_range"])
    if deduplicate_range == -1:  # -1 = infinite
        deduplicate_range = None

    try:
        deduplicate_threshold = float(settings["deduplicate_threshold"])
    except (ValueError, TypeError, KeyError):
        deduplicate_threshold = 0.001

    match settings["deduplicate_method"]:
        case "old":
            video = blur.deduplicate.fill_drops_old(
                video,
                threshold=deduplicate_threshold,
                debug=settings["debug"],
            )

        case "rife":
            video = blur.deduplicate.fill_drops_rife(
                video,
                video_info=video_info,
                model_path=settings["rife_model"],
                gpu_index=rife_gpu_index,
                threshold=deduplicate_threshold,
                max_frames=deduplicate_range,
                debug=settings["debug"],
            )

        case "mvtools":
            video = blur.deduplicate.fill_drops_mvtools(
                video,
                threshold=deduplicate_threshold,
                max_frames=deduplicate_range,
                blocksize=interpolation_blocksize,
                masking=interpolation_mask_area,
                debug=settings["debug"],
            )

        case _:
            video = blur.deduplicate.fill_drops_svp(
                video,
                video_info=video_info,
                threshold=deduplicate_threshold,
                max_frames=deduplicate_range,
                debug=settings["debug"],
                svp_preset=settings["svp_interpolation_preset"],
                svp_algorithm=svp_interpolation_algorithm,
                svp_blocksize=interpolation_blocksize,
                svp_masking=interpolation_mask_area,
                svp_gpu=settings["gpu_interpolation"],
            )

# interpolation
if settings["interpolate"]:

    def parse_fps_setting(setting_key):
        fps_value = settings[setting_key].strip()

        if fps_value.endswith("x"):
            # ends with x, is a multiplier (e.g. 5x)
            multiplier_str = fps_value[:-1].strip()
            if not multiplier_str:
                raise u.BlurException(
                    f"Invalid FPS multiplier {setting_key}: '{fps_value}'. Should be something like 5x."
                )

            try:
                multiplier = float(multiplier_str)
            except ValueError:
                raise u.BlurException(
                    f"Invalid FPS multiplier {setting_key}: '{fps_value}'. Should be something like 5x. Do you have non-number characters before the final x?"
                )

            return video.fps * multiplier

        else:
            # doesn't end with x, is an fps (e.g. 600)
            try:
                return int(fps_value)
            except ValueError:
                raise u.BlurException(
                    f"Invalid FPS {setting_key}: '{fps_value}' - failed to parse it as an integer. Is it an integer?"
                )

    interpolated_fps = parse_fps_setting("interpolated_fps")

    if settings["interpolation_method"] != "rife" and settings["pre_interpolate"]:
        pre_interpolated_fps = parse_fps_setting("pre_interpolated_fps")

        if (
            video.fps < pre_interpolated_fps
        ):  # if can be while if rife limits the max interpolation fps, but i don't think it does
            old_fps = video.fps

            print(f"pre-interpolating to {pre_interpolated_fps}")

            video = blur.interpolate.interpolate_rife(
                video,
                video_info=video_info,
                new_fps=pre_interpolated_fps,
                model_path=settings["rife_model"],
                gpu_index=rife_gpu_index,
            )

            fps_added = video.fps - old_fps
            print(
                f"added {fps_added} (interp: {pre_interpolated_fps}. video.fps: {video.fps}/{pre_interpolated_fps})"
            )

    if video.fps < interpolated_fps:
        print(
            f"interpolating to {interpolated_fps} with {settings['interpolation_method']}"
        )
        old_fps = video.fps

        match settings["interpolation_method"]:
            case "rife":
                video = blur.interpolate.interpolate_rife(
                    video,
                    video_info=video_info,
                    new_fps=interpolated_fps,
                    model_path=settings["rife_model"],
                    gpu_index=rife_gpu_index,
                )

            case "mvtools":
                video = blur.interpolate.interpolate_mvtools(
                    video,
                    interpolated_fps,
                    blocksize=int(settings["interpolation_blocksize"]),
                    masking=int(settings["interpolation_mask_area"]),
                )

            case _:  # svp
                if not settings["manual_svp"]:
                    video = blur.interpolate.interpolate_svp(
                        video,
                        video_info=video_info,
                        new_fps=interpolated_fps,
                        preset=settings["svp_interpolation_preset"],
                        algorithm=svp_interpolation_algorithm,
                        blocksize=interpolation_blocksize,
                        overlap=0,
                        masking=interpolation_mask_area,
                        gpu=settings["gpu_interpolation"],
                    )
                else:
                    # insert interpolated fps
                    smooth_json = json.loads(settings["smooth_string"])
                    if "rate" not in smooth_json:
                        smooth_json["rate"] = {"num": interpolated_fps, "abs": True}
                    smooth_str = json.dumps(smooth_json)

                    video = blur.interpolate.svp(
                        video,
                        video_info=video_info,
                        super_string=settings["super_string"],
                        vectors_string=settings["vectors_string"],
                        smooth_str=smooth_str,
                    )

        fps_added = video.fps - old_fps
        print(
            f"added {fps_added} (interp: {interpolated_fps}. video.fps: {video.fps}/{interpolated_fps})"
        )

# output timescale
if settings["timescale"]:
    output_timescale = float(settings["output_timescale"])
    if output_timescale != 1:
        video = u.assume_scaled_fps(video, output_timescale)

# blurring
if settings["blur"]:
    if settings["blur_amount"] > 0:
        frame_gap = int(video.fps / settings["blur_output_fps"])
        blur_frames = int(frame_gap * settings["blur_amount"])

        if blur_frames > 0:
            # number of weights must be odd
            if blur_frames % 2 == 0:
                blur_frames += 1

            weights = blur.weighting.parse(
                blur_frames,
                weighting_type=settings["blur_weighting"],
                gaussian_std_dev=settings["blur_weighting_gaussian_std_dev"],
                gaussian_mean=settings["blur_weighting_gaussian_mean"],
                gaussian_bound=json.loads(settings["blur_weighting_gaussian_bound"]),
            )

            gamma = float(settings["blur_gamma"])
            if gamma == 1.0:
                video = blur.blending.average(video, weights)
            else:
                video = blur.blending.average_bright(
                    video,
                    video_info,
                    gamma,
                    weights,
                )

    # set exact fps
    video = blur.interpolate.change_fps(video, settings["blur_output_fps"])

# filters
if settings["filters"]:
    if (
        settings["brightness"] != 1
        or settings["contrast"] != 1
        or settings["saturation"] != 1
    ):
        video = u.with_format(
            video,
            video_info,
            vs.YUV444PS,
            lambda video: core.adjust.Tweak(
                video,
                bright=settings["brightness"] - 1,
                cont=settings["contrast"],
                sat=settings["saturation"],
            ),
        )

start = float(vars().get("start", 0.0))
end = float(vars().get("end", 1.0))

# clamp
start = max(0.0, min(1.0, start))
end = max(0.0, min(1.0, end))

# trimming
v_start = 0
v_end = video.num_frames

if start != 0:
    v_start = int(video.num_frames * start)

if end != 1:
    v_end = int(video.num_frames * end)

    # @note:extra-frame add an extra frame. in the preview, visually it seems like the end point is included in the cut.
    v_end = min(v_end + 1, video.num_frames)

# safety, need to have at least one frame
if v_end <= v_start:
    v_end = v_start + 1

video = video[v_start:v_end]
video.set_output(0)

# audio
if has_audio:
    # TODO: multiple audio stream support?
    audio = core.bs.AudioSource(source=video_path, cachemode=0)

    a_start = None
    a_end = None

    # trimming
    fps = Fraction(video.fps.numerator, video.fps.denominator)

    if v_start is not None:
        # TODO MR: make sure this is exactly synced
        time_start = Fraction(v_start) / fps
        a_start = int(time_start * audio.sample_rate)

    if v_end is not None:
        # TODO MR: make sure this is exactly synced
        time_end = Fraction(v_end) / fps
        a_end = int(time_end * audio.sample_rate)

    audio = audio[a_start:a_end]
    audio.set_output(1)
