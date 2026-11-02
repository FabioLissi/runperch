#include "ui/window.h"
#include <QApplication>
#include <QFile>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QTimer>
#include <QSettings>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTest>
using namespace runperch;
namespace {
class TestPlatform final : public Platform {
public:
    ServiceObservation value;
    int actions=0;
    QString description() const override { return "Test computer"; }
    QStringList suggestedRunnerDirectories() const override { return {}; }
    QString githubExecutable() const override { return {}; }
    void inspect(const Registration&,ServiceCallback cb) override { cb(value); }
    void act(const Registration&,ServiceAction action,ActionCallback cb) override {
        ++actions; value.state=action==ServiceAction::Start ? ServiceState::Running : ServiceState::Stopped;
        value.observedAt=QDateTime::currentDateTimeUtc(); cb(true,"Test service request accepted");
    }
};
class TestRemote final : public RemoteProvider {
public:
    RemoteObservation value;
    int calls=0;
    void observe(const Registration&,RemoteCallback cb) override { ++calls; cb(value); }
};
class TestPresentation final : public ApplicationPresentation {
public:
    bool available=true, succeeds=true, dockVisible=true;
    QList<bool> requests;
    bool statusAreaAvailable() const override { return available; }
    bool supportsDockVisibility() const override { return true; }
    bool setDockVisible(bool visible) override {
        requests.append(visible); if(succeeds) dockVisible=visible; return succeeds;
    }
};
}
class UiTests : public QObject {
    Q_OBJECT
private slots:
    void init() { QSettings().clear(); qApp->setQuitOnLastWindowClosed(false); }
    void localStopRequiresConfirmationAndStartWorksWithoutGitHub() {
        QTemporaryDir runner;
        QFile file(runner.filePath(".runner")); QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("{\"agentId\":42,\"agentName\":\"ui-runner\",\"gitHubUrl\":\"https://github.com/example/project\"}"); file.close();
        TestPlatform platform; TestRemote remote;
        platform.value.state=ServiceState::Running; platform.value.controlsAvailable=true;
        platform.value.pid=123; platform.value.observedAt=QDateTime::currentDateTimeUtc();
        Monitor monitor(platform,remote); QVERIFY(monitor.add(runner.path()));
        Window window(monitor,platform); window.show(); monitor.refresh(true);
        auto* stop=window.findChild<QPushButton*>("stopService"); auto* start=window.findChild<QPushButton*>("startService");
        QVERIFY(stop); QVERIFY(start); QVERIFY(stop->isEnabled()); QVERIFY(!start->isEnabled());
        auto* states=window.findChild<QLabel*>("runnerStates"); QVERIFY(states->text().contains("PID 123"));
        auto warning=std::make_shared<bool>(false);
        QTimer::singleShot(0,&window,[warning] {
            auto* box=qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
            if(box) { *warning=box->defaultButton()==box->button(QMessageBox::Cancel) && box->text().contains("Unknown") && box->text().contains("interrupt"); box->button(QMessageBox::Cancel)->click(); }
        });
        stop->click(); QCOMPARE(platform.actions,0); QVERIFY(*warning);
        auto accept=[&window](const QString& text) {
            QTimer::singleShot(0,&window,[text] {
                auto* box=qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
                if(box) for(auto* button : box->buttons()) if(button->text()==text) { button->click(); break; }
            });
        };
        accept("Stop now"); stop->click(); QCOMPARE(platform.actions,1);
        QCOMPARE(monitor.runners()[0].service.state,ServiceState::Stopped); QVERIFY(start->isEnabled()); QVERIFY(!stop->isEnabled());
        accept("Start service"); start->click(); QCOMPARE(platform.actions,2);
        QCOMPARE(monitor.runners()[0].service.state,ServiceState::Running); QCOMPARE(remote.calls,0);
    }
    void closeKeepsMenuBarAndReopenRestoresDock() {
        QSettings().setValue("hideDockWhenClosed",true);
        TestPlatform platform; TestRemote remote; Monitor monitor(platform,remote); TestPresentation presence;
        Window window(monitor,platform,nullptr,&presence); window.show();
        auto* tray=window.findChild<QSystemTrayIcon*>(); QVERIFY(tray); tray->show();
        window.close(); QVERIFY(!window.isVisible()); QVERIFY(tray->isVisible());
        QVERIFY(!presence.requests.isEmpty()); QVERIFY(!presence.dockVisible);
        bool opened=false;
        for(auto* action : tray->contextMenu()->actions()) if(action->text()=="Open Runperch") { action->trigger(); opened=true; break; }
        QVERIFY(opened); QVERIFY(window.isVisible()); QVERIFY(presence.dockVisible);
    }
    void noTrayDoesNotLeaveUnreachableBackgroundWindow() {
        TestPlatform platform; TestRemote remote; Monitor monitor(platform,remote); TestPresentation presence;
        presence.available=false;
        Window window(monitor,platform,nullptr,&presence); window.show();
        window.findChild<QSystemTrayIcon*>()->show();
        QVERIFY(window.close()); QVERIFY(!presence.requests.contains(false));
        QVERIFY(qApp->quitOnLastWindowClosed()); qApp->setQuitOnLastWindowClosed(false);
    }
    void dockPreferenceAndNativeFailureAreRespected() {
        QSettings().setValue("hideDockWhenClosed",false);
        TestPlatform platform; TestRemote remote; Monitor monitor(platform,remote); TestPresentation presence;
        Window window(monitor,platform,nullptr,&presence); window.show(); window.findChild<QSystemTrayIcon*>()->show();
        window.close(); QVERIFY(!presence.requests.contains(false));
        QSettings().setValue("hideDockWhenClosed",true); presence.succeeds=false;
        window.show(); window.close(); QVERIFY(presence.requests.contains(false)); QVERIFY(presence.dockVisible);
        const auto count=presence.requests.count(false);
        window.show(); window.close(); QCOMPARE(presence.requests.count(false),count+1);
    }
    void applicationIconRemainsVisibleAgainstDarkDock() {
        TestPlatform platform; TestRemote remote; Monitor monitor(platform,remote);
        Window window(monitor,platform);
        const auto image=window.windowIcon().pixmap(32,32).toImage().scaled(32,32);
        QVERIFY(!image.isNull());
        // The real window icon must carry its own light backing at Dock size.
        for(const QPoint p : {QPoint(16,4),QPoint(4,16),QPoint(27,16),QPoint(16,27)}) {
            const auto c=image.pixelColor(p);
            QVERIFY2(c.alpha()>240 && c.lightness()>210,"Dock icon has no visible light backing");
        }
        const auto mark=image.pixelColor(11,18);
        QVERIFY(mark.alpha()>240 && mark.lightness()<100);
    }
    void stalePresentationChangesWithoutNetworkRefresh() {
        QTemporaryDir runner;
        QFile file(runner.filePath(".runner")); QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("{\"agentId\":42,\"agentName\":\"ui-runner\",\"gitHubUrl\":\"https://github.com/example/project\"}"); file.close();
        TestPlatform platform; TestRemote remote;
        platform.value.state=ServiceState::Running;
        remote.value.connection=ConnectionState::Online; remote.value.job=JobState::Idle;
        Monitor monitor(platform,remote); QVERIFY(monitor.add(runner.path()));
        Window window(monitor,platform); window.show();
        platform.value.observedAt=QDateTime::currentDateTimeUtc().addSecs(-30);
        remote.value.observedAt=platform.value.observedAt; monitor.refresh();
        auto* table=window.findChild<QTableWidget*>("runnerTable"); QVERIFY(table);
        QCOMPARE(table->item(0,1)->text(),"Idle");
        QTRY_COMPARE_WITH_TIMEOUT(table->item(0,1)->text(),QString("Unknown"),3500);
        auto* states=window.findChild<QLabel*>("runnerStates"); QVERIFY(states);
        QVERIFY(states->text().contains("Service: Unknown")); QVERIFY(states->text().contains("Runner availability: Unknown"));
    }
};
int main(int argc,char** argv) {
    QApplication app(argc,argv); app.setQuitOnLastWindowClosed(false);
    app.setOrganizationName("RunperchTests"); app.setApplicationName("UI");
    QTemporaryDir config; QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,config.path());
    UiTests test; return QTest::qExec(&test,argc,argv);
}
#include "test_ui.moc"
