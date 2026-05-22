#pragma once

#include <QObject>
#include <QDialog>
#include <QEvent>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPoint>
#include <QString>
#include <QPixmap>
#include <QWidget>

class AuthService;
class QAction;
class QLabel;
class QLineEdit;
class QPushButton;
class QResizeEvent;

class RegisterDialog : public QDialog {
    Q_OBJECT
public:
    explicit RegisterDialog(AuthService* auth, QWidget* parent = nullptr);

private slots:
    void onRegister();
    void onUsernameChanged(const QString& text);
    void onConfirmChanged(const QString& text);
    void onTogglePassword();

private:
    void resizeEvent(QResizeEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

    void buildUi();
    bool validateAll();

    AuthService* m_auth = nullptr;

    QLineEdit* m_usernameEdit = nullptr;
    QLineEdit* m_pwdEdit = nullptr;
    QLineEdit* m_confirmEdit = nullptr;

    QAction* m_togglePwdAction = nullptr;

    QPushButton* m_registerBtn = nullptr;
    QPushButton* m_backLoginBtn = nullptr;
    QPushButton* m_closeBtn = nullptr;

    QLabel* m_usernameErr = nullptr;
    QLabel* m_pwdErr = nullptr;
    QLabel* m_confirmErr = nullptr;

    QPixmap m_bgPixmap;
    bool m_dragging = false;
    QPoint m_dragOffset;
};
