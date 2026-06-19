#!/usr/bin/env python3
"""Bake Otto mouth expressions into the GIF assets.

The robot uses six eye GIFs.  This script keeps those eyes, draws mouth frames
directly into the GIF bitmaps, and emits additional emotion-specific GIFs whose
eyes reuse the closest base animation.
"""

from __future__ import annotations

import math
from pathlib import Path

from PIL import Image, ImageDraw, ImageSequence


ROOT = Path(__file__).resolve().parents[1]
GIF_DIR = ROOT / "managed_components" / "txp666__otto-emoji-gif-component" / "gifs"
BASE_GIF_DIR = ROOT / "managed_components" / "txp666__otto-emoji-gif-component" / "base_gifs"
SCALE = 4

BASE_FOR_EMOTION = {
    "staticstate": "staticstate",
    "neutral": "staticstate",
    "idle": "staticstate",
    "happy": "happy",
    "laughing": "happy",
    "funny": "happy",
    "loving": "happy",
    "confident": "happy",
    "smirk": "happy",
    "winking": "happy",
    "cool": "happy",
    "relaxed": "happy",
    "delicious": "happy",
    "kissy": "happy",
    "silly": "happy",
    "sad": "sad",
    "crying": "sad",
    "sleepy": "staticstate",
    "scare": "scare",
    "surprised": "scare",
    "shocked": "scare",
    "buxue": "buxue",
    "thinking": "buxue",
    "confused": "buxue",
    "embarrassed": "buxue",
    "anger": "anger",
    "angry": "anger",
}

TALKING_EMOTIONS = BASE_FOR_EMOTION

STYLE_FOR_EMOTION = {
    "staticstate": "gentle",
    "neutral": "neutral",
    "idle": "gentle",
    "happy": "smile",
    "laughing": "laugh",
    "funny": "funny",
    "loving": "heart",
    "confident": "smirk",
    "smirk": "smirk",
    "winking": "wink",
    "cool": "cool",
    "relaxed": "relaxed",
    "delicious": "delicious",
    "kissy": "kiss",
    "silly": "silly",
    "sad": "sad",
    "crying": "cry",
    "sleepy": "sleepy",
    "scare": "surprised",
    "surprised": "surprised",
    "shocked": "shocked",
    "buxue": "thinking",
    "thinking": "confused",
    "confused": "thinking",
    "embarrassed": "embarrassed",
    "anger": "angry",
    "angry": "angry",
}


def ease(frame: int, count: int, phase: float = 0.0) -> float:
    return 0.5 + 0.5 * math.sin((frame / max(count, 1)) * math.tau + phase)


def s(value: float) -> int:
    return int(round(value * SCALE))


def draw_curve(draw: ImageDraw.ImageDraw, points, color, width: float) -> None:
    scaled = [(s(x), s(y)) for x, y in points]
    draw.line(scaled, fill=color, width=s(width), joint="curve")
    radius = width * 0.5
    for x, y in points:
        draw.ellipse((s(x - radius), s(y - radius), s(x + radius), s(y + radius)), fill=color)


def bezier_points(p0, p1, p2, steps: int = 28):
    points = []
    for i in range(steps + 1):
        t = i / steps
        mt = 1.0 - t
        x = mt * mt * p0[0] + 2 * mt * t * p1[0] + t * t * p2[0]
        y = mt * mt * p0[1] + 2 * mt * t * p1[1] + t * t * p2[1]
        points.append((x, y))
    return points


def draw_open(draw, cx, cy, width, height, accent, inner, tongue=None, teeth=False, tilt=0.0):
    box = (s(cx - width / 2), s(cy - height / 2), s(cx + width / 2), s(cy + height / 2))
    draw.ellipse(box, fill=inner, outline=accent, width=s(4.0))
    if teeth:
        teeth_box = (
            s(cx - width * 0.31),
            s(cy - height * 0.35),
            s(cx + width * 0.31),
            s(cy - height * 0.08),
        )
        draw.rounded_rectangle(teeth_box, radius=s(4), fill=(255, 242, 220), outline=None)
    elif tongue is not None:
        tongue_box = (
            s(cx - width * 0.25 + tilt),
            s(cy + height * 0.10),
            s(cx + width * 0.25 + tilt),
            s(cy + height * 0.45),
        )
        draw.ellipse(tongue_box, fill=tongue)


