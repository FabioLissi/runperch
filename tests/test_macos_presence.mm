#include "ui/window.h"
#include "ui/popover.h"
#include <QKeyEvent>
#include <QTimer>
#include <QTest>
#include <QPushButton>
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
    QTimer::singleShot(0,&window,[&] {
    window.show(); app.processEvents();
    auto* tray=window.findChild<QSystemTrayIcon*>();
    window.close(); app.processEvents();
    const bool hidden=!window.isVisible() && tray && tray->isVisible()
        && [[NSApplication sharedApplication] activationPolicy]==NSApplicationActivationPolicyAccessory;
    auto* popup=window.findChild<runperch::RunnerPopover*>();
    tray->activated(QSystemTrayIcon::Trigger);
    const bool opened=QTest::qWaitFor([&] { return popup && popup->isVisible(); },1500); app.processEvents();
    QTextStream(stdout)<<"Popup visible: "<<(popup && popup->isVisible())<<"; overview visible: "<<window.isVisible()<<"; activation policy: "<<int([[NSApplication sharedApplication] activationPolicy])<<'\n';
    const bool compact=opened && popup && popup->isVisible() && !window.isVisible()
        && [[NSApplication sharedApplication] activationPolicy]==NSApplicationActivationPolicyAccessory;
    NSView* panelView=popup ? reinterpret_cast<NSView*>(popup->winId()) : nil;
    const bool panelNonactivating=panelView && [panelView.window isKindOfClass:[NSPanel class]]
        && (panelView.window.styleMask & NSWindowStyleMaskNonactivatingPanel);
    NSRunningApplication* foreground=NSWorkspace.sharedWorkspace.frontmostApplication;
    [NSWorkspace.sharedWorkspace.notificationCenter postNotificationName:NSWorkspaceDidActivateApplicationNotification object:nil userInfo:@{NSWorkspaceApplicationKey:foreground}];
    app.processEvents();
    const bool duplicateIgnored=popup && popup->isVisible();
    NSRunningApplication* different=nil;
    for(NSRunningApplication* candidate in NSWorkspace.sharedWorkspace.runningApplications) {
        if(candidate.processIdentifier!=foreground.processIdentifier && candidate.processIdentifier!=NSProcessInfo.processInfo.processIdentifier) { different=candidate; break; }
    }
    if(different) [NSWorkspace.sharedWorkspace.notificationCenter postNotificationName:NSWorkspaceDidActivateApplicationNotification object:nil userInfo:@{NSWorkspaceApplicationKey:different}];
    const bool switchDismissed=different && QTest::qWaitFor([&] { return !popup->isVisible(); },1500);
    tray->activated(QSystemTrayIcon::Trigger);
    const bool afterSwitch=QTest::qWaitFor([&] { return popup && popup->isVisible(); },1500);
    QKeyEvent escape(QEvent::KeyPress,Qt::Key_Escape,Qt::NoModifier);
    if(popup) app.sendEvent(popup,&escape); app.processEvents();
    const bool dismissed=popup && !popup->isVisible() && !window.isVisible();
    tray->activated(QSystemTrayIcon::Trigger);
    const bool openedAgain=QTest::qWaitFor([&] { return popup && popup->isVisible(); },1500);
    if(openedAgain) popup->findChild<QPushButton*>("popoverOverview")->click(); app.processEvents();
    const bool fromPopup=openedAgain && window.isVisible() && popup && !popup->isVisible()
        && [[NSApplication sharedApplication] activationPolicy]==NSApplicationActivationPolicyRegular;
    window.close(); app.processEvents();
    bool reopened=false;
    if(auto* menu=window.findChild<QMenu*>("trayActions")) for(auto* action : menu->actions()) {
        if(action->text()=="Open Runperch") { action->trigger(); reopened=true; break; }
    }
    app.processEvents();
    const bool shown=reopened && window.isVisible()
        && [[NSApplication sharedApplication] activationPolicy]==NSApplicationActivationPolicyRegular;
    [[NSApplication sharedApplication] setActivationPolicy:initial];
    QTextStream(stdout)<<"Close retains menu bar and removes Dock: "<<hidden<<"; menu reopen restores window/Dock: "<<shown<<'\n';
    QTextStream(stdout)<<"Compact popup retains accessory mode: "<<compact<<"; native non-activating panel: "<<panelNonactivating<<"; Escape dismisses: "<<dismissed<<"; popup opens overview/Dock: "<<fromPopup<<'\n';
    QTextStream(stdout)<<"Duplicate foreground notification retained panel: "<<duplicateIgnored<<"; synthetic app switch dismissed: "<<switchDismissed<<"; reopened: "<<afterSwitch<<'\n';
    app.exit(hidden && shown && compact && panelNonactivating && dismissed && fromPopup && duplicateIgnored && switchDismissed && afterSwitch ? 0 : 1);
    });
    return app.exec();
}
