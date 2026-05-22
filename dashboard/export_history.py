"""
从 SQLite 数据库导出每日水电消耗聚合数据为 JS 文件。
用法：python export_history.py
输出：js/history_data.js
"""
import sqlite3
import json
import os

DB_PATH = r"D:\System_UI_Data\system_ui.sqlite"
OUT_PATH = os.path.join(os.path.dirname(__file__), "js", "history_data.js")

def main():
    db = sqlite3.connect(DB_PATH)
    cur = db.cursor()

    # 按天聚合：SUM 近似估算每日总消耗
    # current_a 单位 mA，每约5s一条，日消耗 mAh = SUM(current_a * 5/3600)
    # flow_l_min 单位 L/min，日消耗 L = SUM(flow_l_min * 5/60)
    cur.execute("""
        SELECT date(ts) as day,
               COUNT(*) as samples,
               ROUND(AVG(current_a), 1) as avg_current_ma,
               ROUND(AVG(flow_l_min), 2) as avg_flow_l_min,
               ROUND(SUM(current_a * 5.0 / 3600), 1) as daily_power_mah,
               ROUND(SUM(flow_l_min * 5.0 / 60), 2) as daily_water_l
        FROM data
        GROUP BY date(ts)
        ORDER BY day ASC
    """)

    rows = cur.fetchall()
    db.close()

    days = []
    for r in rows:
        days.append({
            "date": r[0],
            "samples": r[1],
            "avgCurrentMA": r[2],
            "avgFlowLMin": r[3],
            "powerMAh": r[4],
            "waterL": r[5],
        })

    # 最近 10 天（含今天）用于趋势图
    recent = days[-10:] if len(days) >= 10 else days
    recent7 = days[-7:] if len(days) >= 7 else days

    js_content = (
        "// 由 export_history.py 自动生成，数据来源 D:\\System_UI_Data\\system_ui.sqlite\n"
        "// 生成时间: 最近一次脚本运行\n"
        "window.HISTORY_DATA = {\n"
        f'  "allDays": {json.dumps(days, ensure_ascii=False, indent=2)},\n'
        f'  "recent10": {json.dumps(recent, ensure_ascii=False, indent=2)},\n'
        f'  "recent7": {json.dumps(recent7, ensure_ascii=False, indent=2)}\n'
        "};\n"
    )

    os.makedirs(os.path.dirname(OUT_PATH), exist_ok=True)
    with open(OUT_PATH, "w", encoding="utf-8") as f:
        f.write(js_content)

    print(f"OK → {OUT_PATH}  ({len(days)} 天, 最近 {len(recent7)} 天用于趋势图)")

if __name__ == "__main__":
    main()
