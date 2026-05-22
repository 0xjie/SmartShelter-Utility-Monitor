from __future__ import annotations

from pathlib import Path
from PIL import Image, ImageDraw, ImageFont


OUT = Path(r"D:\AAA\paper-output\figures\figure3-1-system-architecture-clean.png")
W, H = 3200, 1800
BG = "white"
BLACK = "black"
LINE = 4
SCALE = 2


def load_font(size: int, bold: bool = False):
    candidates = [
        r"C:\Windows\Fonts\msyhbd.ttc" if bold else r"C:\Windows\Fonts\msyh.ttc",
        r"C:\Windows\Fonts\simhei.ttf" if bold else r"C:\Windows\Fonts\simsun.ttc",
        r"C:\Windows\Fonts\arialbd.ttf" if bold else r"C:\Windows\Fonts\arial.ttf",
    ]
    for path in candidates:
        try:
            return ImageFont.truetype(path, size)
        except OSError:
            continue
    return ImageFont.load_default()


FONT_LANE = load_font(30, bold=True)
FONT_BOX = load_font(28)
FONT_BOX_SMALL = load_font(25)


def sx(v: int) -> int:
    return v * SCALE


def draw_centered_multiline(draw: ImageDraw.ImageDraw, box: tuple[int, int, int, int], lines: list[str], font, line_gap: int = 6):
    x1, y1, x2, y2 = box
    widths = []
    heights = []
    for line in lines:
        bbox = draw.textbbox((0, 0), line, font=font)
        widths.append(bbox[2] - bbox[0])
        heights.append(bbox[3] - bbox[1])
    total_h = sum(heights) + line_gap * (len(lines) - 1)
    cy = (y1 + y2 - total_h) // 2
    for line, w, h in zip(lines, widths, heights):
        cx = (x1 + x2 - w) // 2
        draw.text((cx, cy), line, fill=BLACK, font=font)
        cy += h + line_gap


def draw_lane(draw: ImageDraw.ImageDraw, x: int, y: int, w: int, h: int, title: str):
    x1, y1, x2, y2 = sx(x), sx(y), sx(x + w), sx(y + h)
    header_h = sx(30)
    draw.rectangle([x1, y1, x2, y2], outline=BLACK, width=LINE)
    draw.line([x1, y1 + header_h, x2, y1 + header_h], fill=BLACK, width=LINE)
    draw_centered_multiline(draw, (x1, y1, x2, y1 + header_h), [title], FONT_LANE)


def draw_box(draw: ImageDraw.ImageDraw, x: int, y: int, w: int, h: int, lines: list[str], small: bool = False):
    x1, y1, x2, y2 = sx(x), sx(y), sx(x + w), sx(y + h)
    draw.rectangle([x1, y1, x2, y2], outline=BLACK, width=LINE)
    draw_centered_multiline(draw, (x1 + 10, y1 + 8, x2 - 10, y2 - 8), lines, FONT_BOX_SMALL if small else FONT_BOX)
    return (x1, y1, x2, y2)


