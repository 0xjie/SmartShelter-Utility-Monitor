#pragma once

#include <QObject>
#include <QDialog>
#include <QEvent>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPoint>
#include <QPixmap>
#include <QString>
#include <QWidget>

class AuthService;
class QAction;
class QCheckBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QResizeEvent;

class LoginDialog : public QDialog {
    Q_OBJECT
public:
    explicit LoginDialog(AuthService* auth, QWidget* parent = nullptr);

    // 登录成功后，main.cpp 可通过这两个函数获取当前账号和角色
    QString loginAccount() const;
    QString loginRole() const;

private slots:
    void onLogin();
    void onTogglePassword();
    void openRegister();

private:
    void resizeEvent(QResizeEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

    void buildUi();
    bool validateInputs();

    AuthService* m_auth = nullptr;

    QLineEdit* m_accountEdit = nullptr;
    QLineEdit* m_passwordEdit = nullptr;
    QAction* m_togglePwdAction = nullptr;
    QCheckBox* m_rememberCheck = nullptr;

    QLabel* m_accountErr = nullptr;
    QLabel* m_passwordErr = nullptr;
    QLabel* m_globalErr = nullptr;

    QPushButton* m_loginBtn = nullptr;
    QPushButton* m_registerBtn = nullptr;
    QPushButton* m_closeBtn = nullptr;

    // 登录成功后保存下来，供外部读取
    QString m_loginAccount;
    QString m_loginRole;

    // 背景图缓存（从 Qt 资源 :/images/ISP0Y.jpg 加载）
    // 只加载一次，避免每次重绘都重复读资源
    QPixmap m_bgPixmap;

    bool m_dragging = false;
    QPoint m_dragOffset;
};