def draw_heart_mouth(draw, cx, cy, size, color):
    points = []
    for i in range(96):
        t = math.tau * i / 96.0
        x = 16.0 * math.sin(t) ** 3
        y = -(13.0 * math.cos(t) - 5.0 * math.cos(2 * t) -
              2.0 * math.cos(3 * t) - math.cos(4 * t))
        points.append((s(cx + x * size / 34.0), s(cy + y * size / 34.0)))
    draw.polygon(points, fill=color)


def draw_eye_tear(draw, x, y, color):
    draw.ellipse((s(x - 3.2), s(y - 3.0), s(x + 3.2), s(y + 5.8)), fill=color)
    draw.polygon([(s(x - 2.8), s(y - 0.5)), (s(x + 2.8), s(y - 0.5)), (s(x), s(y - 7.2))], fill=color)


def draw_drool(draw, cx, cy, color, openness=0.0):
    height = 9.0 + 3.0 * openness
    width = 5.5
    draw.ellipse((s(cx - width * 0.5), s(cy), s(cx + width * 0.5), s(cy + height)), fill=color)
    draw.polygon([(s(cx - width * 0.42), s(cy + 1.2)),
                  (s(cx + width * 0.42), s(cy + 1.2)),
                  (s(cx), s(cy - 4.5))], fill=color)


def draw_thinking_wave(draw, cx, cy, color, width=4.0):
    draw_curve(draw, bezier_points((cx - 34, cy + 4), (cx - 22, cy - 9),
                                   (cx - 10, cy + 1)), color, width)
    draw_curve(draw, bezier_points((cx - 10, cy + 1), (cx + 5, cy + 13),
                                   (cx + 19, cy - 1)), color, width)
    draw_curve(draw, bezier_points((cx + 19, cy - 1), (cx + 27, cy - 8),
                                   (cx + 34, cy + 1)), color, width)


def draw_smile_talk_open(draw, cx, cy, width, height, accent, inner, openness, teeth=False):
    left = cx - width * 0.5
    right = cx + width * 0.5
    top = bezier_points((left, cy - height * 0.22), (cx, cy + height * 0.05),
                        (right, cy - height * 0.22), 28)
    bottom = bezier_points((right, cy + height * 0.06), (cx, cy + height * 0.68 + 4 * openness),
                           (left, cy + height * 0.06), 28)
    right_side = bezier_points((right, cy - height * 0.22), (right + height * 0.24, cy - height * 0.03),
                               (right, cy + height * 0.06), 8)
    left_side = bezier_points((left, cy + height * 0.06), (left - height * 0.24, cy - height * 0.03),
                              (left, cy - height * 0.22), 8)
    polygon = [(s(x), s(y)) for x, y in top + right_side[1:] + bottom[1:] + left_side[1:]]
    draw.polygon(polygon, fill=inner)
    if teeth:
        teeth_box = (
            s(cx - width * 0.26),
            s(cy - height * 0.08),
            s(cx + width * 0.26),
            s(cy + height * 0.10),
        )
        draw.rounded_rectangle(teeth_box, radius=s(3), fill=(255, 242, 220), outline=None)
    draw_curve(draw, top, accent, 4.5)
    draw_curve(draw, right_side, accent, 4.5)
    draw_curve(draw, bottom, accent, 4.5)
    draw_curve(draw, left_side, accent, 4.5)


