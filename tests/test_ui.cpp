#include "ui/window.h"
#include "ui/popover.h"
#include "ui/metrics_chart.h"
#include "ui/display.h"
#include "core/resource_history.h"
#include <QComboBox>
#include <QAbstractItemView>
#include <QDesktopServices>
#include <QSignalSpy>
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
class UrlCapture final : public QObject {
    Q_OBJECT
public:
    QList<QUrl> urls;
    ~UrlCapture() override { QDesktopServices::unsetUrlHandler("https"); }
public slots:
    void open(const QUrl& url) { urls<<url; }
};
class UiTests : public QObject {
    Q_OBJECT
private slots:
    void init() { QSettings().clear(); qApp->setQuitOnLastWindowClosed(false); }
    void popoverClampsToScreensAndChartsPreserveTimeUnitsAndGaps() {
        const QRect screen(-1440,-900,1440,860);
        const auto fitted=popoverGeometry(QSize(400,550),QRect(-5,-50,5,10),screen); QVERIFY(screen.contains(fitted));
        const auto small=popoverGeometry(QSize(400,550),QRect(-100,20,1,1),QRect(-300,-100,300,200));
        QVERIFY(QRect(-300,-100,300,200).contains(small));
        const auto now=QDateTime::currentDateTimeUtc();
        QList<ResourceSample> samples{{now.addSecs(-60),1,0.0,qint64(1073741824)},{now.addSecs(-45),1,4.0,qint64(2147483648)},
            {now.addSecs(-30),1,std::nullopt,std::nullopt},{now.addSecs(-15),1,2.0,qint64(1073741824)},{now,2,1.0,qint64(1073741824)}};
        auto cpu=resourcePlot(samples,ResourceMetric::Cpu,QRectF(0,0,180,100),now);
        QCOMPARE(cpu.maximum,4.0); QCOMPARE(cpu.samples,4); QCOMPARE(cpu.segments.size(),3);
        QCOMPARE(cpu.segments[0][0],QPointF(168,100)); QCOMPARE(cpu.segments[0][1],QPointF(171,0));
        auto memory=resourcePlot(samples,ResourceMetric::Memory,QRectF(0,0,180,100),now); QCOMPARE(memory.maximum,2.0);
        QCOMPARE(memory.segments[0][0].y(),50.0);
        samples={{now.addSecs(-60),1,1.0,1024},{now,1,1.0,1024}};
        QCOMPARE(resourcePlot(samples,ResourceMetric::Cpu,QRectF(0,0,180,100),now).segments.size(),2);
    }
    void popoverUsesMonitorHistoryAndRoutesConfirmedControls() {
        QTemporaryDir runner; QFile file(runner.filePath(".runner")); QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("{\"agentId\":42,\"agentName\":\"quick-runner\",\"gitHubUrl\":\"https://github.com/example/project\"}"); file.close();
        TestPlatform p; TestRemote g; p.value.state=ServiceState::Running; p.value.controlsAvailable=true;
        p.value.observedAt=QDateTime::currentDateTimeUtc(); p.value.cpuCores=0.75; p.value.rssBytes=1073741824; p.value.activeWorkers=1;
        Monitor monitor(p,g); QVERIFY(monitor.add(runner.path())); Window window(monitor,p); monitor.refresh(true);
        auto* popup=window.findChild<RunnerPopover*>(); QVERIFY(popup);
        QCOMPARE(monitor.runners()[0].resources.size(),1);
        QVERIFY(popup->findChild<QLabel*>("popoverCpu")->text().contains("0.75"));
        auto* stop=popup->findChild<QPushButton*>("popoverStop"); auto* start=popup->findChild<QPushButton*>("popoverStart");
        QVERIFY(stop->isEnabled()); QVERIFY(!start->isEnabled()); QVERIFY(!popup->findChild<QPushButton*>("popoverJob")->isEnabled());
        QTimer::singleShot(0,&window,[] { if(auto* box=qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) box->button(QMessageBox::Cancel)->click(); });
        stop->click(); QCOMPARE(p.actions,0);
        auto accept=[&window](QString text) { QTimer::singleShot(0,&window,[text] { if(auto* box=qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) for(auto* b:box->buttons()) if(b->text()==text) { b->click(); break; } }); };
        accept("Stop now"); stop->click(); QCOMPARE(p.actions,1); QVERIFY(start->isEnabled());
        accept("Start service"); start->click(); QCOMPARE(p.actions,2); QCOMPARE(g.calls,0);
        const auto sampleCount=monitor.runners()[0].resources.size(); popup->refresh(); QCOMPARE(monitor.runners()[0].resources.size(),sampleCount);
        p.value.observedAt=QDateTime::currentDateTimeUtc().addSecs(-31); monitor.refresh(true);
        QVERIFY(!stop->isEnabled()); QCOMPARE(popup->findChild<QLabel*>("popoverCpu")->text(),QString("CPU avg —"));
    }
    void popoverSelectionAndPendingConfirmationStayOnTheirAttachment() {
        QTemporaryDir first,second;
        auto registration=[](QString path,QString name) { QFile f(path+"/.runner"); if(!f.open(QIODevice::WriteOnly)) return false; f.write(("{\"agentId\":42,\"agentName\":\""+name+"\",\"gitHubUrl\":\"https://github.com/example/project\"}").toUtf8()); return true; };
        QVERIFY(registration(first.path(),"first")); QVERIFY(registration(second.path(),"second"));
        TestPlatform p; TestRemote g; p.value.state=ServiceState::Running; p.value.observedAt=QDateTime::currentDateTimeUtc(); p.value.controlsAvailable=true;
        Monitor monitor(p,g); QVERIFY(monitor.add(first.path())); QVERIFY(monitor.add(second.path())); Window window(monitor,p); monitor.refresh(true);
        auto* popup=window.findChild<RunnerPopover*>(); auto* selector=popup->findChild<QComboBox*>("popoverRunners");
        selector->setCurrentIndex(1); QCOMPARE(selector->currentText(),QString("second"));
        QSignalSpy resets(selector->model(),&QAbstractItemModel::rowsRemoved);
        popup->present(QRect(400,40,20,20)); selector->showPopup(); QVERIFY(selector->view()->isVisible());
        popup->refresh(); QCOMPARE(resets.count(),0); QVERIFY(selector->view()->isVisible()); selector->hidePopup(); popup->hide();
        auto accepted=std::make_shared<bool>(false);
        QTimer::singleShot(0,&window,[&monitor,&second,accepted] {
            monitor.remove(1); *accepted=monitor.add(second.path()); monitor.refresh(true);
            if(auto* box=qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) for(auto* b:box->buttons()) if(b->text()=="Stop now") { b->click(); break; }
        });
        popup->findChild<QPushButton*>("popoverStop")->click(); QVERIFY(*accepted); QCOMPARE(p.actions,0);
        monitor.remove(0); monitor.remove(0); QCOMPARE(selector->count(),0); QVERIFY(!popup->findChild<QPushButton*>("popoverStop")->isEnabled());
    }
    void samplePopoverCanRenderProductionHistory() {
        QTemporaryDir runner; QFile f(runner.filePath(".runner")); QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("{\"agentId\":42,\"agentName\":\"studio-mac-mini\",\"gitHubUrl\":\"https://github.com/example/project\"}"); f.close();
        TestPlatform p; TestRemote g; p.value.state=ServiceState::Running; p.value.controlsAvailable=true; p.value.pid=42; p.value.activeWorkers=1; p.value.uptimeSeconds=27210;
        Monitor monitor(p,g); QVERIFY(monitor.add(runner.path())); const auto now=QDateTime::currentDateTimeUtc();
        for(int i=0;i<20;++i) {
            p.value.observedAt=now.addSecs(-285+15*i); p.value.cpuCores=1.5+(i%7)*0.38; p.value.rssBytes=qint64((1.2+(i%5)*0.09)*1073741824);
            // Historical samples pass through the same recorder used by Monitor.
            auto& fixture=const_cast<Runner&>(monitor.runners()[0]); recordResources(fixture,p.value,p.value.observedAt);
        }
        p.value.observedAt=now; g.value.observedAt=now; g.value.job=JobState::Running; g.value.jobId=1; g.value.jobName="Build and test macOS"; g.value.stepName="Running desktop tests"; g.value.startedAt=now.addSecs(-173); g.value.jobUrl="https://github.com/example/project/actions/runs/1/job/1";
        monitor.refresh(); auto presence=createApplicationPresentation(); Window window(monitor,p,nullptr,presence.get()); auto* popup=window.findChild<RunnerPopover*>();
        popup->present(QRect(400,40,20,20)); QTest::qWait(50);
        QVERIFY(!popup->grab().isNull()); QCOMPARE(monitor.runners()[0].resources.size(),20);
        const auto output=qEnvironmentVariable("RUNPERCH_CAPTURE_POPUP"); if(!output.isEmpty()) QVERIFY(popup->grab().save(output));
        QTest::keyClick(popup,Qt::Key_Escape); QVERIFY(!popup->isVisible());
    }
    void popoverCurrentJobRechecksFreshnessAndIdentity() {
        QTemporaryDir runner; QFile file(runner.filePath(".runner")); QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("{\"agentId\":42,\"agentName\":\"job-runner\",\"gitHubUrl\":\"https://github.com/example/project\"}"); file.close();
        TestPlatform p; TestRemote g; Monitor monitor(p,g); QVERIFY(monitor.add(runner.path())); Window window(monitor,p);
        g.value.job=JobState::Running; g.value.jobId=12; g.value.jobName="Build"; g.value.observedAt=QDateTime::currentDateTimeUtc();
        g.value.jobUrl="https://github.com/example/project/actions/runs/11/job/12"; monitor.refresh();
        auto* popup=window.findChild<RunnerPopover*>(); auto* job=popup->findChild<QPushButton*>("popoverJob"); QVERIFY(job->isEnabled());
        UrlCapture capture; QDesktopServices::setUrlHandler("https",&capture,"open"); job->click(); QCOMPARE(capture.urls.size(),1);
        const auto root=monitor.runners()[0].registration.root; const auto oldId=monitor.runners()[0].instanceId;
        g.value.observedAt=g.value.observedAt.addSecs(-31); monitor.refresh(); QVERIFY(!job->isEnabled());
        popup->currentJobRequested(root,oldId); QCOMPARE(capture.urls.size(),1);
        monitor.remove(0); QVERIFY(monitor.add(runner.path())); g.value.observedAt=QDateTime::currentDateTimeUtc(); monitor.refresh();
        popup->currentJobRequested(root,oldId); QCOMPARE(capture.urls.size(),1);
        for(const QString& url:{"https://github.com/example/project/actions/runs/11/job/99", "https://github.com/example/project/actions/runs/../../elsewhere", "https://example.com/actions/runs/11"}) {
            g.value.jobUrl=url; monitor.refresh(); QVERIFY(!job->isEnabled());
        }
    }
    void trayActivationOffersCompactPopover() {
        TestPlatform platform; TestRemote remote; Monitor monitor(platform,remote); TestPresentation presence;
        Window window(monitor,platform,nullptr,&presence); window.show();
        auto* tray=window.findChild<QSystemTrayIcon*>(); QVERIFY(tray);
        tray->activated(QSystemTrayIcon::Trigger);
        auto* popup=window.findChild<QWidget*>("runnerPopover"); QVERIFY(popup);
        QVERIFY(popup->isVisible()); QVERIFY(popup->width()<500);
        tray->activated(QSystemTrayIcon::Trigger); QVERIFY(!popup->isVisible());
    }
    void localStatsAndWorkerActivityNeedNoGitHub() {
        QTemporaryDir runner; QFile file(runner.filePath(".runner")); QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("{\"agentId\":42,\"agentName\":\"local-stats\",\"gitHubUrl\":\"https://github.com/example/project\"}"); file.close();
        TestPlatform platform; TestRemote remote; platform.value.state=ServiceState::Running;
        platform.value.observedAt=QDateTime::currentDateTimeUtc(); platform.value.uptimeSeconds=195330;
        platform.value.activeWorkers=1; platform.value.cpuCores=0.75; platform.value.rssBytes=1048576;
        Monitor monitor(platform,remote); QVERIFY(monitor.add(runner.path())); Window window(monitor,platform); monitor.refresh(true);
        auto* states=window.findChild<QLabel*>("runnerStates"); QVERIFY(states->text().contains("Local worker active"));
        auto* metrics=window.findChild<QLabel*>("metrics"); QVERIFY(metrics->text().contains("Service uptime: 2d 6h"));
        auto* table=window.findChild<QTableWidget*>("runnerTable"); QCOMPARE(table->item(0,1)->text(),"Running");
        platform.value.activeWorkers=0; monitor.refresh(true);
        QVERIFY(states->text().contains("No active worker observed")); QCOMPARE(table->item(0,1)->text(),"Service running");
        QCOMPARE(remote.calls,0);
    }
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
        for(auto* action : window.findChild<QMenu*>("trayActions")->actions()) if(action->text()=="Open Runperch") { action->trigger(); opened=true; break; }
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
