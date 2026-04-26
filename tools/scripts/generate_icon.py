"""Generate Asteria multi-resolution .ico file.

Creates a compelling astrology-themed icon with a stylized star/compass
motif in deep navy and gold, suitable for taskbar and About screen display.
Outputs 16x16, 24x24, 32x32, 48x48, 64x64, 128x128, and 256x256 sizes.
"""
import math
from PIL import Image, ImageDraw, ImageFont

def draw_icon(size):
    """Draw the Asteria icon at the given size."""
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    cx, cy = size / 2, size / 2
    r = size / 2 - 1

    # Background circle — deep navy
    draw.ellipse([cx - r, cy - r, cx + r, cy + r], fill=(18, 22, 48, 255))

    # Outer ring — gold border
    ring_w = max(1, size // 24)
    draw.ellipse([cx - r, cy - r, cx + r, cy + r], outline=(212, 175, 55, 255), width=ring_w)

    # Inner ring
    r2 = r * 0.82
    draw.ellipse([cx - r2, cy - r2, cx + r2, cy + r2], outline=(212, 175, 55, 180), width=max(1, ring_w // 2))

    # Draw a 6-pointed star (Star of David / hexagram) — classic astrology motif
    star_r = r * 0.52
    gold = (212, 175, 55, 255)

    # Two overlapping triangles
    for offset_angle in [0, 180]:
        points = []
        for i in range(3):
            angle = math.radians(offset_angle - 90 + i * 120)
            px = cx + star_r * math.cos(angle)
            py = cy + star_r * math.sin(angle)
            points.append((px, py))
        draw.polygon(points, outline=gold, fill=None, width=max(1, size // 32))

    # Small central dot
    dot_r = max(1, size // 16)
    draw.ellipse([cx - dot_r, cy - dot_r, cx + dot_r, cy + dot_r], fill=gold)

    # 12 small tick marks around the outer ring (zodiac divisions)
    if size >= 32:
        tick_inner = r * 0.87
        tick_outer = r * 0.95
        tick_w = max(1, size // 48)
        for i in range(12):
            angle = math.radians(i * 30 - 90)
            x1 = cx + tick_inner * math.cos(angle)
            y1 = cy + tick_inner * math.sin(angle)
            x2 = cx + tick_outer * math.cos(angle)
            y2 = cy + tick_outer * math.sin(angle)
            draw.line([(x1, y1), (x2, y2)], fill=(212, 175, 55, 200), width=tick_w)

    # Cardinal cross lines (subtle)
    if size >= 48:
        cross_inner = r * 0.15
        cross_outer = r * 0.38
        cross_w = max(1, size // 48)
        cross_color = (212, 175, 55, 120)
        for angle_deg in [0, 90, 180, 270]:
            angle = math.radians(angle_deg - 90)
            x1 = cx + cross_inner * math.cos(angle)
            y1 = cy + cross_inner * math.sin(angle)
            x2 = cx + cross_outer * math.cos(angle)
            y2 = cy + cross_outer * math.sin(angle)
            draw.line([(x1, y1), (x2, y2)], fill=cross_color, width=cross_w)

    # Letter "A" in center for larger sizes
    if size >= 64:
        try:
            font_size = int(size * 0.22)
            font = ImageFont.truetype("arial.ttf", font_size)
        except OSError:
            font = ImageFont.load_default()
        bbox = draw.textbbox((0, 0), "A", font=font)
        tw, th = bbox[2] - bbox[0], bbox[3] - bbox[1]
        tx = cx - tw / 2 - bbox[0]
        ty = cy - th / 2 - bbox[1] + size * 0.01
        # Draw over the center dot area
        dot_clear = dot_r + 2
        draw.ellipse([cx - dot_clear, cy - dot_clear, cx + dot_clear, cy + dot_clear], fill=(18, 22, 48, 255))
        draw.text((tx, ty), "A", fill=gold, font=font)

    return img


def main():
    sizes = [16, 24, 32, 48, 64, 128, 256]
    images = [draw_icon(s) for s in sizes]

    # Save as .ico (multi-resolution)
    ico_path = r"c:\Projects\other\Asteria\assets\asteria.ico"
    images[-1].save(ico_path, format="ICO", sizes=[(s, s) for s in sizes],
                    append_images=images[:-1])
    print(f"Icon saved to {ico_path}")

    # Also save a 256px PNG for the About screen
    png_path = r"c:\Projects\other\Asteria\assets\asteria_icon.png"
    images[-1].save(png_path, format="PNG")
    print(f"PNG saved to {png_path}")

    # Copy to packaging assets directory
    import shutil, os
    pkg_dir = r"c:\Projects\other\Asteria\tools\packaging\assets"
    os.makedirs(pkg_dir, exist_ok=True)
    shutil.copy2(ico_path, os.path.join(pkg_dir, "asteria.ico"))
    print(f"Copied to {pkg_dir}")


if __name__ == "__main__":
    main()
