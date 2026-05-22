#include "registerdialog.h"

#include "authdialog_styles.h"

#include <QAction>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPushButton>
#include <QRegularExpression>
#include <QSizePolicy>
#include <QVBoxLayout>

#include "authservice.h"

namespace {
QIcon buildEyeIcon(bool hidden) {
    QPixmap pixmap(16, 16);
    pixmap.fill(Qt::transparent);

    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing, true);
    const QColor ink("#E0E7FF");
    QPen pen(ink);
    pen.setWidthF(1.5);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(QRectF(2.0, 4.0, 12.0, 8.0));
    p.setBrush(ink);
    p.drawEllipse(QRectF(6.3, 6.3, 3.4, 3.4));

    if (hidden) {
        QPen slashPen(ink);
        slashPen.setWidthF(1.6);
        p.setPen(slashPen);
        p.drawLine(QPointF(3.0, 13.0), QPointF(13.0, 3.0));
    }

    return QIcon(pixmap);
}

void applyLineEditPlaceholderStyle(QLineEdit* edit) {
    if (!edit) {
        return;
    }
    QPalette pal = edit->palette();
    pal.setColor(QPalette::PlaceholderText, QColor(0xe8, 0xee, 0xff));
    edit->setPalette(pal);
}

QIcon buildAuthWindowIcon() {
    QPixmap pixmap(48, 48);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);

    QLinearGradient bg(0, 0, 48, 48);
    bg.setColorAt(0.0, QColor(0x0a, 0x2f, 0x63));
    bg.setColorAt(1.0, QColor(0x0f, 0x7a, 0xe5));
    painter.setPen(Qt::NoPen);
    painter.setBrush(bg);
    painter.drawRoundedRect(QRectF(2, 2, 44, 44), 12, 12);

    QPainterPath shield;
    shield.moveTo(24, 9);
    shield.lineTo(35, 14);
    shield.lineTo(33, 26);
    shield.quadTo(31, 35, 24, 40);
    shield.quadTo(17, 35, 15, 26);
    shield.lineTo(13, 14);
    shield.closeSubpath();
    painter.setBrush(QColor(255, 255, 255, 235));
    painter.drawPath(shield);

    painter.setBrush(QColor(0x0f, 0x5f, 0xa8));
    painter.drawRoundedRect(QRectF(21, 16, 6, 14), 2, 2);
    painter.drawRoundedRect(QRectF(17, 20, 14, 6), 2, 2);

    return QIcon(pixmap);
}
}  // namespace

RegisterDialog::RegisterDialog(AuthService* auth, QWidget* parent)
    : QDialog(parent), m_auth(auth) {
    setObjectName("registerDialog");
    setWindowTitle(QString());
    resize(520, 480);
    setMinimumSize(520, 480);

    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground, false);
    setAttribute(Qt::WA_StyledBackground, true);
    setWindowIcon(buildAuthWindowIcon());

    m_bgPixmap = QPixmap(":/assets/images/ISP0Y.jpg");

    buildUi();

    setStyleSheet(authDialogsStyleSheet());

    connect(m_usernameEdit, &QLineEdit::textChanged, this, &RegisterDialog::onUsernameChanged);
    connect(m_confirmEdit, &QLineEdit::textChanged, this, &RegisterDialog::onConfirmChanged);
}

void RegisterDialog::resizeEvent(QResizeEvent* event) {
    QDialog::resizeEvent(event);
    if (m_closeBtn != nullptr) {
        const int size = 34;
        const int margin = 12;
        m_closeBtn->setGeometry(width() - margin - size, margin, size, size);
        m_closeBtn->raise();
    }
}

void RegisterDialog::paintEvent(QPaintEvent* event) {
    QDialog::paintEvent(event);

    if (m_bgPixmap.isNull()) {
        return;
    }

    const QPixmap scaled = m_bgPixmap.scaled(size(),
                                             Qt::KeepAspectRatioByExpanding,
                                             Qt::SmoothTransformation);

    int x = (scaled.width() - width()) / 2;
    int y = (scaled.height() - height()) / 2;
    if (x < 0) {
        x = 0;
    }
    if (y < 0) {
        y = 0;
    }

    const QPixmap cropped = scaled.copy(x, y, width(), height());

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    const qreal radius = 12.0;
    QPainterPath path;
    path.addRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), radius, radius);
    painter.setClipPath(path);
    painter.drawPixmap(0, 0, cropped);
}

