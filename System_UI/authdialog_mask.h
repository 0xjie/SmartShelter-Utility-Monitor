#pragma once

#include <QBitmap>
#include <QPainter>
#include <QWidget>

#include <QtGlobal>

// 若仍对其它窗口使用 applyAuthDialogWindowMask，请与 authdialog_styles.h 中 kAuthCardCornerRadius 保持一致
inline constexpr int kAuthGlassCornerRadius = 30;

/**
 * Qt 圆角窗口蒙版（QWidget::setMask）。
 *
 * 使用 1-bit QBitmap + drawRoundedRect（抗锯齿），边缘通常优于
 * QRegion(QPainterPath::toFillPolygon()) 的折线逼近。
 *
 * 典型用法：无边框窗口 + 自绘/透明背景
 *   setWindowFlags(windowFlags() | Qt::FramelessWindowHint);
 *   setAttribute(Qt::WA_TranslucentBackground, true);  // 圆角外透明时建议开启
 *   applyRoundedWindowMask(this, 16);
 * 在 resizeEvent 中随尺寸重设蒙版。
 *
 * @param radiusY <= 0 时与 radiusX 相同
 */
inline void applyRoundedWindowMask(QWidget* w, int radiusX, int radiusY = -1)
{
    if (!w || radiusX <= 0) {
        return;
    }
    if (radiusY <= 0) {
        radiusY = radiusX;
    }

    const QSize sz = w->size();
    if (sz.isEmpty()) {
        return;
    }

    QBitmap bitmap(sz);
    bitmap.fill(Qt::color0);

    QPainter painter(&bitmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(Qt::NoPen);
    painter.setBrush(Qt::color1);

    const QRectF r(0, 0, qreal(sz.width()), qreal(sz.height()));
    const qreal rx = qMin(qreal(radiusX), r.width() / 2.0);
    const qreal ry = qMin(qreal(radiusY), r.height() / 2.0);
    painter.drawRoundedRect(r, rx, ry);

    w->setMask(bitmap);
}

/** 登录 / 注册对话框：与玻璃卡片圆角一致 */
inline void applyAuthDialogWindowMask(QWidget* w)
{
    applyRoundedWindowMask(w, kAuthGlassCornerRadius, kAuthGlassCornerRadius);
}
