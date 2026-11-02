#include "ui/window.h"
#include <QApplication>
#include <QFile>
#include <QLabel>
#include <QSettings>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTest>
using namespace runperch;
namespace {
class TestPlatform final : public Platform {
public:
    ServiceObservation value;
    QString description() const override { return "Test computer"; }
    QStringList suggestedRunnerDirectories() const override { return {}; }
    QString githubExecutable() const override { return {}; }
    void inspect(const Registration&,ServiceCallback cb) override { cb(value); }
    void act(const Registration&,ServiceAction,ActionCallback cb) override { cb(false,"No service changes in UI tests"); }
};
class TestRemote final : public RemoteProvider {
public:
    RemoteObservation value;
    void observe(const Registration&,RemoteCallback cb) override { cb(value); }
};
}
class UiTests : public QObject {
    Q_OBJECT
private slots:
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
