#include <QApplication>
#include <QDialog>
#include <QFile>
#include <QPointer>
#include <QObject>
#include <QTextStream>
#include <functional>

#include "authservice.h"
#include "logindialog.h"
#include "mainwindow.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);

    QFile f("style.qss");
    if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&f);
        app.setStyleSheet(in.readAll());
    }

    AuthService auth;
    std::function<void()> showLogin;
    showLogin = [&]() {
        auto* dlg = new LoginDialog(&auth);
        QObject::connect(dlg, &QDialog::finished, &app, [&, dlg](int result) {
            const QString account = dlg->loginAccount();
            const QString role = dlg->loginRole();
            dlg->deleteLater();

            if (result != QDialog::Accepted) {
                app.quit();
                return;
            }

            auto* mainWindow = new MainWindow(account, role);
            QObject::connect(mainWindow, &MainWindow::switchAccountRequested, &app, [&, mainWindow]() {
                mainWindow->deleteLater();
                showLogin();
            });
            QObject::connect(mainWindow, &QWidget::destroyed, &app, [&app]() {
                bool hasTopLevel = false;
                const auto windows = QApplication::topLevelWidgets();
                for (QWidget* w : windows) {
                    if (w != nullptr && w->isVisible()) {
                        hasTopLevel = true;
                        break;
                    }
                }
                if (!hasTopLevel) {
                    app.quit();
                }
            });
            mainWindow->show();
        });
        dlg->show();
    };

    showLogin();
    return app.exec();
}
