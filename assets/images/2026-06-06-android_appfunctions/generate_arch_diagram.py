#!/usr/bin/env python3
"""Generate AppFunction architecture diagram as JPEG image."""

from PIL import Image, ImageDraw, ImageFont

# Canvas settings
WIDTH = 1100
HEIGHT = 420
BG_COLOR = (250, 251, 252)
BOX_COLOR = (200, 205, 210)
HEADER_BG = (55, 120, 180)
HEADER_TEXT_COLOR = (255, 255, 255)
TEXT_COLOR = (40, 40, 50)
ARROW_COLOR = (120, 130, 140)
LABEL_COLOR = (90, 95, 100)
SECTION_BG = (255, 255, 255)

# Try to use a monospace font
try:
    font_header = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 18)
    font_body = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf", 14)
    font_label = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 13)
except OSError:
    font_header = ImageFont.load_default()
    font_body = ImageFont.load_default()
    font_label = ImageFont.load_default()

img = Image.new("RGB", (WIDTH, HEIGHT), BG_COLOR)
draw = ImageDraw.Draw(img)

# Box dimensions - more compact
BOX_W = 280
BOX_H = 330
GAP = 70
START_X = 50
START_Y = 40

# Three columns
columns = [
    {
        "title": "Agent App",
        "x": START_X,
        "upper": ["AppSearchManager"],
        "lower": ["AppFunctionManager"],
    },
    {
        "title": "System Server",
        "x": START_X + BOX_W + GAP,
        "upper": ["AppSearchManagerService"],
        "lower": ["AppFunctionManagerService"],
    },
    {
        "title": "Provider App",
        "x": START_X + 2 * (BOX_W + GAP),
        "upper": ["app_functions.xml", "app_function_schema.xml"],
        "lower": ["MyAppFunctionService"],
    },
]

HEADER_H = 40
SECTION_H = (BOX_H - HEADER_H) // 2


def text_width(text, font):
    """Get text width compatible with old Pillow versions."""
    return draw.textsize(text, font=font)[0]


def draw_rounded_rect(xy, radius, fill=None, outline=None, width=1):
    """Draw a rounded rectangle (approximate with arcs)."""
    x1, y1, x2, y2 = xy
    if fill:
        draw.rectangle([x1 + radius, y1, x2 - radius, y2], fill=fill)
        draw.rectangle([x1, y1 + radius, x2, y2 - radius], fill=fill)
        draw.pieslice([x1, y1, x1 + 2 * radius, y1 + 2 * radius], 180, 270, fill=fill)
        draw.pieslice([x2 - 2 * radius, y1, x2, y1 + 2 * radius], 270, 360, fill=fill)
        draw.pieslice([x1, y2 - 2 * radius, x1 + 2 * radius, y2], 90, 180, fill=fill)
        draw.pieslice([x2 - 2 * radius, y2 - 2 * radius, x2, y2], 0, 90, fill=fill)
    if outline:
        draw.arc([x1, y1, x1 + 2 * radius, y1 + 2 * radius], 180, 270, fill=outline, width=width)
        draw.arc([x2 - 2 * radius, y1, x2, y1 + 2 * radius], 270, 360, fill=outline, width=width)
        draw.arc([x1, y2 - 2 * radius, x1 + 2 * radius, y2], 90, 180, fill=outline, width=width)
        draw.arc([x2 - 2 * radius, y2 - 2 * radius, x2, y2], 0, 90, fill=outline, width=width)
        draw.line([(x1 + radius, y1), (x2 - radius, y1)], fill=outline, width=width)
        draw.line([(x1 + radius, y2), (x2 - radius, y2)], fill=outline, width=width)
        draw.line([(x1, y1 + radius), (x1, y2 - radius)], fill=outline, width=width)
        draw.line([(x2, y1 + radius), (x2, y2 - radius)], fill=outline, width=width)