def draw_cylinder(draw: ImageDraw.ImageDraw, x: int, y: int, w: int, h: int, lines: list[str]):
    x1, y1, x2, y2 = sx(x), sx(y), sx(x + w), sx(y + h)
    r = sx(10)
    top_h = sx(18)
    draw.rectangle([x1, y1 + top_h // 2, x2, y2 - top_h // 2], outline=BLACK, width=LINE)
    draw.ellipse([x1, y1, x2, y1 + top_h], outline=BLACK, width=LINE)
    draw.arc([x1, y2 - top_h, x2, y2], start=0, end=180, fill=BLACK, width=LINE)
    draw_centered_multiline(draw, (x1 + r, y1 + top_h + 6, x2 - r, y2 - top_h), lines, FONT_BOX_SMALL)
    return (x1, y1, x2, y2)


def arrow(draw: ImageDraw.ImageDraw, pts: list[tuple[int, int]], head: bool = True, dashed: bool = False):
    if dashed:
        for (x1, y1), (x2, y2) in zip(pts[:-1], pts[1:]):
            seg_len = abs(x2 - x1) + abs(y2 - y1)
            if seg_len == 0:
                continue
            steps = max(1, seg_len // 22)
            for i in range(0, steps, 2):
                t1 = i / steps
                t2 = min((i + 1) / steps, 1)
                xa = int(x1 + (x2 - x1) * t1)
                ya = int(y1 + (y2 - y1) * t1)
                xb = int(x1 + (x2 - x1) * t2)
                yb = int(y1 + (y2 - y1) * t2)
                draw.line([xa, ya, xb, yb], fill=BLACK, width=LINE)
    else:
        draw.line(pts, fill=BLACK, width=LINE)

    if head and len(pts) >= 2:
        x1, y1 = pts[-2]
        x2, y2 = pts[-1]
        ah = 16
        if x2 > x1:
            tri = [(x2, y2), (x2 - ah, y2 - ah // 2), (x2 - ah, y2 + ah // 2)]
        elif x2 < x1:
            tri = [(x2, y2), (x2 + ah, y2 - ah // 2), (x2 + ah, y2 + ah // 2)]
        elif y2 > y1:
            tri = [(x2, y2), (x2 - ah // 2, y2 - ah), (x2 + ah // 2, y2 - ah)]
        else:
            tri = [(x2, y2), (x2 - ah // 2, y2 + ah), (x2 + ah // 2, y2 + ah)]
        draw.polygon(tri, fill=BLACK)


def mid_top(box): return ((box[0] + box[2]) // 2, box[1])
def mid_bottom(box): return ((box[0] + box[2]) // 2, box[3])
def mid_left(box): return (box[0], (box[1] + box[3]) // 2)
def mid_right(box): return (box[2], (box[1] + box[3]) // 2)


def main():
    img = Image.new("RGB", (W, H), BG)
    draw = ImageDraw.Draw(img)

    draw_lane(draw, 70, 50, 1460, 105, "展示与交互层")
    draw_lane(draw, 70, 190, 1460, 145, "管理与存储层")
    draw_lane(draw, 70, 370, 1460, 105, "通信与接入层")
    draw_lane(draw, 70, 510, 1460, 110, "嵌入式控制层")
    draw_lane(draw, 70, 655, 1460, 165, "感知与执行层")

    web = draw_box(draw, 185, 92, 300, 52, ["Web 看板", "实时展示 / 历史趋势 / 一键求助"], small=True)
    user = draw_box(draw, 1145, 92, 295, 52, ["管理人员与安置点用户"], small=True)
    qt = draw_box(draw, 145, 246, 470, 64, ["Qt 桌面管理端", "实时监测 / 阈值设置 / 远程控制 / 报警管理 / 历史查询 / 用户认证"], small=True)
    db = draw_cylinder(draw, 985, 234, 420, 80, ["SQLite 本地数据库", "data / alarm_info / remote_exec_logs / device_info / users"])
    esp32 = draw_box(draw, 200, 404, 420, 56, ["ESP32 通信网关", "UART 缓存 / WiFi 接入 / MQTT 发布订阅 / 命令转发 / 防回环"], small=True)
    mqtt = draw_box(draw, 1000, 404, 365, 56, ["MQTT 消息服务", "test001up / WebQT1"], small=True)
    stm32 = draw_box(draw, 445, 552, 710, 56, ["STM32F103 主控制器", "数据采集 / 阈值判断 / 三级预警 / 组合联动 / JSON 打包"], small=True)
    sensor_env = draw_box(draw, 185, 728, 310, 58, ["环境传感器", "DHT11 / MQ-135 / GP2Y10"], small=True)
    sensor_res = draw_box(draw, 645, 728, 270, 58, ["资源传感器", "ACS712 / 水流传感器"], small=True)
    actuator = draw_box(draw, 1065, 728, 370, 58, ["执行与本地显示", "风扇 / 舵机 / 蜂鸣器 / 三色灯 / OLED"], small=True)

    arrow(draw, [mid_right(web), (sx(1145), mid_right(web)[1]), mid_left(user)], head=False)
    arrow(draw, [mid_top(qt), (mid_top(qt)[0], sx(176)), (mid_bottom(web)[0], sx(176)), mid_bottom(web)])
    arrow(draw, [mid_right(qt), (sx(800), mid_right(qt)[1]), (sx(985), mid_left(db)[1]), mid_left(db)])
    arrow(draw, [mid_bottom(qt), (mid_bottom(qt)[0], sx(432)), mid_left(mqtt)], head=True)
    arrow(draw, [mid_top(mqtt), (mid_top(mqtt)[0], sx(165)), (mid_bottom(web)[0], sx(165)), mid_bottom(web)])
    arrow(draw, [mid_bottom(web), (mid_bottom(web)[0], sx(392)), (mid_top(mqtt)[0], sx(392)), mid_top(mqtt)], dashed=True)
    arrow(draw, [mid_right(esp32), mid_left(mqtt)])
    arrow(draw, [mid_bottom(esp32), (mid_bottom(esp32)[0], sx(580)), (sx(445), sx(580)), mid_left(stm32)])
    arrow(draw, [mid_top(sensor_env), (mid_top(sensor_env)[0], sx(642)), (sx(560), sx(642)), (sx(560), mid_bottom(stm32)[1]), mid_bottom(stm32)])
    arrow(draw, [mid_top(sensor_res), (mid_top(sensor_res)[0], sx(642)), (sx(780), sx(642)), (sx(780), mid_bottom(stm32)[1]), mid_bottom(stm32)])
    arrow(draw, [mid_bottom(stm32), (sx(1250), mid_bottom(stm32)[1]), (sx(1250), sx(642)), (sx(1250), mid_top(actuator)[1]), mid_top(actuator)])

    img.save(OUT)
    print(OUT)


if __name__ == "__main__":
    main()