def draw_frown_talk_open(draw, cx, cy, width, height, accent, inner, openness):
    left = cx - width * 0.5
    right = cx + width * 0.5
    top = bezier_points((left, cy + height * 0.24), (cx, cy - height * 0.72),
                        (right, cy + height * 0.24), 30)
    bottom = bezier_points((right, cy + height * 0.45), (cx, cy - height * 0.16 + 3 * openness),
                           (left, cy + height * 0.45), 30)
    right_side = bezier_points((right, cy + height * 0.24), (right + height * 0.18, cy + height * 0.34),
                               (right, cy + height * 0.45), 8)
    left_side = bezier_points((left, cy + height * 0.45), (left - height * 0.18, cy + height * 0.34),
                              (left, cy + height * 0.24), 8)
    draw.polygon([(s(x), s(y)) for x, y in top + right_side[1:] + bottom[1:] + left_side[1:]], fill=inner)
    draw_curve(draw, top, accent, 5.2)
    draw_curve(draw, right_side, accent, 4.2)
    draw_curve(draw, bottom, accent, 4.2)
    draw_curve(draw, left_side, accent, 4.2)


def draw_slanted_talk_open(draw, cx, cy, width, height, accent, inner, openness, direction):
    left = cx - width * 0.5
    right = cx + width * 0.5
    lift = 9 * direction
    top = bezier_points((left, cy + lift * 0.45), (cx, cy - height * 0.55),
                        (right, cy - lift * 0.45), 28)
    bottom = bezier_points((right, cy + height * 0.24 - lift * 0.35),
                           (cx, cy + height * 0.52 + 3 * openness),
                           (left, cy + height * 0.24 + lift * 0.35), 28)
    right_side = bezier_points((right, cy - lift * 0.45),
                               (right + height * 0.16, cy + height * 0.02 - lift * 0.35),
                               (right, cy + height * 0.24 - lift * 0.35), 8)
    left_side = bezier_points((left, cy + height * 0.24 + lift * 0.35),
                              (left - height * 0.16, cy + height * 0.02 + lift * 0.35),
                              (left, cy + lift * 0.45), 8)
    draw.polygon([(s(x), s(y)) for x, y in top + right_side[1:] + bottom[1:] + left_side[1:]], fill=inner)
    draw_curve(draw, top, accent, 4.8)
    draw_curve(draw, right_side, accent, 4.2)
    draw_curve(draw, bottom, accent, 4.2)
    draw_curve(draw, left_side, accent, 4.2)


def draw_thinking_talk_open(draw, cx, cy, width, height, accent, inner, tongue, style, openness):
    draw_open(draw, cx + (5 if style != "thinking" else 0), cy + 1,
              width, height, accent, inner, None, tilt=4 if style == "confused" else 0)


def draw_moon_smile(draw, cx, cy, width, height, accent, fill, shadow=None, openness=0.0):
    left = cx - width * 0.5
    right = cx + width * 0.5
    top_edge = cy - height * 0.42
    bottom_edge = cy + height * 0.38 + openness * 4.0
    top_mid = cy - height * 0.06 + openness * 1.5
    bottom_mid = cy + height * 0.76 + openness * 5.0
    top = bezier_points((left, top_edge), (cx, top_mid), (right, top_edge), 32)
    bottom = bezier_points((right, bottom_edge), (cx, bottom_mid), (left, bottom_edge), 32)
    corner_bulge = height * 0.36
    right_corner = bezier_points((right, top_edge), (right + corner_bulge, cy + height * 0.10),
                                 (right, bottom_edge), 14)
    left_corner = bezier_points((left, bottom_edge), (left - corner_bulge, cy + height * 0.10),
                                (left, top_edge), 14)
    polygon = [(s(x), s(y)) for x, y in top + right_corner[1:] + bottom[1:] + left_corner[1:]]

    draw.polygon(polygon, fill=fill)

    if shadow is not None:
        shadow_top = bezier_points((left + width * 0.22, cy + height * 0.40 + openness * 3.0),
                                   (cx, cy + height * 0.60 + openness * 5.0),
                                   (right - width * 0.18, cy + height * 0.34 + openness * 3.0), 18)
        shadow_bottom = bezier_points((right - width * 0.16, bottom_edge - 1),
                                      (cx, bottom_mid + 2),
                                      (left + width * 0.28, bottom_edge), 18)
        draw.polygon([(s(x), s(y)) for x, y in shadow_top + shadow_bottom], fill=shadow)

    outline_width = 4.4
    draw_curve(draw, top, accent, outline_width)
    draw_curve(draw, right_corner, accent, outline_width)
    draw_curve(draw, bottom, accent, outline_width)
    draw_curve(draw, left_corner, accent, outline_width)


