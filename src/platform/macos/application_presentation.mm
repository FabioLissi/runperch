#include "platform/application_presentation.h"
#import <AppKit/AppKit.h>
#include <QWidget>
#include <QGuiApplication>
#include <QPointer>
#include <QCursor>
#include <QVariant>
#include <QEvent>

namespace runperch {
namespace {
class PanelEvents final : public QObject {
public:
    explicit PanelEvents(QWidget* panel):QObject(panel),popup(panel) { panel->installEventFilter(this); }
    ~PanelEvents() override { stop(); }
protected:
    bool eventFilter(QObject*,QEvent* event) override {
        if(event->type()==QEvent::Show) start();
        else if(event->type()==QEvent::Hide) stop();
        return false;
    }
private:
    void start() {
        if(mouse) return;
        const QPointer<QWidget> panel=popup;
        // A non-activating panel leaves another application in front. A delayed
        // notification for that same application is not a new app switch.
        const pid_t foreground=NSWorkspace.sharedWorkspace.frontmostApplication.processIdentifier;
        mouse=[NSEvent addGlobalMonitorForEventsMatchingMask:NSEventMaskLeftMouseDown | NSEventMaskRightMouseDown | NSEventMaskOtherMouseDown handler:^(NSEvent*) {
            if(panel && panel->isVisible() && !panel->property("trayAnchor").toRect().contains(QCursor::pos())) panel->hide();
        }];
        workspace=[NSWorkspace.sharedWorkspace.notificationCenter addObserverForName:NSWorkspaceDidActivateApplicationNotification object:nil queue:NSOperationQueue.mainQueue usingBlock:^(NSNotification* notification) {
            NSRunningApplication* app=notification.userInfo[NSWorkspaceApplicationKey];
            if(panel && panel->isVisible() && app.processIdentifier!=foreground
               && app.processIdentifier!=NSProcessInfo.processInfo.processIdentifier) panel->hide();
        }];
    }
    void stop() {
        if(mouse) { [NSEvent removeMonitor:mouse]; mouse=nil; }
        if(workspace) { [NSWorkspace.sharedWorkspace.notificationCenter removeObserver:workspace]; workspace=nil; }
    }
    QPointer<QWidget> popup;
    id mouse=nil,workspace=nil;
};
class MacApplicationPresentation final : public ApplicationPresentation {
public:
    Qt::WindowFlags popoverFlags() const override { return QGuiApplication::platformName()=="cocoa" ? Qt::Tool | Qt::FramelessWindowHint : Qt::Popup; }
    void configurePopover(QWidget* widget) override {
        if(QGuiApplication::platformName()!="cocoa") return;
        widget->setAttribute(Qt::WA_MacAlwaysShowToolWindow);
        widget->setAttribute(Qt::WA_ShowWithoutActivating);
        auto* view=reinterpret_cast<NSView*>(widget->winId());
        if([view.window isKindOfClass:[NSPanel class]]) {
            view.window.styleMask |= NSWindowStyleMaskNonactivatingPanel;
            view.window.hidesOnDeactivate=NO;
        }
        new PanelEvents(widget);
    }
    bool supportsDockVisibility() const override { return true; }
    bool setDockVisible(bool visible) override {
        if(![NSThread isMainThread]) return false;
        return [[NSApplication sharedApplication] setActivationPolicy:visible
            ? NSApplicationActivationPolicyRegular : NSApplicationActivationPolicyAccessory];
    }
};
}
std::unique_ptr<ApplicationPresentation> createApplicationPresentation() { return std::make_unique<MacApplicationPresentation>(); }
}
