#include "logindialog.h"
#include "authdialog_styles.h"

#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QCheckBox>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QMouseEvent>
#include <QIcon>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPushButton>
#include <QSizePolicy>
#include <QVBoxLayout>
#include <QAction>
#include <QDebug>

#include "authservice.h"
#include "securestore.h"

namespace {
QIcon buildEyeIcon(bool hidden) {
    QPixmap pixmap(16, 16);
    pixmap.fill(Qt::transparent);

    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing);

    QColor c("#E0E7FF");
    QPen pen(c);
    pen.setWidthF(1.5);
    p.setPen(pen);

    p.drawEllipse(2, 4, 12, 8);
    p.setBrush(c);
    p.drawEllipse(6, 6, 4, 4);

    if (hidden) {
        p.drawLine(3, 13, 13, 3);
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

LoginDialog::LoginDialog(AuthService* auth, QWidget* parent)
    : QDialog(parent), m_auth(auth) {
    setObjectName("loginDialog");
    setWindowTitle(QString());
    resize(520, 500);
    setMinimumSize(520, 500);

    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground, false);
    setAttribute(Qt::WA_StyledBackground, true);
    setWindowIcon(buildAuthWindowIcon());
    m_bgPixmap = QPixmap(":/assets/images/ISP0Y.jpg");

    buildUi();
    setStyleSheet(authDialogsStyleSheet());

    // 读取记住密码
    QString acc, pwd;
    bool remember = false;
    if (SecureStore::loadRemembered(acc, pwd, remember)) {
        m_accountEdit->setText(acc);
        m_passwordEdit->setText(pwd);
        m_rememberCheck->setChecked(remember);
    }
}

void LoginDialog::resizeEvent(QResizeEvent* event) {
    QDialog::resizeEvent(event);
    if (m_closeBtn != nullptr) {
        const int size = 34;
        const int margin = 12;
        m_closeBtn->setGeometry(width() - margin - size, margin, size, size);
        m_closeBtn->raise();
    }
}

void LoginDialog::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    if (!m_bgPixmap.isNull()) {
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
        painter.drawPixmap(0, 0, scaled.copy(x, y, width(), height()));
    }
}

void LoginDialog::mousePressEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton) {
        m_dragging = true;
        m_dragOffset = e->globalPosition().toPoint() - frameGeometry().topLeft();
        e->accept();
        return;
    }
    QDialog::mousePressEvent(e);
}

void LoginDialog::mouseMoveEvent(QMouseEvent* e) {
    if (m_dragging && (e->buttons() & Qt::LeftButton)) {
        move(e->globalPosition().toPoint() - m_dragOffset);
        e->accept();
        return;
    }
    QDialog::mouseMoveEvent(e);
}

void LoginDialog::mouseReleaseEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton) {
        m_dragging = false;
        e->accept();
        return;
    }
    QDialog::mouseReleaseEvent(e);
}

