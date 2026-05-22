from __future__ import annotations

from pathlib import Path

from docx import Document


DOCX_PATH = Path(r"D:\AAA\备份论文_插图前备份.docx")
FALLBACK_PATH = Path(r"D:\AAA\备份论文_插图前备份_已加入引图说明.docx")


def copy_paragraph_format(src, dst):
    dst.style = src.style
    dst.alignment = src.alignment

    src_pf = src.paragraph_format
    dst_pf = dst.paragraph_format
    dst_pf.left_indent = src_pf.left_indent
    dst_pf.right_indent = src_pf.right_indent
    dst_pf.first_line_indent = src_pf.first_line_indent
    dst_pf.space_before = src_pf.space_before
    dst_pf.space_after = src_pf.space_after
    dst_pf.line_spacing = src_pf.line_spacing
    dst_pf.line_spacing_rule = src_pf.line_spacing_rule
    dst_pf.keep_together = src_pf.keep_together
    dst_pf.keep_with_next = src_pf.keep_with_next
    dst_pf.page_break_before = src_pf.page_break_before
    dst_pf.widow_control = src_pf.widow_control


def find_paragraph_index_exact(doc: Document, text: str):
    for idx, para in enumerate(doc.paragraphs):
        if para.text.strip() == text:
            return idx
    raise ValueError(f"未找到段落：{text}")


def nearby_body_paragraph(doc: Document, anchor_idx: int):
    paragraphs = doc.paragraphs
    for back in range(anchor_idx - 1, -1, -1):
        text = paragraphs[back].text.strip()
        style = paragraphs[back].style.name if paragraphs[back].style else ""
        if text and not style.startswith("Heading"):
            return paragraphs[back]
    for forward in range(anchor_idx + 1, len(paragraphs)):
        text = paragraphs[forward].text.strip()
        style = paragraphs[forward].style.name if paragraphs[forward].style else ""
        if text and not style.startswith("Heading"):
            return paragraphs[forward]
    for back in range(anchor_idx - 1, -1, -1):
        if paragraphs[back].text.strip():
            return paragraphs[back]
    for forward in range(anchor_idx + 1, len(paragraphs)):
        if paragraphs[forward].text.strip():
            return paragraphs[forward]
    raise ValueError("未找到可复用的正文段落样式")


def insert_before_heading(doc: Document, heading_text: str, texts: list[str]):
    anchor_idx = find_paragraph_index_exact(doc, heading_text)
    anchor = doc.paragraphs[anchor_idx]
    style_src = nearby_body_paragraph(doc, anchor_idx)
    for text in texts:
        new_para = anchor.insert_paragraph_before(text)
        copy_paragraph_format(style_src, new_para)


def replace_paragraph_containing(doc: Document, needle: str, new_text: str):
    for para in doc.paragraphs:
        if needle in para.text:
            para.text = new_text
            return
    raise ValueError(f"未找到包含文本：{needle}")