def draw_talking_mouth(draw, cx, cy, frame, count, accent, inner, tongue):
    cycle = frame % 8
    openness = [0.10, 0.35, 0.78, 1.0, 0.72, 0.38, 0.18, 0.08][cycle]
    width = 58 + 10 * openness
    height = 16 + 20 * openness
    if openness < 0.28:
        draw_moon_smile(draw, cx, cy, width, 17, accent, (255, 238, 214, 255), inner, openness)
        return
    draw_open(draw, cx, cy + 3, width, height, accent, inner, tongue, teeth=openness > 0.65)


def draw_emotion_talking_mouth(draw, cx, cy, frame, count, style, accent, inner, tongue,
                               pale, cool, blue, yellow, red, pink):
    cycle = frame % 8
    openness = [0.12, 0.36, 0.74, 1.0, 0.70, 0.40, 0.20, 0.10][cycle]
    wobble = math.sin((frame / max(count, 1)) * math.tau)

    def curve(points, color, width):
        draw_curve(draw, points, color, width)

    if style in ("neutral", "gentle", "smile", "relaxed"):
        width = 66 + 8 * openness
        if openness < 0.32:
            if style == "neutral":
                curve(bezier_points((88, cy), (120, cy + 2), (152, cy)), accent, 4)
            else:
                draw_moon_smile(draw, cx, cy, width, 15 + 5 * openness,
                                accent, (255, 238, 214, 255), inner,
                                openness * 0.45)
            return
        if style == "neutral":
            draw_open(draw, cx, cy + 2, width, 12 + 14 * openness, accent, inner, tongue,
                      teeth=openness > 0.82)
        else:
            draw_smile_talk_open(draw, cx, cy + 1, width + 2, 14 + 17 * openness,
                                 accent, inner, openness, teeth=style == "smile" and openness > 0.82)
        return

    if style in ("smirk", "wink"):
        mouth_cx = cx + 3
        direction = 1 if style == "wink" else -1
        if openness < 0.34:
            curve(bezier_points((86, cy + 4), (116, cy + 8), (154, cy - 8)), accent, 5)
            if style == "smirk":
                draw.ellipse((s(151), s(cy - 12), s(157), s(cy - 7)), fill=accent)
            return
        draw_slanted_talk_open(draw, mouth_cx, cy + 2, 58 + 8 * openness, 14 + 16 * openness,
                               accent, inner, openness, direction)
        return

    if style == "cool":
        if openness < 0.34:
            curve(bezier_points((88, cy + 1), (120, cy + 1), (152, cy - 1)), cool, 4.5)
            return
        box = (s(cx - 31 - 4 * openness), s(cy - 3 - 3 * openness),
               s(cx + 31 + 4 * openness), s(cy + 9 + 9 * openness))
        draw.rounded_rectangle(box, radius=s(5), fill=inner, outline=cool, width=s(4))
        draw.line((s(cx - 24), s(cy + 1), s(cx + 24), s(cy + 1)), fill=(255, 242, 220, 255), width=s(2))
        return

    if style in ("laugh", "funny"):
        tilt = 5 if style == "funny" else 0
        draw_open(draw, cx - (3 if style == "funny" else 0), cy + 3,
                  62 + 10 * openness, 22 + 18 * openness, accent, inner, None,
                  teeth=style == "laugh" or openness > 0.75, tilt=tilt)
        return

    if style == "heart":
        draw_heart_mouth(draw, cx, cy + 3, 46 + 3 * openness, pink)
        return

    if style == "delicious":
        draw_open(draw, cx - 2, cy + 3, 48 + 10 * openness, 18 + 18 * openness,
                  accent, inner, None)
        return

    if style == "kiss":
        rx = 10 + 5 * openness
        ry = 5 + 4 * openness
        draw.ellipse((s(cx - rx), s(cy - 8 - ry), s(cx + rx), s(cy - 8 + ry)), fill=pink)
        draw.ellipse((s(cx - rx), s(cy + 4 - ry), s(cx + rx), s(cy + 4 + ry)), fill=pink)
        if openness > 0.48:
            draw.ellipse((s(cx - 5), s(cy - 6), s(cx + 5), s(cy + 8)), fill=inner)
        return

    if style == "silly":
        draw_open(draw, cx + 4, cy + 3, 48 + 9 * openness, 18 + 17 * openness,
                  accent, inner, None, tilt=-4)
        draw_drool(draw, cx + 14, cy + 15, blue, openness)
        return

    if style in ("sad", "cry", "sleepy"):
        if openness < 0.34:
            if style == "sleepy":
                curve(bezier_points((88, cy + 1), (120, cy + 7), (150, cy + 3)), blue, 4)
                draw.ellipse((s(159), s(cy - 14), s(168), s(cy - 8)), outline=blue, width=s(2))
            else:
                curve(bezier_points((82, cy + 14), (120, cy - 15 - 2 * openness),
                                    (158, cy + 14)), blue, 5.5)
            if style == "cry":
                draw_eye_tear(draw, 160, 132 + 2 * openness, blue)
            return
        draw_frown_talk_open(draw, cx, cy + 4, 50 + 8 * openness, 13 + 15 * openness,
                             blue, inner, openness)
        if style == "cry":
            draw_eye_tear(draw, 160, 132 + 3 * openness, blue)
        if style == "sleepy":
            draw.ellipse((s(158), s(cy - 13), s(168), s(cy - 7)), outline=blue, width=s(2))
        return

    if style == "surprised":
        draw.ellipse((s(cx - 15 - 8 * openness), s(cy - 17 - 8 * openness),
                      s(cx + 15 + 8 * openness), s(cy + 17 + 8 * openness)),
                     fill=inner, outline=(255, 193, 90, 255), width=s(4))
        draw.arc((s(cx - 10), s(cy - 14), s(cx + 10), s(cy + 14)), 205, 335,
                 fill=pale, width=s(2))
        return

    if style == "shocked":
        draw_open(draw, cx, cy + 2, 50 + 9 * openness, 24 + 18 * openness,
                  (255, 193, 90, 255), inner, None, teeth=True)
        return

    if style in ("thinking", "confused", "embarrassed"):
        if openness < 0.34:
            if style == "thinking":
                draw_thinking_wave(draw, cx, cy, yellow, 4.0)
            elif style == "confused":
                curve(bezier_points((83, cy + 4), (101, cy - 10), (120, cy + 3)), yellow, 4.5)
                curve(bezier_points((120, cy + 3), (139, cy + 16), (158, cy - 8)), yellow, 4.5)
            else:
                curve(bezier_points((94, cy + 2), (120, cy + 8), (146, cy + 1)), yellow, 3.8)
            return
        draw_thinking_talk_open(draw, cx, cy, 44 + 8 * openness, 12 + 14 * openness,
                                yellow, inner, tongue, style, openness)
        return

    if style == "angry":
        width = 62 + 8 * openness
        height = 18 + 8 * openness
        box = (s(cx - width / 2), s(cy - height / 2), s(cx + width / 2), s(cy + height / 2))
        draw.rounded_rectangle(box, radius=s(4), fill=pale, outline=red, width=s(4))
        for x in (cx - width * 0.25, cx, cx + width * 0.25):
            draw.line((s(x), s(cy - height * 0.35), s(x), s(cy + height * 0.38)),
                      fill=inner, width=s(2))
        draw.line((s(cx - width * 0.45), s(cy + (openness - 0.4) * 3),
                   s(cx + width * 0.45), s(cy - (openness - 0.4) * 3)),
                  fill=inner, width=s(2))
        return

    draw_talking_mouth(draw, cx, cy, frame, count, accent, inner, tongue)


