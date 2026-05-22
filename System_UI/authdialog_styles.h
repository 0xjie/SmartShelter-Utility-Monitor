#pragma once

#include <QString>

// 登录 / 注册卡片圆角（与 C++ 中背景铺满矩形窗口、仅卡片圆角描边的设计一致）
inline constexpr int kAuthCardCornerRadius = 20;

// 登录 / 注册对话框共用样式（在 LoginDialog / RegisterDialog 上 setStyleSheet）。
// 说明：Qt Style Sheets 不支持 CSS 的 transform: scale() 与 transition；悬停“放大”用
// padding / min-height / 边框粗细与颜色变化近似；过渡感依赖 Qt 原生重绘（无真正动画）。
inline QString authDialogsStyleSheet()
{
    const QString rpx = QString::number(kAuthCardCornerRadius);
    QString sheet = QString::fromUtf8(R"(
/* ========= 外壳：无边框整窗由代码铺满背景图；不设整窗圆角，四角由位图填充（非透明抠角） ========= */
QDialog#loginDialog,
QDialog#registerDialog {
    background-color: #0B1B46;
    color: #FFFFFF;
    font-family: "Microsoft YaHei UI", "Segoe UI", sans-serif;
    font-size: 14px;
    border: none;
    outline: none;
}

/* ========= 内容区：实底 + 双层描边感，背景铺满至圆角内侧（含四角） ========= */
QFrame#loginCard,
QFrame#registerCard {
    background-color: transparent;
    border: none;
    border-radius: 0px;
    margin: 0px;
    outline: none;
}

/* 主标题 */
QLabel#titleLabel {
    color: #F8FBFF;
    font-size: 26px;
    font-weight: 800;
    margin-top: 2px;
}

/* 副标题 / 次要说明 */
QLabel#loginSubTitle,
QLabel#registerSubTitle,
QLabel#secondaryLabel {
    color: #C7DCFF;
    font-size: 13px;
}

/* 表单行标签（主文字） */
QLabel#formFieldLabel {
    color: #D6E8FF;
    font-size: 14px;
    font-weight: 600;
}

/* 输入框：半透明白底、蓝边、白字 —— 占位符颜色请在代码里用 QPalette::PlaceholderText */
QDialog#loginDialog QLineEdit,
QDialog#registerDialog QLineEdit {
    min-height: 44px;
    max-height: 48px;
    border-radius: 14px;
    padding: 0 12px;
    background-color: rgba(255, 255, 255, 0.12);
    border: 1px solid rgba(125, 211, 252, 0.62);
    color: #FFFFFF;
    selection-background-color: #3B82F6;
    selection-color: #FFFFFF;
    font-size: 14px;
}

QDialog#loginDialog QLineEdit:hover,
QDialog#registerDialog QLineEdit:hover {
    /* 近似 scale(1.03)：略增内边距 + 更亮边框 */
    padding: 0 13px;
    min-height: 46px;
    max-height: 50px;
    border: 2px solid rgba(147, 197, 253, 1.0);
    background-color: rgba(255, 255, 255, 0.16);
}

QDialog#loginDialog QLineEdit:focus,
QDialog#registerDialog QLineEdit:focus {
    border: 2px solid #93C5FD;
    background-color: rgba(255, 255, 255, 0.18);
}

/* 主按钮 */
QPushButton#primaryButton {
    min-height: 44px;
    border-radius: 16px;
    border: 1px solid rgba(147, 197, 253, 0.9);
    background-color: qlineargradient(x1:0, y1:0, x2:1, y2:0,
        stop:0 #0F5FA8, stop:1 #28A6FF);
    color: #FFFFFF;
    font-size: 16px;
    font-weight: 700;
    padding: 8px 16px;
}

QPushButton#primaryButton:hover {
    /* 近似 scale(1.08) */
    padding: 10px 18px;
    min-height: 48px;
    background-color: qlineargradient(x1:0, y1:0, x2:1, y2:0,
        stop:0 #1673c9, stop:1 #4cc3ff);
    border: 1px solid #BFDBFE;
}

QPushButton#primaryButton:pressed {
    /* 近似 scale(0.98) */
    padding: 7px 15px;
    min-height: 42px;
    background-color: #0b4f92;
}

