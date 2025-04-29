import vapoursynth as vs
from functools import partial

core = vs.core


def create_center_color_mask(clip, radius, target_color, tolerance=10):
    """
    Creates a mask identifying pixels of a specified color within a radius from the center.

    Args:
        clip: Input clip
        radius: Radius from center in pixels
        target_color: RGB color to detect [R,G,B] values from 0-255
        tolerance: Color matching tolerance (default: 10)

    Returns:
        Mask clip (GRAY format) where white pixels indicate matched areas
    """
    width = clip.width
    height = clip.height
    center_x = width // 2
    center_y = height // 2

    # Convert to RGB if not already
    if clip.format.color_family != vs.RGB:
        # Assuming BT.709 color matrix - adjust if your content uses different matrix
        rgb_clip = core.resize.Bicubic(clip, format=vs.RGB24, matrix_in_s="709")
    else:
        rgb_clip = clip

    # Create RGB reference clips with constant values for target color
    r_ref = core.std.BlankClip(clip=rgb_clip, format=vs.GRAY8, color=[target_color[0]])
    g_ref = core.std.BlankClip(clip=rgb_clip, format=vs.GRAY8, color=[target_color[1]])
    b_ref = core.std.BlankClip(clip=rgb_clip, format=vs.GRAY8, color=[target_color[2]])

    # Extract RGB planes from input
    r = core.std.ShufflePlanes(rgb_clip, 0, vs.GRAY)
    g = core.std.ShufflePlanes(rgb_clip, 1, vs.GRAY)
    b = core.std.ShufflePlanes(rgb_clip, 2, vs.GRAY)

    # Create color mask using absolute difference from reference
    color_mask = core.std.Expr(
        clips=[r, g, b, r_ref, g_ref, b_ref],
        expr=f"x a - abs y b - abs + z c - abs + {tolerance} <= 255 0 ?",
    )

    # Debug: Return the circle mask to verify it's working
    # return circle_mask

    # Debug: Return the color mask to verify it's detecting colors
    # return color_mask

    return color_mask


def ColorReplaceCentered(clip, clip2, radius, target_color, tolerance=10):
    """
    Creates a clip where pixels of the specified color within the center radius
    are replaced with the original video.

    Args:
        clip: Input clip to process
        radius: Radius from center in pixels
        target_color: RGB color to detect [R,G,B] values from 0-255
        tolerance: Color matching tolerance (default: 10)

    Returns:
        Processed clip
    """
    # Create the mask for the specified color in the center region
    mask = create_center_color_mask(clip, radius, target_color, tolerance)

    # # Use MaskedMerge to apply the original video where the mask is white
    # result = core.std.MaskedMerge(clip, clip2, mask)

    return mask