def draw_box(col):
    x, y = col["x"], START_Y
    radius = 8

    # Step 1: Draw full rounded rect as white background
    draw_rounded_rect((x, y, x + BOX_W, y + BOX_H), radius, fill=SECTION_BG, outline=None, width=0)

    # Step 2: Fill header area with blue (top portion only)
    # Top rounded corners
    draw.pieslice([x, y, x + 2 * radius, y + 2 * radius], 180, 270, fill=HEADER_BG)
    draw.pieslice([x + BOX_W - 2 * radius, y, x + BOX_W, y + 2 * radius], 270, 360, fill=HEADER_BG)
    # Top edge between corners
    draw.rectangle([x + radius, y, x + BOX_W - radius, y + radius], fill=HEADER_BG)
    # Main header body
    draw.rectangle([x, y + radius, x + BOX_W, y + HEADER_H], fill=HEADER_BG)

    # Step 3: Outer border (rounded)
    draw_rounded_rect((x, y, x + BOX_W, y + BOX_H), radius, fill=None, outline=BOX_COLOR, width=1)

    # Header text
    title_w = text_width(col["title"], font_header)
    draw.text(
        (x + (BOX_W - title_w) // 2, y + 10),
        col["title"],
        fill=HEADER_TEXT_COLOR,
        font=font_header,
    )

    # Subtle divider between sections
    mid_y = y + HEADER_H + SECTION_H
    draw.line([(x + 15, mid_y), (x + BOX_W - 15, mid_y)], fill=(220, 225, 230), width=1)

    # Upper section items (centered vertically)
    upper_y_start = y + HEADER_H
    total_text_h = len(col["upper"]) * 22
    item_y = upper_y_start + (SECTION_H - total_text_h) // 2
    for item in col["upper"]:
        item_w = text_width(item, font_body)
        draw.text(
            (x + (BOX_W - item_w) // 2, item_y),
            item,
            fill=TEXT_COLOR,
            font=font_body,
        )
        item_y += 22

    # Lower section items (centered vertically)
    lower_y_start = mid_y
    total_text_h = len(col["lower"]) * 22
    item_y = lower_y_start + (SECTION_H - total_text_h) // 2
    for item in col["lower"]:
        item_w = text_width(item, font_body)
        draw.text(
            (x + (BOX_W - item_w) // 2, item_y),
            item,
            fill=TEXT_COLOR,
            font=font_body,
        )
        item_y += 22


def draw_arrow(x1, y_center, x2, label):
    """Draw a right-pointing arrow from x1 to x2 at y_center with label."""
    # Arrow line
    draw.line([(x1, y_center), (x2, y_center)], fill=ARROW_COLOR, width=2)
    # Arrowhead
    arrow_size = 10
    draw.polygon(
        [
            (x2, y_center),
            (x2 - arrow_size, y_center - arrow_size // 2),
            (x2 - arrow_size, y_center + arrow_size // 2),
        ],
        fill=ARROW_COLOR,
    )
    # Label above the arrow
    label_w = text_width(label, font_label)
    mid_x = (x1 + x2) // 2
    draw.text(
        (mid_x - label_w // 2, y_center - 22),
        label,
        fill=LABEL_COLOR,
        font=font_label,
    )


# Draw boxes
for col in columns:
    draw_box(col)

# Draw arrows
# Upper section arrows (query / parse) - y center of upper section
upper_y = START_Y + HEADER_H + SECTION_H // 2

# Agent -> System Server (query)
draw_arrow(
    columns[0]["x"] + BOX_W,
    upper_y,
    columns[1]["x"],
    "query",
)
# System Server -> Provider (parse)
draw_arrow(
    columns[1]["x"] + BOX_W,
    upper_y,
    columns[2]["x"],
    "parse",
)

# Lower section arrows (call / bind) - y center of lower section
lower_y = START_Y + HEADER_H + SECTION_H + SECTION_H // 2

# Agent -> System Server (call)
draw_arrow(
    columns[0]["x"] + BOX_W,
    lower_y,
    columns[1]["x"],
    "call",
)
# System Server -> Provider (bind)
draw_arrow(
    columns[1]["x"] + BOX_W,
    lower_y,
    columns[2]["x"],
    "bind",
)

# Save
output_path = "/home/fusion/workspace/android-16.0.0_r4/demo/appfunction_architecture.jpg"
img.save(output_path, "JPEG", quality=95)
print(f"Saved to: {output_path}")