def draw_mouth(canvas: Image.Image, style: str, frame: int, count: int) -> Image.Image:
    layer = Image.new("RGBA", (canvas.width * SCALE, canvas.height * SCALE), (0, 0, 0, 0))
    draw = ImageDraw.Draw(layer)

    t = ease(frame, count)
    wobble = math.sin((frame / max(count, 1)) * math.tau)
    cx = 120.0
    cy = 184.0
    accent = (255, 112, 80, 255)
    inner = (33, 16, 20, 255)
    tongue = (255, 111, 145, 255)
    pale = (255, 242, 220, 255)
    cool = (183, 230, 255, 255)
    blue = (112, 200, 255, 255)
    yellow = (232, 209, 107, 255)
    red = (255, 74, 74, 255)
    pink = (255, 123, 176, 255)

    if style.startswith("talking:"):
        draw_emotion_talking_mouth(draw, cx, cy, frame, count, style.split(":", 1)[1],
                                   accent, inner, tongue, pale, cool, blue, yellow, red, pink)
    elif style == "talking":
        draw_talking_mouth(draw, cx, cy, frame, count, accent, inner, tongue)
    elif style == "neutral":
        draw_curve(draw, bezier_points((86, cy), (120, cy + 2), (154, cy)), accent, 4)
    elif style == "gentle":
        draw_moon_smile(draw, cx, cy, 72, 16, accent, (255, 238, 214, 255), inner, 0.0)
    elif style == "smile":
        draw_moon_smile(draw, cx, cy, 78, 18, accent, (255, 238, 214, 255), inner, 0.0)
    elif style == "relaxed":
        draw_moon_smile(draw, cx, cy, 70, 15, accent, (255, 238, 214, 255), None, 0.0)
    elif style == "laugh":
        draw_open(draw, cx, cy + 3, 68 + 6 * t, 32 + 4 * t, accent, inner, None, teeth=True)
    elif style == "funny":
        draw_open(draw, cx - 3, cy + 4, 58 + 5 * t, 28 + 5 * t, accent, inner, None, tilt=4)
    elif style == "heart":
        draw_heart_mouth(draw, cx, cy + 3, 46 + 3 * t, pink)
    elif style == "smirk":
        draw_curve(draw, bezier_points((85, cy + 6), (116, cy + 5), (154, cy - 11)), accent, 5)
        draw.ellipse((s(151), s(cy - 14), s(157), s(cy - 8)), fill=accent)
    elif style == "wink":
        draw_curve(draw, bezier_points((86, cy + 2), (116, cy + 15), (154, cy - 8)), accent, 5)
    elif style == "cool":
        draw_curve(draw, bezier_points((88, cy + 1), (120, cy + 1), (152, cy - 1)), cool, 4.5)
    elif style == "delicious":
        draw_open(draw, cx - 2, cy + 3, 48 + 4 * t, 24 + 4 * t, accent, inner, None)
    elif style == "kiss":
        draw.ellipse((s(cx - 12), s(cy - 10), s(cx + 12), s(cy - 1)), fill=pink)
        draw.ellipse((s(cx - 12), s(cy + 1), s(cx + 12), s(cy + 11)), fill=pink)
    elif style == "silly":
        draw_open(draw, cx + 4, cy + 3, 48 + 5 * t, 24 + 4 * t, accent, inner, None, tilt=-3)
        draw_drool(draw, cx + 14, cy + 15, blue, t)
    elif style == "sad":
        draw_curve(draw, bezier_points((82, cy + 13), (120, cy - 15 - 2 * t), (158, cy + 13)), blue, 5.5)
    elif style == "cry":
        draw_curve(draw, bezier_points((82, cy + 16), (120, cy - 18), (158, cy + 16)), blue, 6)
        draw_eye_tear(draw, 160, 132 + 2 * t, blue)
    elif style == "sleepy":
        draw_curve(draw, bezier_points((88, cy + 1), (120, cy + 7), (150, cy + 3)), blue, 4)
        draw.ellipse((s(159), s(cy - 14), s(168), s(cy - 8)), outline=blue, width=s(2))
    elif style == "surprised":
        draw.ellipse((s(cx - 19 - t * 2), s(cy - 24), s(cx + 19 + t * 2), s(cy + 24)), fill=inner, outline=(255, 193, 90, 255), width=s(4))
        draw.arc((s(cx - 12), s(cy - 18), s(cx + 12), s(cy + 18)), 205, 335, fill=pale, width=s(2))
    elif style == "shocked":
        draw_open(draw, cx, cy + 2, 54 + 4 * t, 34 + 4 * t, (255, 193, 90, 255), inner, None, teeth=True)
    elif style == "thinking":
        draw_thinking_wave(draw, cx, cy, yellow, 4.0)
    elif style == "confused":
        draw_curve(draw, bezier_points((83, cy + 4), (101, cy - 10), (120, cy + 3)), yellow, 4.5)
        draw_curve(draw, bezier_points((120, cy + 3), (139, cy + 16), (158, cy - 8)), yellow, 4.5)
    elif style == "embarrassed":
        draw_curve(draw, bezier_points((94, cy + 2), (120, cy + 8), (146, cy + 1)), yellow, 3.8)
    elif style == "angry":
        box = (s(84), s(cy - 11), s(156), s(cy + 12))
        draw.rounded_rectangle(box, radius=s(4), fill=pale, outline=red, width=s(4))
        for x in (102, 120, 138):
            draw.line((s(x), s(cy - 8), s(x), s(cy + 10)), fill=inner, width=s(2))
        draw.line((s(86), s(cy), s(154), s(cy)), fill=inner, width=s(2))
    else:
        draw_curve(draw, bezier_points((86, cy), (120, cy + 2), (154, cy)), accent, 4)

    layer = layer.resize(canvas.size, Image.Resampling.LANCZOS)
    return Image.alpha_composite(canvas, layer)