def main():
    doc = Document(str(DOCX_PATH))

    replace_paragraph_containing(
        doc,
        "本系统的功能性需求围绕",
        "为了更直观地说明本系统从现场感知、状态判定、消息传输到多端展示与求助交互之间的完整业务闭环关系，本章在功能性需求分析开始处给出系统业务流程图，以便为后续需求分解提供统一参照，如图2-1所示。",
    )

    insert_before_heading(
        doc,
        "3.3.2 各子系统功能划分",
        [
            "为了从整体上展示系统各层之间的组织关系以及现场控制端、通信网关、桌面管理端、Web 展示端和本地数据库之间的协同方式，本节在五层架构说明之后给出系统总体架构图，用于概括本系统的分层结构与核心组成，如图3-1所示。"
        ],
    )
    insert_before_heading(
        doc,
        "3.3 系统软件总体设计",
        [
            "为了避免硬件架构部分停留在分模块说明层面，在硬件总体结构介绍结束后，有必要进一步给出系统硬件连接图，用于说明 STM32 主控、ESP32 网关以及各类传感器、执行器之间的实际接口关系和关键引脚分配，如图3-3所示。",
            "此外，为了反映本课题硬件平台的真实搭建情况与模块集成效果，在硬件连接关系说明之后补充系统硬件实物图，使读者能够从实物层面对系统组成形成直观认识，如图3-4所示。",
        ],
    )
    insert_before_heading(
        doc,
        "3.4 嵌入式控制与通信总体设计",
        [
            "为了更清晰地呈现系统在运行过程中上行监测数据、下行控制命令以及求助消息在各节点之间的流转路径，本节在协同工作流程说明之后补充系统数据流图，以帮助读者理解各模块之间的先后关系和信息方向，如图3-2所示。"
        ],
    )
    insert_before_heading(
        doc,
        "3.7.2 数据存储逻辑设计",
        [
            "为了更直观地展示本地数据库中各核心数据表的组成以及它们在业务层面的关联关系，本节在数据库结构设计说明之后给出数据库 ER 图，从而帮助读者理解历史数据、报警事件、用户认证、设备信息与操作日志之间的组织方式，如图3-5所示。"
        ],
    )

    insert_before_heading(
        doc,
        "4.2.2 数据采集与预处理实现",
        [
            "为了使主程序结构不只停留在文字层面的任务说明，本节在总体机制分析之后给出 STM32 主程序流程图，用于展示系统上电初始化、周期采样、状态判定、联动执行、数据打包与显示刷新之间的运行顺序，如图4-1所示。"
        ],
    )
    insert_before_heading(
        doc,
        "4.1.3 电流与水流采集模块设计",
        [
            "在多类传感器中，GP2Y10 粉尘传感器的采样精度与 LED 驱动时序密切相关，因此有必要单独给出其关键脉冲驱动与采样时序关系，以说明系统如何在固定周期内完成发光、延时采样与熄灭控制，如图4-2所示。"
        ],
    )
    insert_before_heading(
        doc,
        "4.2.4 JSON 上报与命令解析实现",
        [
            "为了更清楚地表达各监测指标从原始采样值到正常、预警和告警三级状态的判定过程，以及全局风险等级与联动输出之间的关系，本节在阈值与联动机制说明之后给出三级状态判定示意图，如图4-3所示。",
            "同时，考虑到本系统状态切换并非简单阈值越界即立刻升级，而是引入了连续确认和滞回恢复机制，因此有必要进一步补充滞回防抖示意图，用于解释系统如何抑制临界值附近的频繁误触发，如图4-4所示。",
        ],
    )
    insert_before_heading(
        doc,
        "4.3.4 阈值同步与消息过滤实现",
        [
            "为了更直观地说明 STM32、ESP32、MQTT 服务以及 Qt 与 Web 端之间的消息交互顺序，本节在串口与 MQTT 桥接机制分析之后给出 MQTT 通信时序图，以展示系统在上行监测、下行控制和求助消息传递中的完整链路，如图4-5所示。"
        ],
    )
    insert_before_heading(
        doc,
        "4.4.3 远程控制与阈值下发模块实现",
        [
            "为了使界面实现效果与前述实时展示逻辑形成对应关系，本节在实时数据展示模块说明之后给出 Qt 实时监控页面截图，用于展示系统对环境状态、风险等级、趋势曲线和总体运行状态的综合呈现效果，如图4-6所示。",
            "除环境监测外，客户端还需要对水电资源状态进行实时表达，因此在实时监控页面之后补充 Qt 水电数据页面截图，以进一步说明系统在资源剩余量、当前负载与水流趋势展示方面的实现效果，如图4-7所示。",
        ],
    )
    insert_before_heading(
        doc,
        "4.4.4 报警与求助处理模块实现",
        [
            "为了使远程控制与阈值下发部分的实现结果更加直观，本节在模块逻辑说明之后给出 Qt 设备管理页面截图，用于展示阈值远程设置、执行器控制以及远程操作日志三部分功能在同一管理界面中的组织方式，如图4-8所示。"
        ],
    )
    insert_before_heading(
        doc,
        "4.5.3 历史趋势展示模块实现",
        [
            "为了说明浏览器端在轻量展示场景下的整体界面布局和信息组织方式，本节在实时数据展示模块说明之后给出 Web 看板页面截图，使读者能够直观了解环境指标、资源状态、执行器状态和报警区域的可视化呈现效果，如图4-9所示。"
        ],
    )
    insert_before_heading(
        doc,
        "4.6 数据库详细设计与实现",
        [
            "为了补充说明一键求助功能在前端交互层面的具体表现形式，本节在功能机制说明之后给出 Web 端提示界面截图，用于展示求助或告警触发后页面向用户反馈的可视化效果，如图4-10所示。"
        ],
    )

    insert_before_heading(
        doc,
        "5.6.2 存在问题",
        [
            "为了更直观地展示系统在长时间连续运行条件下各关键监测指标的变化趋势以及整体运行波动情况，本节在测试总结之后给出 72 小时连续运行数据曲线，以作为系统持续运行能力的图形化证据，如图5-1所示。"
        ],
    )

    try:
        doc.save(str(DOCX_PATH))
        print(f"已写入：{DOCX_PATH}")
    except PermissionError:
        doc.save(str(FALLBACK_PATH))
        print(f"原文件被占用，已写入：{FALLBACK_PATH}")


if __name__ == "__main__":
    main()
