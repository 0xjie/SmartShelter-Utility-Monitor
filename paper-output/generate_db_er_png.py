from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


OUT = Path(r"C:\Users\王杰\Desktop\论文\pictures\图3-4-数据库ER图.png")
OUT_ALT = Path(r"C:\Users\王杰\Desktop\论文\pictures\图3-4-数据库ER图-修正版.png")


def load_font(size: int, bold: bool = False):
    candidates = [
        r"C:\Windows\Fonts\msyhbd.ttc" if bold else r"C:\Windows\Fonts\msyh.ttc",
        r"C:\Windows\Fonts\simhei.ttf",
        r"C:\Windows\Fonts\simsun.ttc",
    ]
    for path in candidates:
        try:
            return ImageFont.truetype(path, size=size)
        except OSError:
            continue
    return ImageFont.load_default()


FONT_TITLE = load_font(28, bold=True)
FONT_HEAD = load_font(22, bold=True)
FONT_BODY = load_font(18, bold=False)
FONT_NOTE = load_font(16, bold=False)


def box(draw: ImageDraw.ImageDraw, xy, lines, width=3):
    x1, y1, x2, y2 = xy
    draw.rectangle(xy, outline="black", width=width, fill="white")
    text = "\n".join(lines)
    draw.multiline_text(
        (x1 + 14, y1 + 12),
        text,
        fill="black",
        font=FONT_BODY,
        spacing=6,
    )


def note(draw: ImageDraw.ImageDraw, xy, text):
    draw.rounded_rectangle(xy, radius=12, outline="black", width=2, fill="white")
    draw.multiline_text(
        (xy[0] + 12, xy[1] + 12),
        text,
        fill="black",
        font=FONT_NOTE,
        spacing=5,
    )


def arrow(draw: ImageDraw.ImageDraw, start, end, label=None, dashed=False):
    if dashed:
        x1, y1 = start
        x2, y2 = end
        segments = 18
        for i in range(segments):
            t1 = i / segments
            t2 = min((i + 0.55) / segments, 1)
            sx = x1 + (x2 - x1) * t1
            sy = y1 + (y2 - y1) * t1
            ex = x1 + (x2 - x1) * t2
            ey = y1 + (y2 - y1) * t2
            draw.line((sx, sy, ex, ey), fill="black", width=3)
    else:
        draw.line((start, end), fill="black", width=3)

    x1, y1 = start
    x2, y2 = end
    vx = x2 - x1
    vy = y2 - y1
    length = max((vx * vx + vy * vy) ** 0.5, 1)
    ux = vx / length
    uy = vy / length
    px = -uy
    py = ux
    tip = (x2, y2)
    left = (x2 - ux * 16 + px * 8, y2 - uy * 16 + py * 8)
    right = (x2 - ux * 16 - px * 8, y2 - uy * 16 - py * 8)
    draw.polygon([tip, left, right], fill="black")

    if label:
        mx = (x1 + x2) / 2
        my = (y1 + y2) / 2
        pad = 6
        bbox = draw.multiline_textbbox((0, 0), label, font=FONT_NOTE, spacing=4)
        tw = bbox[2] - bbox[0]
        th = bbox[3] - bbox[1]
        rect = (mx - tw / 2 - pad, my - th / 2 - pad, mx + tw / 2 + pad, my + th / 2 + pad)
        draw.rectangle(rect, fill="white")
        draw.multiline_text((mx - tw / 2, my - th / 2), label, fill="black", font=FONT_NOTE, spacing=4)


img = Image.new("RGB", (1700, 980), "white")
draw = ImageDraw.Draw(img)

draw.text((36, 24), "图3-4 数据库ER图（按真实 SQLite 表结构）", fill="black", font=FONT_TITLE)

box(draw, (60, 100, 330, 320), [
    "users",
    "----------------",
    "id (PK)",
    "username",
    "password",
    "role",
    "created_at",
])

box(draw, (480, 90, 790, 360), [
    "data",
    "----------------",
    "id (PK)",
    "ts",
    "temperature",
    "humidity",
    "pm25",
    "air_index",
    "current_a",
    "flow_l_min",
])

box(draw, (980, 90, 1320, 360), [
    "alarm_info",
    "----------------",
    "id (PK)",
    "alarm_time",
    "sensor_name",
    "alarm_content",
    "end_time",
    "level",
    "status",
    "alarm_code",
])

box(draw, (180, 430, 510, 670), [
    "device_info",
    "----------------",
    "id (PK)",
    "device_id (UNIQUE)",
    "device_name",
    "device_type",
    "location",
])

box(draw, (650, 430, 1010, 670), [
    "remote_exec_logs",
    "----------------",
    "id (PK)",
    "execute_time",
    "device_id",
    "command_text",
    "result_text",
])

box(draw, (1090, 430, 1500, 670), [
    "device_abnormal_records",
    "----------------",
    "id (PK)",
    "sensor_name",
    "offline_time",
    "offline_duration_sec",
    "UNIQUE(sensor_name, offline_time)",
])

note(draw, (60, 740, 560, 830), "说明：系统所有业务表位于同一 SQLite 数据库中。\n图中连线表示业务关联，不表示源码中显式定义了外键约束。")
note(draw, (980, 740, 1520, 830), "注意：device_abnormal_records 按 sensor_name 记录离线事件，\n并不通过 device_id 直接外键关联到 device_info。")

arrow(draw, (790, 225), (980, 225), label="业务生成报警记录", dashed=True)
arrow(draw, (510, 550), (650, 550), label="device_id 业务关联", dashed=True)

img.save(OUT)
img.save(OUT_ALT)
