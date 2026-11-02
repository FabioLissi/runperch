#include "ui/window.h"
#include <QApplication>
#include <QMenu>
#include <QSettings>
#include <QTemporaryDir>
#include <QTextStream>
#import <AppKit/AppKit.h>

namespace {
class NoService final : public runperch::Platform {
public:
    QString description() const override { return "Presentation test"; }
    QStringList suggestedRunnerDirectories() const override { return {}; }
    QString githubExecutable() const override { return {}; }
    void inspect(const runperch::Registration&,runperch::ServiceCallback cb) override { cb({}); }
    void act(const runperch::Registration&,runperch::ServiceAction,runperch::ActionCallback cb) override { cb(false,"No service actions in this test"); }
};
class NoNetwork final : public runperch::RemoteProvider {
public:
    void observe(const runperch::Registration&,runperch::RemoteCallback cb) override { cb({}); }
};
}
int main(int argc,char** argv) {
    QApplication app(argc,argv); app.setQuitOnLastWindowClosed(false);
    QTemporaryDir settings;
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,settings.path());
    app.setOrganizationName("RunperchTests"); app.setApplicationName("NativePresence");
    auto presentation=runperch::createApplicationPresentation();
    const auto initial=[[NSApplication sharedApplication] activationPolicy];
    NoService platform; NoNetwork remote; runperch::Monitor monitor(platform,remote);
    runperch::Window window(monitor,platform,nullptr,presentation.get());
    window.show(); app.processEvents();
    auto* tray=window.findChild<QSystemTrayIcon*>();
    window.close(); app.processEvents();
    const bool hidden=!window.isVisible() && tray && tray->isVisible()
        && [[NSApplication sharedApplication] activationPolicy]==NSApplicationActivationPolicyAccessory;
    bool reopened=false;
    if(tray && tray->contextMenu()) for(auto* action : tray->contextMenu()->actions()) {
        if(action->text()=="Open Runperch") { action->trigger(); reopened=true; break; }
    }
    app.processEvents();
    const bool shown=reopened && window.isVisible()
        && [[NSApplication sharedApplication] activationPolicy]==NSApplicationActivationPolicyRegular;
    [[NSApplication sharedApplication] setActivationPolicy:initial];
    QTextStream(stdout)<<"Close retains menu bar and removes Dock: "<<hidden<<"; menu reopen restores window/Dock: "<<shown<<'\n';
    return hidden && shown ? 0 : 1;
}