def load_frames(path: Path):
    image = Image.open(path)
    frames = [frame.copy().convert("RGBA") for frame in ImageSequence.Iterator(image)]
    durations = [frame.info.get("duration", image.info.get("duration", 80)) for frame in ImageSequence.Iterator(image)]
    return frames, durations, image.info.get("loop", 0)


def save_gif(path: Path, frames, durations, loop: int) -> None:
    rgb_frames = [frame.convert("RGB") for frame in frames]
    rgb_frames[0].save(
        path,
        save_all=True,
        append_images=rgb_frames[1:],
        duration=durations,
        loop=loop,
        optimize=True,
        disposal=2,
    )


def main() -> None:
    for old_talking_gif in GIF_DIR.glob("*_talking.gif"):
        old_talking_gif.unlink()

    base_cache = {}
    for base in sorted(set(BASE_FOR_EMOTION.values())):
        base_cache[base] = load_frames(BASE_GIF_DIR / f"{base}.gif")

    for emotion, base in sorted(BASE_FOR_EMOTION.items()):
        frames, durations, loop = base_cache[base]
        style = STYLE_FOR_EMOTION[emotion]
        out_frames = [
            draw_mouth(frame.copy(), style, index, len(frames))
            for index, frame in enumerate(frames)
        ]
        out_path = GIF_DIR / f"{emotion}.gif"
        save_gif(out_path, out_frames, durations, loop)
        print(f"wrote {out_path.relative_to(ROOT)} ({len(out_frames)} frames, style={style})")

    for emotion, base in sorted(TALKING_EMOTIONS.items()):
        frames, durations, loop = base_cache[base]
        style = STYLE_FOR_EMOTION[emotion]
        out_frames = [
            draw_mouth(frame.copy(), f"talking:{style}", index, len(frames))
            for index, frame in enumerate(frames)
        ]
        out_path = GIF_DIR / f"{emotion}_talking.gif"
        save_gif(out_path, out_frames, durations, loop)
        print(f"wrote {out_path.relative_to(ROOT)} ({len(out_frames)} frames, style=talking:{style})")


if __name__ == "__main__":
    main()
