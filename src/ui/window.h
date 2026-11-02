#pragma once
#include "core/monitor.h"
#include "core/auth.h"
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
    Window(Monitor&, Platform&, GitHubAuth* auth=nullptr, QWidget* parent=nullptr);
protected:
    void closeEvent(QCloseEvent*) override;
private:
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
    QSystemTrayIcon* tray;
    QMenu* menu;
    QString selectedRoot;
};
}