void RegisterDialog::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        m_dragging = true;
        m_dragOffset = event->globalPosition().toPoint() - frameGeometry().topLeft();
        event->accept();
        return;
    }
    QDialog::mousePressEvent(event);
}

void RegisterDialog::mouseMoveEvent(QMouseEvent* event) {
    if (m_dragging && (event->buttons() & Qt::LeftButton)) {
        move(event->globalPosition().toPoint() - m_dragOffset);
        event->accept();
        return;
    }
    QDialog::mouseMoveEvent(event);
}

void RegisterDialog::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        m_dragging = false;
        event->accept();
        return;
    }
    QDialog::mouseReleaseEvent(event);
}

void RegisterDialog::buildUi() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(18, 18, 18, 18);
    root->setSpacing(0);

    if (m_closeBtn == nullptr) {
        m_closeBtn = new QPushButton(QString(QChar(0x00D7)), this);
        m_closeBtn->setObjectName("registerDialogCloseButton");
        m_closeBtn->setCursor(Qt::PointingHandCursor);
        m_closeBtn->setToolTip(QStringLiteral("关闭"));
        m_closeBtn->setFocusPolicy(Qt::NoFocus);
        connect(m_closeBtn, &QPushButton::clicked, this, &QDialog::reject);
        m_closeBtn->setStyleSheet(
            "QPushButton#registerDialogCloseButton{"
            "color:white;"
            "font-size:16px;"
            "font-weight:800;"
            "border:none;"
            "background:transparent;"
            "}"
            "QPushButton#registerDialogCloseButton:hover{color:rgba(255,77,79,1.0);}"
            "QPushButton#registerDialogCloseButton:pressed{color:rgba(255,120,117,1.0);}");
    }
    const int size = 34;
    const int margin = 12;
    m_closeBtn->setGeometry(width() - margin - size, margin, size, size);
    m_closeBtn->raise();

    auto* registerCard = new QFrame(this);
    registerCard->setObjectName("registerCard");
    registerCard->setFrameShape(QFrame::NoFrame);
    registerCard->setLineWidth(0);
    registerCard->setAttribute(Qt::WA_StyledBackground, true);
    registerCard->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    auto* cardLayout = new QVBoxLayout(registerCard);
    cardLayout->setContentsMargins(32, 30, 32, 30);
    cardLayout->setSpacing(10);

    auto* title = new QLabel("创建账号", registerCard);
    title->setObjectName("titleLabel");
    title->setAlignment(Qt::AlignCenter);

    auto* subTitle = new QLabel("填写信息完成注册", registerCard);
    subTitle->setObjectName("registerSubTitle");
    subTitle->setAlignment(Qt::AlignCenter);

    auto* formWrap = new QWidget(registerCard);
    formWrap->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::MinimumExpanding);
    formWrap->setMinimumWidth(360);
    auto* form = new QFormLayout(formWrap);
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    form->setFormAlignment(Qt::AlignTop);
    form->setHorizontalSpacing(14);
    form->setVerticalSpacing(10);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    auto createFieldBox = [registerCard](QLineEdit*& edit, QLabel*& err, const QString& ph) {
        edit = new QLineEdit(registerCard);
        edit->setPlaceholderText(ph);
        applyLineEditPlaceholderStyle(edit);
        err = new QLabel(registerCard);
        err->setObjectName("errorLabel");
        auto* box = new QWidget(registerCard);
        auto* layout = new QVBoxLayout(box);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(2);
        layout->addWidget(edit);
        layout->addWidget(err);
        return box;
    };

    auto* usernameLabel = new QLabel("用户名", registerCard);
    usernameLabel->setObjectName("formFieldLabel");
    auto* pwdLabel = new QLabel("密码", registerCard);
    pwdLabel->setObjectName("formFieldLabel");
    auto* confirmLabel = new QLabel("确认密码", registerCard);
    confirmLabel->setObjectName("formFieldLabel");

    form->addRow(usernameLabel, createFieldBox(m_usernameEdit, m_usernameErr, "4-20位字母数字下划线"));

    m_pwdEdit = new QLineEdit(registerCard);
    m_pwdEdit->setEchoMode(QLineEdit::Password);
    m_pwdEdit->setPlaceholderText("至少6位密码");
    m_pwdEdit->setMinimumHeight(44);
    m_pwdEdit->setMaximumHeight(52);
    applyLineEditPlaceholderStyle(m_pwdEdit);
    m_togglePwdAction = m_pwdEdit->addAction(buildEyeIcon(true), QLineEdit::TrailingPosition);
    connect(m_togglePwdAction, &QAction::triggered, this, &RegisterDialog::onTogglePassword);

    m_pwdErr = new QLabel(registerCard);
    m_pwdErr->setObjectName("errorLabel");

    auto* pwdBox = new QWidget(registerCard);
    auto* pwdBoxLayout = new QVBoxLayout(pwdBox);
    pwdBoxLayout->setContentsMargins(0, 0, 0, 0);
    pwdBoxLayout->setSpacing(2);
    pwdBoxLayout->addWidget(m_pwdEdit);
    pwdBoxLayout->addWidget(m_pwdErr);
    form->addRow(pwdLabel, pwdBox);

    auto* confirmBox = new QWidget(registerCard);
    confirmBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    auto* confirmBoxLayout = new QVBoxLayout(confirmBox);
    confirmBoxLayout->setContentsMargins(0, 0, 0, 0);
    confirmBoxLayout->setSpacing(2);
    m_confirmEdit = new QLineEdit(registerCard);
    m_confirmEdit->setEchoMode(QLineEdit::Password);
    m_confirmEdit->setPlaceholderText("再次输入密码");
    m_confirmEdit->setMinimumHeight(44);
    m_confirmEdit->setMaximumHeight(52);
    applyLineEditPlaceholderStyle(m_confirmEdit);
    confirmBoxLayout->addWidget(m_confirmEdit);
    m_confirmErr = new QLabel(registerCard);
    m_confirmErr->setObjectName("errorLabel");
    confirmBoxLayout->addWidget(m_confirmErr);
    form->addRow(confirmLabel, confirmBox);

    m_registerBtn = new QPushButton("确认注册", registerCard);
    m_registerBtn->setObjectName("primaryButton");
    m_backLoginBtn = new QPushButton("返回登录", registerCard);
    m_backLoginBtn->setObjectName("linkButton");
    connect(m_registerBtn, &QPushButton::clicked, this, &RegisterDialog::onRegister);
    connect(m_backLoginBtn, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_confirmEdit, &QLineEdit::returnPressed, this, &RegisterDialog::onRegister);

    cardLayout->addWidget(title);
    cardLayout->addWidget(subTitle);
    cardLayout->addSpacing(6);
    cardLayout->addWidget(formWrap);
    cardLayout->addSpacing(6);
    cardLayout->addWidget(m_registerBtn);
    cardLayout->addWidget(m_backLoginBtn);
    cardLayout->addStretch(1);

    root->addWidget(registerCard, 1);
}

