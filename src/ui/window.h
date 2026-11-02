#pragma once
#include "core/monitor.h"
#include "core/auth.h"
#include "platform/application_presentation.h"
#include <QMainWindow>
#include <QSystemTrayIcon>

class QTableWidget;
class QLabel;
class QPushButton;
class QMenu;
class QStackedWidget;
namespace runperch {
class Window final : public QMainWindow {
    Q_OBJECT
public:
    Window(Monitor&, Platform&, GitHubAuth* auth=nullptr, ApplicationPresentation* presentation=nullptr, QWidget* parent=nullptr);
protected:
    void closeEvent(QCloseEvent*) override;
    void showEvent(QShowEvent*) override;
    void hideEvent(QHideEvent*) override;
private:
    bool statusAreaAvailable() const;
    void updateDockVisibility();
    void rebuild();
    void details();
    void trayMenu();
    void addRunner();
    void settings();
    void history();
    void authorization();
    void control(ServiceAction);
    void persist();
    Monitor& monitor;
    Platform& platform;
    GitHubAuth* auth;
    ApplicationPresentation* presentation;
    QPushButton* connectButton;
    QTableWidget* table;
    QLabel* summary;
    QLabel* title;
    QLabel* identity;
    QLabel* states;
    QLabel* activity;
    QLabel* resources;
    QLabel* explanation;
    QLabel* observed;
    QPushButton* githubButton;
    QPushButton* startButton;
    QPushButton* stopButton;
    QSystemTrayIcon* tray=nullptr;
    bool dockVisible=true;
    QMenu* menu;
    QString selectedRoot;
};
}