/* 次要按钮（发送验证码等） */
QPushButton#secondaryButton {
    min-height: 40px;
    min-width: 108px;
    border-radius: 14px;
    border: 1px solid rgba(147, 197, 253, 0.75);
    background-color: rgba(255, 255, 255, 0.14);
    color: #FFFFFF;
    font-size: 14px;
    font-weight: 600;
    padding: 6px 12px;
}

QPushButton#secondaryButton:hover {
    padding: 8px 14px;
    min-height: 44px;
    background-color: rgba(255, 255, 255, 0.22);
    border: 1px solid #BFDBFE;
}

QPushButton#secondaryButton:pressed {
    padding: 5px 11px;
    min-height: 38px;
    background-color: rgba(255, 255, 255, 0.10);
}

QPushButton#secondaryButton:disabled {
    color: #94A3B8;
    border: 1px solid rgba(148, 163, 184, 0.45);
    background-color: rgba(15, 23, 42, 0.25);
}

/* 注册页「发送验证码」：高度与同行 QLineEdit（44~50px 样式）对齐，固定 50px */
QDialog#registerDialog QPushButton#registerSendCodeButton {
    min-height: 50px;
    max-height: 50px;
    min-width: 120px;
    max-width: 120px;
    border-radius: 14px;
    border: 1px solid rgba(147, 197, 253, 0.75);
    background-color: rgba(255, 255, 255, 0.14);
    color: #FFFFFF;
    font-size: 14px;
    font-weight: 600;
    padding: 0px 10px;
}

QDialog#registerDialog QPushButton#registerSendCodeButton:hover {
    padding: 0px 10px;
    min-height: 50px;
    max-height: 50px;
    background-color: rgba(255, 255, 255, 0.22);
    border: 1px solid #BFDBFE;
}

QDialog#registerDialog QPushButton#registerSendCodeButton:pressed {
    padding: 0px 10px;
    min-height: 50px;
    max-height: 50px;
    background-color: rgba(255, 255, 255, 0.10);
}

QDialog#registerDialog QPushButton#registerSendCodeButton:disabled {
    color: #94A3B8;
    border: 1px solid rgba(148, 163, 184, 0.45);
    background-color: rgba(15, 23, 42, 0.25);
}

/* 文字链按钮 */
QPushButton#linkButton {
    min-height: 36px;
    border: none;
    border-radius: 14px;
    background: transparent;
    color: #E0E7FF;
    font-size: 14px;
    font-weight: 600;
    padding: 6px 8px;
}

QPushButton#linkButton:hover {
    color: #FFFFFF;
    background-color: rgba(255, 255, 255, 0.08);
    padding: 8px 10px;
}

QPushButton#linkButton:pressed {
    color: #CBD5E1;
    background-color: rgba(255, 255, 255, 0.05);
    padding: 5px 7px;
}

/* 复选框 */
QDialog#loginDialog QCheckBox,
QDialog#registerDialog QCheckBox {
    spacing: 8px;
    color: #E0E7FF;
    font-size: 13px;
}

QDialog#loginDialog QCheckBox::indicator,
QDialog#registerDialog QCheckBox::indicator {
    width: 18px;
    height: 18px;
    border-radius: 6px;
    border: 1px solid rgba(147, 197, 253, 0.85);
    background-color: rgba(255, 255, 255, 0.12);
}

QDialog#loginDialog QCheckBox::indicator:hover,
QDialog#registerDialog QCheckBox::indicator:hover {
    border: 1px solid #BFDBFE;
    background-color: rgba(255, 255, 255, 0.20);
}

QDialog#loginDialog QCheckBox::indicator:checked,
QDialog#registerDialog QCheckBox::indicator:checked {
    background-color: #2563EB;
    border: 1px solid #93C5FD;
}

/* 错误提示（保持醒目） */
QLabel#errorLabel {
    color: #FCA5A5;
    font-size: 12px;
}

/* 密码强度条 */
QDialog#registerDialog QProgressBar {
    min-height: 8px;
    max-height: 8px;
    border: none;
    border-radius: 4px;
    background-color: rgba(15, 23, 42, 0.35);
    text-align: center;
}

QDialog#registerDialog QProgressBar::chunk {
    border-radius: 4px;
    background-color: qlineargradient(x1:0, y1:0, x2:1, y2:0,
        stop:0 #38BDF8, stop:1 #2563EB);
}
)");
    sheet.replace(QStringLiteral("__AUTH_R__"), rpx);
    return sheet;
}
