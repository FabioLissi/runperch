#include "core/command.h"
#include "core/github.h"
#include "core/monitor.h"
#include "core/registration.h"
#include "core/auth.h"
#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QTextStream>
#include <QTimer>
#include <filesystem>
#ifdef RUNPERCH_MACOS_TESTS
#include "platform/macos/launchd_parser.h"
#endif

using namespace runperch;
namespace {
QByteArray registrationData() {
    return "{\"agentId\":42,\"agentName\":\"test-runner\",\"gitHubUrl\":\"https://github.com/example/project\"}";
}
Registration registration() { return *parseRegistration(registrationData(),"/unused").value; }
Runner ready() {
    Runner r; r.registration=registration();
    r.service.state=ServiceState::Running; r.service.controlsAvailable=true;
    r.service.observedAt=QDateTime::currentDateTimeUtc();
    r.remote.connection=ConnectionState::Online; r.remote.job=JobState::Idle;
    r.remote.observedAt=r.service.observedAt; return r;
}
class FakeExecutor final : public CommandExecutor {
public:
    QList<CommandResult> results;
    QList<QStringList> calls;
    bool deferred=false;
    QList<CommandCallback> callbacks;
    void run(const QString& program,const QStringList& args,CommandCallback cb,int,qsizetype) override {
        calls << (QStringList{program}+args);
        if(deferred) { callbacks << cb; return; }
        auto result=results.isEmpty() ? CommandResult{} : results.takeFirst(); cb(result);
    }
};
class FakePlatform final : public Platform {
public:
    ServiceObservation observation;
    QList<ServiceCallback> callbacks;
    int actions=0;
    bool deferred=false;
    bool reflectActions=false;
    bool rejectActions=false;
    QString description() const override { return "Test"; }
    QStringList suggestedRunnerDirectories() const override { return {}; }
    QString githubExecutable() const override { return {}; }
    void inspect(const Registration&,ServiceCallback cb) override { if(deferred) callbacks << cb; else cb(observation); }
    void act(const Registration&,ServiceAction action,ActionCallback cb) override {
        ++actions;
        if(rejectActions) { cb(false,"Test service request rejected"); return; }
        if(reflectActions) { observation.state=action==ServiceAction::Start ? ServiceState::Running : ServiceState::Stopped; observation.observedAt=QDateTime::currentDateTimeUtc(); }
        cb(true,"accepted");
    }
};
class FakeRemote final : public RemoteProvider {
public:
    RemoteObservation observation;
    int calls=0;
    bool deferred=false;
    QList<RemoteCallback> callbacks;
    void observe(const Registration&,RemoteCallback cb) override { ++calls; if(deferred) callbacks<<cb; else cb(observation); }
};
void writeFile(const QString& path,const QByteArray& data) { QFile f(path); QVERIFY(f.open(QIODevice::WriteOnly)); QCOMPARE(f.write(data),data.size()); }
}
class CoreTests : public QObject {
    Q_OBJECT
private slots:
#ifdef RUNPERCH_MACOS_TESTS
    void loadedServiceRequiresIdentity() {
        // Field layout captured from a real user LaunchAgent via launchctl print.
        QByteArray output="gui/501/actions.runner.test = {\n\tpath = /agents/test.plist\n\tstate = running\n\tprogram = /runner/runsvc.sh\n\tworking directory = /runner\n\tpid = 123\n}\n";
        auto got=parseLaunchdStatus(output,"/agents/test.plist","/runner");
        QVERIFY(got.controlsAvailable); QCOMPARE(got.pid,123);
        auto wrong=output; wrong.replace("/agents/test.plist","/agents/other.plist");
        QVERIFY(!parseLaunchdStatus(wrong,"/agents/test.plist","/runner").controlsAvailable);
        wrong=output; wrong.replace("/runner/runsvc.sh","/other/runsvc.sh");
        QVERIFY(!parseLaunchdStatus(wrong,"/agents/test.plist","/runner").controlsAvailable);
        wrong=output; wrong.replace("working directory = /runner","working directory = /other");
        QVERIFY(!parseLaunchdStatus(wrong,"/agents/test.plist","/runner").controlsAvailable);
        QVERIFY(!parseLaunchdStatus("pid = 123\n","/agents/test.plist","/runner").controlsAvailable);
    }
#endif
    void acceptsBomAndProducerFields() {
        auto parsed=parseRegistration(QByteArray::fromHex("efbbbf")+registrationData(),"/installation");
        QVERIFY(parsed.value); QCOMPARE(parsed.value->name,"test-runner"); QCOMPARE(parsed.value->runnerId,42);
        QCOMPARE(parsed.value->owner,"example"); QCOMPARE(parsed.value->repository,"project");
    }
    void invalidRegistration_data() {
        QTest::addColumn<QByteArray>("json");
        QTest::newRow("malformed") << QByteArray("{");
        QTest::newRow("array") << QByteArray("[]");
        QTest::newRow("huge") << QByteArray(65537,'x');
        QTest::newRow("missing-id") << QByteArray("{\"agentName\":\"runner\",\"gitHubUrl\":\"https://github.com/o/r\"}");
        QTest::newRow("fractional-id") << QByteArray("{\"agentId\":1.5,\"agentName\":\"r\",\"gitHubUrl\":\"https://github.com/o/r\"}");
        for(const auto& url : {"https://evil.invalid/o/r","http://github.com/o/r","https://github.com/o/r?token=x",
                              "https://user:password@github.com/o/r","https://github.com/o/r/extra","https://github.com/o/.."}) {
            QJsonObject o{{"agentId",42},{"agentName","runner"},{"gitHubUrl",url}};
            QTest::newRow(url) << QJsonDocument(o).toJson();
        }
    }
    void invalidRegistration() { QFETCH(QByteArray,json); QVERIFY(!parseRegistration(json,"/unused").value); }
    void actualMetadataReadAndDeduplication() {
        QTemporaryDir dir; writeFile(dir.filePath(".runner"),registrationData());
        FakePlatform p; FakeRemote g; Monitor m(p,g); QString error;
        QVERIFY(m.add(dir.path(),&error)); QVERIFY(m.add(dir.path()+"/.",&error)); QCOMPARE(m.runners().size(),1);
        QVERIFY(!m.add(dir.filePath("missing"),&error));
    }
    void rejectsSymlinkedMetadata() {
        QTemporaryDir dir;
        writeFile(dir.filePath("target"),registrationData());
        std::error_code error;
        auto link=[&](const QString& name) {
            std::filesystem::create_symlink("target",std::filesystem::path(dir.filePath(name).toStdU16String()),error);
        };
        link(".runner");
        if(error) QSKIP("This host cannot create native filesystem symlinks; metadata symlink rejection requires a capable host.");
        QVERIFY(QFileInfo(dir.filePath(".runner")).isSymLink());
        QVERIFY(!readRegistration(dir.path()).value);
        QFile::remove(dir.filePath(".runner"));
        writeFile(dir.filePath(".runner"),registrationData());
        link(".service"); QVERIFY(!error); QVERIFY(QFileInfo(dir.filePath(".service")).isSymLink());
        QVERIFY(!readRegistration(dir.path()).value);
    }
    void localServiceDoesNotImplyOnlineOrIdle() {
        auto r=ready(); r.remote={}; QCOMPARE(status(r),"Service running");
        QVERIFY(!canControl(r,ServiceAction::Stop,QDateTime::currentDateTimeUtc()).allowed);
    }
    void stopPolicy_data() {
        QTest::addColumn<int>("variant"); QTest::addColumn<bool>("allowed");
        QTest::newRow("fresh-idle") << 0 << true;
        QTest::newRow("busy") << 1 << false;
        QTest::newRow("unknown-job") << 2 << false;
        QTest::newRow("offline") << 3 << false;
        QTest::newRow("stale-remote") << 4 << false;
        QTest::newRow("stale-local") << 5 << false;
        QTest::newRow("refresh-in-flight") << 6 << false;
        QTest::newRow("no-control") << 7 << false;
        QTest::newRow("future-timestamp") << 8 << false;
    }
    void stopPolicy() {
        QFETCH(int,variant); QFETCH(bool,allowed); auto r=ready(); auto now=QDateTime::currentDateTimeUtc();
        if(variant==1) r.remote.job=JobState::Running;
        if(variant==2) r.remote.job=JobState::Unknown;
        if(variant==3) r.remote.connection=ConnectionState::Offline;
        if(variant==4) r.remote.observedAt=now.addSecs(-31);
        if(variant==5) r.service.observedAt=now.addSecs(-31);
        if(variant==6) r.refreshing=true;
        if(variant==7) r.service.controlsAvailable=false;
        if(variant==8) r.remote.observedAt=now.addSecs(2);
        QCOMPARE(canControl(r,ServiceAction::Stop,now).allowed,allowed);
    }
    void startUsesConfirmedLocalStoppedState() {
        auto r=ready(); r.service.state=ServiceState::Stopped;
        QVERIFY(canControl(r,ServiceAction::Start,QDateTime::currentDateTimeUtc()).allowed);
        r.remote.job=JobState::Running; QVERIFY(canControl(r,ServiceAction::Start,QDateTime::currentDateTimeUtc()).allowed);
    }
    void confirmedStopUsesFreshLocalStateWithoutInventingIdle() {
        auto r=ready(); r.remote={};
        QVERIFY(canControl(r,ServiceAction::Stop,QDateTime::currentDateTimeUtc(),StopAuthorization::ConfirmedImmediate).allowed);
        QCOMPARE(r.remote.job,JobState::Unknown);
        QVERIFY(!canControl(r,ServiceAction::Stop,QDateTime::currentDateTimeUtc()).allowed);
        r.service.observedAt=r.service.observedAt.addSecs(-31);
        QVERIFY(!canControl(r,ServiceAction::Stop,QDateTime::currentDateTimeUtc(),StopAuthorization::ConfirmedImmediate).allowed);
    }
    void explicitStopDoesNotWaitForGitHubAndPublishesLocalState() {
        QTemporaryDir dir; writeFile(dir.filePath(".runner"),registrationData());
        FakePlatform p; p.observation=ready().service; p.reflectActions=true;
        FakeRemote g; g.deferred=true; Monitor monitor(p,g); QVERIFY(monitor.add(dir.path()));
        monitor.refresh(); QVERIFY(monitor.runners()[0].refreshing);
        monitor.control(0,ServiceAction::Stop,StopAuthorization::ConfirmedImmediate);
        QCOMPARE(p.actions,1); QCOMPARE(g.calls,1);
        QCOMPARE(monitor.runners()[0].service.state,ServiceState::Stopped);
        auto remote=ready().remote; remote.job=JobState::Running; g.callbacks.takeFirst()(remote);
        QCOMPARE(status(monitor.runners()[0]),"Stopped");
    }
    void startDoesNotWaitForGitHub() {
        QTemporaryDir dir; writeFile(dir.filePath(".runner"),registrationData());
        FakePlatform p; p.observation=ready().service; p.observation.state=ServiceState::Stopped; p.reflectActions=true;
        FakeRemote g; g.deferred=true; Monitor monitor(p,g); QVERIFY(monitor.add(dir.path())); monitor.refresh();
        monitor.control(0,ServiceAction::Start);
        QCOMPARE(p.actions,1); QCOMPARE(g.calls,1); QCOMPARE(monitor.runners()[0].service.state,ServiceState::Running);
        g.callbacks.takeFirst()({});
    }
    void explicitStopCannotBypassUnknownLocalStateOrIdentityChange() {
        auto r=ready(); r.service.state=ServiceState::Unknown;
        QVERIFY(!canControl(r,ServiceAction::Stop,QDateTime::currentDateTimeUtc(),StopAuthorization::ConfirmedImmediate).allowed);
        r.service.state=ServiceState::Running; r.service.controlsAvailable=false;
        QVERIFY(!canControl(r,ServiceAction::Stop,QDateTime::currentDateTimeUtc(),StopAuthorization::ConfirmedImmediate).allowed);
        QTemporaryDir dir; writeFile(dir.filePath(".runner"),registrationData());
        FakePlatform p; p.observation=ready().service; FakeRemote g; Monitor monitor(p,g);
        QVERIFY(monitor.add(dir.path())); monitor.refresh(true); p.deferred=true;
        monitor.control(0,ServiceAction::Stop,StopAuthorization::ConfirmedImmediate);
        QCOMPARE(p.callbacks.size(),1);
        auto changed=registrationData(); changed.replace("42","43"); writeFile(dir.filePath(".runner"),changed);
        p.callbacks.takeFirst()(p.observation); QCOMPARE(p.actions,0); QVERIFY(!monitor.runners()[0].changingService);
    }
    void postActionInspectionSurvivesAnOlderRemoteRefresh() {
        QTemporaryDir dir; writeFile(dir.filePath(".runner"),registrationData());
        FakePlatform p; p.observation=ready().service; p.reflectActions=true;
        FakeRemote g; g.deferred=true; Monitor monitor(p,g); QVERIFY(monitor.add(dir.path())); monitor.refresh();
        p.deferred=true; monitor.control(0,ServiceAction::Stop,StopAuthorization::ConfirmedImmediate);
        p.callbacks.takeFirst()(p.observation); QCOMPARE(p.actions,1); QCOMPARE(p.callbacks.size(),1);
        auto remote=ready().remote; remote.job=JobState::Running; g.callbacks.takeFirst()(remote);
        QVERIFY(monitor.runners()[0].changingService); QVERIFY(monitor.runners()[0].serviceRefreshing); QVERIFY(monitor.runners()[0].refreshing);
        p.callbacks.takeFirst()(p.observation);
        QCOMPARE(status(monitor.runners()[0]),"Stopped"); QVERIFY(!monitor.runners()[0].refreshing); QVERIFY(!monitor.runners()[0].changingService);
    }
    void rejectedServiceRequestDoesNotClaimStopped() {
        QTemporaryDir dir; writeFile(dir.filePath(".runner"),registrationData());
        FakePlatform p; p.observation=ready().service; p.rejectActions=true;
        FakeRemote g; Monitor monitor(p,g); QVERIFY(monitor.add(dir.path())); monitor.refresh(true);
        QSignalSpy messages(&monitor,&Monitor::message);
        monitor.control(0,ServiceAction::Stop,StopAuthorization::ConfirmedImmediate);
        QCOMPARE(p.actions,1); QCOMPARE(messages.size(),1); QVERIFY(messages[0][1].toBool());
        QCOMPARE(monitor.runners()[0].service.state,ServiceState::Running); QVERIFY(!monitor.runners()[0].changingService);
        QCOMPARE(g.calls,0);
    }
    void staleStatusIsNotIdle() {
        auto r=ready(); r.remote.observedAt=r.remote.observedAt.addSecs(-31);
        QCOMPARE(status(r),"Service running");
    }
    void parseActualApiSchemaDoesNotInventIdle() {
        auto out=parseRunnerResponse("{\"runners\":[{\"id\":42,\"status\":\"offline\",\"busy\":false}]}",42);
        QCOMPARE(out.connection,ConnectionState::Offline); QCOMPARE(out.job,JobState::Unknown);
        out=parseRunnerResponse("{\"runners\":[{\"id\":42,\"status\":\"online\",\"busy\":false}]}",42);
        QCOMPARE(out.job,JobState::Idle);
        out=parseRunnerResponse("{\"runners\":[{\"id\":43,\"status\":\"online\",\"busy\":false}]}",42);
        QCOMPARE(out.job,JobState::Unknown);
        out=parseRunnerResponse("{}",42); QCOMPARE(out.connection,ConnectionState::Unknown);
    }
    void jobRequiresRunnerIdentityAndSafeUrl() {
        const auto r=registration();
        auto job=QJsonDocument::fromJson("{\"id\":17,\"runner_id\":42,\"runner_name\":\"test-runner\",\"status\":\"in_progress\",\n            \"html_url\":\"https://github.com/example/project/actions/runs/12/job/17\",\"name\":\"Build\",\n            \"started_at\":\"2026-11-01T10:00:00Z\",\"steps\":[{\"name\":\"Tests\",\"status\":\"in_progress\"}]}").object();
        RemoteObservation out; QVERIFY(applyJobResponse(job,r,out)); QCOMPARE(out.stepName,"Tests");
        QCOMPARE(out.job,JobState::Running); QCOMPARE(out.connection,ConnectionState::Unknown);
        job["runner_id"]=43; QVERIFY(!applyJobResponse(job,r,out));
        job["runner_id"]=42; job["html_url"]="https://evil.invalid/"; QVERIFY(!applyJobResponse(job,r,out));
    }
    void permissionFailureStaysUnknown() {
        FakeExecutor e; e.results={CommandResult{},CommandResult{true,0,"{\"permissions\":{\"admin\":false}}"},CommandResult{true,0,"{\"workflow_runs\":[]}"},CommandResult{true,0,"{\"workflow_runs\":[]}"}};
        GitHubProvider provider(e,"fake-gh"); bool called=false;
        provider.setRunnerStatusAccessEnabled(true);
        provider.observe(registration(),[&](RemoteObservation out) { called=true; QCOMPARE(out.job,JobState::Unknown); QCOMPARE(out.connection,ConnectionState::Unknown); });
        QVERIFY(called); QCOMPARE(e.calls.size(),4);
        for(const auto& call : e.calls) { QVERIFY(call.contains("GET")); QVERIFY(call.contains("github.com")); QVERIFY(!call.contains("POST")); }
    }
    void normalMonitoringDoesNotRequestAdministration() {
        FakeExecutor e; e.results={CommandResult{true,0,"{\"workflow_runs\":[]}"},CommandResult{true,0,"{\"workflow_runs\":[]}"}};
        GitHubProvider provider(e,"fake-gh"); bool called=false;
        provider.observe(registration(),[&](RemoteObservation out) { called=true; QVERIFY(out.workflowsReadable); QVERIFY(!out.jobsReadable); QCOMPARE(out.job,JobState::Unknown); });
        QVERIFY(called); QCOMPARE(e.calls.size(),2);
        QVERIFY(e.calls[0].last().contains("actions/runs")); QVERIFY(!e.calls[0].last().contains("actions/runners"));
        QVERIFY(e.calls[0].last().contains("status=in_progress")); QVERIFY(e.calls[1].last().contains("status=completed"));
    }
    void completedJobIsEvidenceNotAnIdleInference() {
        auto job=QJsonDocument::fromJson("{\"id\":17,\"runner_id\":42,\"runner_name\":\"test-runner\",\"status\":\"completed\",\"conclusion\":\"failure\",\"name\":\"Tests\",\"html_url\":\"https://github.com/example/project/actions/runs/12/job/17\",\"completed_at\":\"2026-11-01T10:01:00Z\"}").object();
        RemoteObservation out; QVERIFY(applyCompletedJob(job,registration(),out)); QCOMPARE(out.lastConclusion,"failure");
        QCOMPARE(out.job,JobState::Unknown); QCOMPARE(out.connection,ConnectionState::Unknown);
        job["completed_at"]="2026-11-01T09:01:00Z"; QVERIFY(!applyCompletedJob(job,registration(),out));
        job["runner_id"]=43; QVERIFY(!applyCompletedJob(job,registration(),out));
    }
    void activeJobPublishesWithoutHistoryDelay() {
        FakeExecutor e;
        e.results={CommandResult{true,0,"{\"workflow_runs\":[{\"id\":12,\"name\":\"Active workflow\"},{\"id\":11,\"name\":\"Older workflow\"}]}"},
            CommandResult{true,0,"{\"jobs\":[{\"id\":17,\"runner_id\":42,\"runner_name\":\"test-runner\",\"status\":\"in_progress\",\"name\":\"Build\",\"html_url\":\"https://github.com/example/project/actions/runs/12/job/17\"}]}"}};
        GitHubProvider provider(e,"fake-gh"); bool called=false;
        provider.observe(registration(),[&](RemoteObservation out) {
            called=true; QCOMPARE(out.jobName,"Build"); QCOMPARE(out.workflowName,"Active workflow");
            QVERIFY(out.jobsReadable); QCOMPARE(out.job,JobState::Running);
        });
        QVERIFY(called); QCOMPARE(e.calls.size(),2);
    }
    void deviceCodeParserShowsNoOtherCliOutput() {
        QCOMPARE(extractDeviceCode("! First copy your one-time code: ABCD-1234\nOpen this URL to continue..."),"ABCD-1234");
        QCOMPARE(extractDeviceCode("! First copy your one-time code: \033[1mABCD-1234\033[0m\n"),"ABCD-1234");
        QCOMPARE(extractDeviceCode("! One-time code (ABCD-1234) copied to clipboard\n"),"ABCD-1234");
        QVERIFY(extractDeviceCode("token=ghp_not_a_device_code\n").isEmpty());
        QVERIFY(extractDeviceCode("server sent ABCD-1234 without a code prompt").isEmpty());
    }
    void authorizationUsesOfficialFlowAndVerifiesAccount() {
        FakeExecutor executor; executor.results={CommandResult{true,0,"{\"login\":\"octocat\"}"}};
        GitHubAuth auth(executor,QCoreApplication::applicationFilePath()); QSignalSpy connected(&auth,&GitHubAuth::connected);
        auth.begin(); QTRY_COMPARE(connected.count(),1); QCOMPARE(auth.login(),"octocat"); QVERIFY(auth.deviceCode().isEmpty());
        QCOMPARE(executor.calls.size(),1); QVERIFY(executor.calls[0].contains("user"));
    }
    void preLoginAccountResponseCannotVerifyNewLogin() {
        FakeExecutor executor; executor.deferred=true;
        GitHubAuth auth(executor,QCoreApplication::applicationFilePath());
        QSignalSpy connected(&auth,&GitHubAuth::connected);
        auth.check(); QCOMPARE(executor.callbacks.size(),1);
        auto old=executor.callbacks.takeFirst();
        auth.begin(); QTRY_COMPARE(executor.callbacks.size(),1);
        executor.callbacks.takeFirst()(CommandResult{true,0,"{\"login\":\"new-account\"}"});
        old(CommandResult{true,0,"{\"login\":\"old-account\"}"});
        QCOMPARE(auth.login(),"new-account"); QCOMPARE(connected.count(),1);
    }
    void cancelledAuthorizationNeverClaimsConnected() {
        FakeExecutor executor;
        GitHubAuth auth(executor,QCoreApplication::applicationFilePath());
        QSignalSpy connected(&auth,&GitHubAuth::connected);
        auth.begin(); auth.cancel(); QTRY_VERIFY(!auth.busy());
        QCOMPARE(connected.count(),0); QVERIFY(auth.deviceCode().isEmpty());
    }
    void overlapRefreshDoesNotDuplicateQueries() {
        QTemporaryDir dir; writeFile(dir.filePath(".runner"),registrationData());
        FakePlatform p; p.deferred=true; FakeRemote g; Monitor m(p,g); QVERIFY(m.add(dir.path()));
        QSignalSpy finished(&m,&Monitor::refreshFinished);
        m.refresh(); m.refresh(); QCOMPARE(p.callbacks.size(),1); QCOMPARE(g.calls,1);
        p.callbacks.takeFirst()({}); QCOMPARE(finished.count(),1); QVERIFY(!m.runners()[0].refreshing);
    }
    void changedRegistrationDisablesObservationAndControls() {
        QTemporaryDir dir; writeFile(dir.filePath(".runner"),registrationData());
        FakePlatform p; FakeRemote g; Monitor m(p,g); QVERIFY(m.add(dir.path()));
        writeFile(dir.filePath(".runner"),"{}"); m.refresh();
        QCOMPARE(g.calls,0); QCOMPARE(m.runners()[0].service.state,ServiceState::Unknown);
        QVERIFY(!m.runners()[0].service.controlsAvailable);
    }
    void serviceWriteRechecksBusyState() {
        QTemporaryDir dir; writeFile(dir.filePath(".runner"),registrationData());
        auto r=ready(); FakePlatform p; p.observation=r.service; FakeRemote g; g.observation=r.remote;
        Monitor m(p,g); QVERIFY(m.add(dir.path())); m.refresh();
        g.observation.job=JobState::Running; m.control(0,ServiceAction::Stop); QCOMPARE(p.actions,0);
    }
    void serviceWriteRejectsChangedRegistration() {
        QTemporaryDir dir; writeFile(dir.filePath(".runner"),registrationData());
        auto r=ready(); FakePlatform p; p.observation=r.service; FakeRemote g; g.observation=r.remote;
        Monitor m(p,g); QVERIFY(m.add(dir.path())); m.refresh();
        auto changed=registrationData(); changed.replace("42","43"); writeFile(dir.filePath(".runner"),changed);
        m.control(0,ServiceAction::Stop); QCOMPARE(p.actions,0);
    }
    void removedAndReaddedRunnerRejectsOldObservation() {
        QTemporaryDir dir; writeFile(dir.filePath(".runner"),registrationData());
        FakePlatform p; p.deferred=true; FakeRemote g; Monitor m(p,g); QVERIFY(m.add(dir.path())); m.refresh();
        m.remove(0); QVERIFY(m.add(dir.path()));
        auto observation=ready().service; p.callbacks.takeFirst()(observation);
        QCOMPARE(m.runners()[0].service.state,ServiceState::Unknown);
    }
    void failedCommand() {
        ProcessExecutor executor; bool called=false; CommandResult got;
        executor.run("runperch-nonexistent-program",{},[&](auto r) { called=true; got=r; });
        QTRY_VERIFY(called); QVERIFY(!got.ok);
    }
    void asynchronousRealProducerOutput() {
        ProcessExecutor executor; bool called=false; CommandResult got;
        executor.run(QCoreApplication::applicationFilePath(),{"--process-fixture=echo"},[&](auto r) { called=true; got=r; });
        QVERIFY(!called); QTRY_VERIFY(called); QVERIFY(got.ok);
#ifdef Q_OS_WIN
        QCOMPARE(got.output,QByteArray("runner producer output\r\n"));
#else
        QCOMPARE(got.output,QByteArray("runner producer output\n"));
#endif
        QVERIFY(!got.output.contains("private-stderr"));
    }
    void timeoutAndOutputLimits() {
        ProcessExecutor executor; bool called=false; CommandResult got;
        executor.run(QCoreApplication::applicationFilePath(),{"--process-fixture=wait"},[&](auto r) { called=true; got=r; },50,1024);
        QTRY_VERIFY(called); QVERIFY(got.timedOut); QVERIFY(!got.ok);
        called=false;
        executor.run(QCoreApplication::applicationFilePath(),{"--process-fixture=large"},[&](auto r) { called=true; got=r; },5000,128);
        QTRY_VERIFY(called); QVERIFY(got.outputLimited); QVERIFY(!got.ok); QVERIFY(got.output.size()<=128);
    }
    void shutdownDoesNotInvokeConsumers() {
        bool called=false;
        {
            ProcessExecutor executor;
            executor.run(QCoreApplication::applicationFilePath(),{"--process-fixture=wait"},[&](auto) { called=true; });
            QTest::qWait(30);
        }
        QVERIFY(!called);
    }
};
int main(int argc,char** argv) {
    QCoreApplication app(argc,argv);
    if(app.arguments().value(1)=="auth") {
        const QStringList expected{"auth","login","--web","--hostname","github.com","--skip-ssh-key","--clipboard=false"};
        if(app.arguments().mid(1)!=expected) return 3;
        QTextStream(stderr)<<"! First copy your one-time code: ABCD-1234\n";
        QTimer::singleShot(50,&app,&QCoreApplication::quit); return app.exec();
    }
    if(app.arguments().contains("--process-fixture=echo")) { QTextStream(stdout)<<"runner producer output\n"; QTextStream(stderr)<<"private-stderr\n"; return 0; }
    if(app.arguments().contains("--process-fixture=large")) { QTextStream(stdout)<<QString(10000,'x'); return 0; }
    if(app.arguments().contains("--process-fixture=wait")) { QTimer::singleShot(10000,&app,&QCoreApplication::quit); return app.exec(); }
    QTemporaryDir config; QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,config.path());
    app.setOrganizationName("RunperchTests"); app.setApplicationName("Core");
    CoreTests tests; return QTest::qExec(&tests,argc,argv);
}
#include "test_core.moc"
