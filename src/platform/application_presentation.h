#pragma once
#include <QSystemTrayIcon>
#include <memory>

namespace runperch {
class ApplicationPresentation {
public:
    virtual ~ApplicationPresentation() = default;
    virtual bool statusAreaAvailable() const { return QSystemTrayIcon::isSystemTrayAvailable(); }
    virtual Qt::WindowFlags popoverFlags() const { return Qt::Popup; }
    virtual void configurePopover(QWidget*) {}
    virtual bool supportsDockVisibility() const { return false; }
    virtual bool setDockVisible(bool) { return false; }
};
std::unique_ptr<ApplicationPresentation> createApplicationPresentation();
}