void RegisterDialog::onUsernameChanged(const QString& text) {
    static const QRegularExpression re(R"(^[A-Za-z0-9_]{4,20}$)");
    if (text.isEmpty()) {
        m_usernameErr->clear();
        return;
    }
    if (!re.match(text).hasMatch()) {
        m_usernameErr->setText("用户名需为4-20位字母/数字/下划线");
        return;
    }
    m_usernameErr->setText(m_auth->isUsernameAvailable(text) ? "" : "用户名已被占用");
}

void RegisterDialog::onConfirmChanged(const QString& text) {
    if (text.isEmpty()) {
        m_confirmErr->clear();
        return;
    }
    m_confirmErr->setText(text == m_pwdEdit->text() ? "" : "两次密码不一致");
}

bool RegisterDialog::validateAll() {
    m_pwdErr->clear();

    bool ok = true;
    if (!m_auth->isUsernameAvailable(m_usernameEdit->text()) || !m_usernameErr->text().isEmpty()) {
        ok = false;
    }

    if (m_pwdEdit->text().size() < 6) {
        m_pwdErr->setText("密码长度至少6位");
        ok = false;
    }
    if (m_confirmEdit->text() != m_pwdEdit->text()) {
        m_confirmErr->setText("两次密码不一致");
        ok = false;
    }
    return ok;
}

void RegisterDialog::onRegister() {
    if (!validateAll()) {
        return;
    }

    QString err;
    const bool ok =
        m_auth->registerUser(m_usernameEdit->text().trimmed(), m_pwdEdit->text(), err);

    if (!ok) {
        QMessageBox::warning(this, "注册失败", err);
        return;
    }

    QMessageBox::information(this, "成功", "注册成功，请返回登录");
    accept();
}

void RegisterDialog::onTogglePassword() {
    const bool hidden = m_pwdEdit->echoMode() == QLineEdit::Password;
    m_pwdEdit->setEchoMode(hidden ? QLineEdit::Normal : QLineEdit::Password);
    m_togglePwdAction->setIcon(buildEyeIcon(!hidden));
}