void LoginDialog::buildUi() {
    auto root = new QVBoxLayout(this);
    root->setContentsMargins(18, 18, 18, 18);
    root->setSpacing(0);

    if (m_closeBtn == nullptr) {
        m_closeBtn = new QPushButton(QString(QChar(0x00D7)), this);
        m_closeBtn->setObjectName("dialogCloseButton");
        m_closeBtn->setCursor(Qt::PointingHandCursor);
        m_closeBtn->setToolTip(QStringLiteral("关闭"));
        m_closeBtn->setFocusPolicy(Qt::NoFocus);
        connect(m_closeBtn, &QPushButton::clicked, this, &QDialog::reject);
        m_closeBtn->setStyleSheet(
            "QPushButton#dialogCloseButton{"
            "color:white;"
            "font-size:16px;"
            "font-weight:700;"
            "border:none;"
            "background:transparent;"
            "}"
            "QPushButton#dialogCloseButton:hover{color:rgba(255,77,79,1.0);}"
            "QPushButton#dialogCloseButton:pressed{color:rgba(255,120,117,1.0);}");
    }

    auto* loginCard = new QFrame(this);
    loginCard->setObjectName("loginCard");
    loginCard->setFrameShape(QFrame::NoFrame);
    loginCard->setAttribute(Qt::WA_StyledBackground, true);
    loginCard->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    auto* cardLayout = new QVBoxLayout(loginCard);
    cardLayout->setContentsMargins(32, 30, 32, 30);
    cardLayout->setSpacing(12);

    QLabel* title = new QLabel("欢迎登录", loginCard);
    title->setAlignment(Qt::AlignCenter);
    title->setObjectName("titleLabel");

    QLabel* sub = new QLabel("灾后临时安置点智慧水电管理与环境监测系统", loginCard);
    sub->setObjectName("loginSubTitle");
    sub->setAlignment(Qt::AlignCenter);

    auto* formWrap = new QWidget(loginCard);
    formWrap->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    formWrap->setMinimumWidth(360);
    auto* form = new QFormLayout(formWrap);
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    form->setHorizontalSpacing(14);
    form->setVerticalSpacing(10);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    m_accountEdit = new QLineEdit(loginCard);
    m_accountEdit->setPlaceholderText("用户名");
    applyLineEditPlaceholderStyle(m_accountEdit);

    m_passwordEdit = new QLineEdit(loginCard);
    m_passwordEdit->setPlaceholderText("请输入密码");
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    applyLineEditPlaceholderStyle(m_passwordEdit);

    m_togglePwdAction = m_passwordEdit->addAction(
        buildEyeIcon(true), QLineEdit::TrailingPosition);

    connect(m_togglePwdAction, &QAction::triggered,
            this, &LoginDialog::onTogglePassword);

    auto* accountLabel = new QLabel("账号", loginCard);
    accountLabel->setObjectName("formFieldLabel");
    auto* pwdLabel = new QLabel("密码", loginCard);
    pwdLabel->setObjectName("formFieldLabel");
    form->addRow(accountLabel, m_accountEdit);
    form->addRow(pwdLabel, m_passwordEdit);

    m_rememberCheck = new QCheckBox("记住密码", loginCard);

    m_loginBtn = new QPushButton("立即登录", loginCard);
    m_loginBtn->setObjectName("primaryButton");

    connect(m_loginBtn, &QPushButton::clicked, this, &LoginDialog::onLogin);
    connect(m_passwordEdit, &QLineEdit::returnPressed, this, &LoginDialog::onLogin);
    connect(m_accountEdit, &QLineEdit::returnPressed, this, &LoginDialog::onLogin);

    cardLayout->addWidget(title);
    cardLayout->addWidget(sub);
    cardLayout->addSpacing(10);
    cardLayout->addWidget(formWrap);
    cardLayout->addWidget(m_rememberCheck);
    cardLayout->addSpacing(8);
    cardLayout->addWidget(m_loginBtn);
    cardLayout->addStretch();

    root->addWidget(loginCard, 1);
}

QString LoginDialog::loginAccount() const { return m_loginAccount; }
QString LoginDialog::loginRole() const { return m_loginRole; }

bool LoginDialog::validateInputs() {
    return !(m_accountEdit->text().isEmpty() ||
             m_passwordEdit->text().isEmpty());
}

void LoginDialog::onLogin() {
    if (!validateInputs()) return;

    QString err;
    QString acc = m_accountEdit->text();

    if (!m_auth->login(acc, m_passwordEdit->text(), err)) {
        QMessageBox::warning(this, QStringLiteral("登录失败"), err);
        return;
    }

    m_loginAccount = acc;
    m_loginRole = m_auth->roleForUser(acc);

    SecureStore::saveRemembered(acc,
                                m_passwordEdit->text(),
                                m_rememberCheck->isChecked());

    accept();
}

void LoginDialog::onTogglePassword() {
    bool hidden = m_passwordEdit->echoMode() == QLineEdit::Password;
    m_passwordEdit->setEchoMode(hidden ? QLineEdit::Normal : QLineEdit::Password);
    m_togglePwdAction->setIcon(buildEyeIcon(!hidden));
}

