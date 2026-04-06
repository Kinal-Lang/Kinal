from __future__ import annotations

import argparse
from pathlib import Path

from PIL import Image


DEFAULT_SOURCE = Path("infra/assets/Kinal.png")
DEFAULT_OUTPUT = Path("infra/assets/Kinal.ico")
DEFAULT_SIZES = (16, 20, 24, 32, 40, 48, 64, 96, 128, 256)


def square_rgba(image: Image.Image) -> Image.Image:
    rgba = image.convert("RGBA")
    width, height = rgba.size
    side = max(width, height)
    if width == height:
        return rgba
    canvas = Image.new("RGBA", (side, side), (0, 0, 0, 0))
    offset = ((side - width) // 2, (side - height) // 2)
    canvas.paste(rgba, offset)
    return canvas


def generate_icon(source: Path, output: Path, sizes: tuple[int, ...]) -> Path:
    image = square_rgba(Image.open(source))
    output.parent.mkdir(parents=True, exist_ok=True)
    image.save(output, format="ICO", sizes=[(size, size) for size in sizes])
    return output


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Generate a multi-size Windows icon from infra/assets/Kinal.png")
    parser.add_argument("--source", type=Path, default=DEFAULT_SOURCE, help="input PNG path")
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT, help="output ICO path")
    parser.add_argument(
        "--sizes",
        default=",".join(str(size) for size in DEFAULT_SIZES),
        help="comma-separated square icon sizes",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    sizes = tuple(sorted({int(part) for part in str(args.sizes).split(",") if part.strip()}))
    if not sizes:
        raise SystemExit("at least one icon size is required")
    output = generate_icon(args.source, args.output, sizes)
    print(f"[OK] generated icon: {output}")
    print(f"[OK] sizes: {', '.join(str(size) for size in sizes)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())