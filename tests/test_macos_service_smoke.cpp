#include "core/command.h"
#include "core/monitor.h"
#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QProcess>
#include <QTemporaryDir>
#include <QTextStream>
#include <QThread>
#include <QUuid>
#include <QXmlStreamWriter>
#include <unistd.h>
using namespace runperch;
namespace {
class NoNetwork final : public RemoteProvider {
public:
    int calls=0;
    void observe(const Registration&,RemoteCallback cb) override { ++calls; cb({}); }
};
bool writeExclusive(const QString& path,const QByteArray& data) {
    QFile file(path); return file.open(QIODevice::WriteOnly|QIODevice::NewOnly) && file.write(data)==data.size();
}
int command(const QStringList& arguments) {
    QProcess process; process.start("/bin/launchctl",arguments);
    if(!process.waitForFinished(5000)) { process.kill(); process.waitForFinished(1000); return -1; }
    return process.exitStatus()==QProcess::NormalExit ? process.exitCode() : -1;
}
}
int main(int argc,char** argv) {
    QCoreApplication app(argc,argv);
    if(app.arguments()!=QStringList{app.arguments()[0],"--run-disposable-service-test"} || getuid()==0) {
        QTextStream(stderr)<<"Opt-in test only: --run-disposable-service-test, as a logged-in non-root user.\n"; return 2;
    }
    const QString agents=QDir::homePath()+"/Library/LaunchAgents";
    if(!QFileInfo(agents).isDir()) { QTextStream(stderr)<<"Existing user LaunchAgents directory required.\n"; return 2; }
    QTemporaryDir temporary(QDir::tempPath()+"/runperch-service-smoke-XXXXXX");
    if(!temporary.isValid()) return 2;
    const QString root=QFileInfo(temporary.path()).canonicalFilePath();
    const QString label="actions.runner.Runperch-Smoke."+QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QString plist=agents+"/"+label+".plist", domain=QString("gui/%1").arg(getuid()), target=domain+"/"+label;
    if(!writeExclusive(root+"/.runner","{\"agentId\":1,\"agentName\":\"runperch-disposable-test\",\"gitHubUrl\":\"https://github.com/example/project\"}")
       || !writeExclusive(root+"/.service",label.toUtf8())
       || !writeExclusive(root+"/runsvc.sh","#!/bin/sh\nexec /bin/sleep 120\n")
       || !QFile::setPermissions(root+"/runsvc.sh",QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner)) return 2;
    QByteArray bytes; QXmlStreamWriter xml(&bytes); xml.writeStartDocument(); xml.writeStartElement("plist"); xml.writeAttribute("version","1.0"); xml.writeStartElement("dict");
    auto pair=[&](const QString& key,const QString& value) { xml.writeTextElement("key",key); xml.writeTextElement("string",value); };
    pair("Label",label); pair("WorkingDirectory",root);
    xml.writeTextElement("key","ProgramArguments"); xml.writeStartElement("array"); xml.writeTextElement("string",root+"/runsvc.sh"); xml.writeEndElement();
    xml.writeTextElement("key","RunAtLoad"); xml.writeEmptyElement("true"); xml.writeEndElement(); xml.writeEndElement(); xml.writeEndDocument();
    if(!writeExclusive(plist,bytes)) return 2;
    // Until the final cleanup is proven, retain the target referenced by our plist.
    temporary.setAutoRemove(false);
    ProcessExecutor executor; auto platform=createPlatform(executor); NoNetwork network; Monitor monitor(*platform,network);
    QStringList errors;
    QObject::connect(&monitor,&Monitor::message,&app,[&](QString text,bool error) { if(error) errors<<text; });
    auto waitFor=[&](const std::function<bool()>& ready) {
        QElapsedTimer deadline; deadline.start();
        while(deadline.elapsed()<10000) {
            app.processEvents(QEventLoop::AllEvents,50);
            if(ready()) return true;
            QThread::msleep(10);
        }
        return false;
    };
    bool allSettled=true;
    auto settled=[&] {
        const bool done=waitFor([&] { return !monitor.runners().isEmpty() && !monitor.runners()[0].refreshing && !monitor.runners()[0].changingService; });
        allSettled &= done; return done;
    };
    bool initial=false,started=false,stopped=false;
    QString error;
    if(monitor.add(root,&error)) {
        monitor.refresh(true); initial=settled() && monitor.runners()[0].service.state==ServiceState::Stopped;
        if(initial) {
            monitor.control(0,ServiceAction::Start); started=settled() && errors.isEmpty()
                && monitor.runners()[0].service.state==ServiceState::Running && monitor.runners()[0].service.pid>0;
            if(started) {
                monitor.control(0,ServiceAction::Stop,StopAuthorization::ConfirmedImmediate);
                stopped=settled() && errors.isEmpty() && monitor.runners()[0].service.state==ServiceState::Stopped;
            }
        }
    } else errors<<error;
    executor.cancel();
    QFile ownPlist(plist); const bool unchanged=!QFileInfo(plist).isSymLink() && ownPlist.open(QIODevice::ReadOnly) && ownPlist.readAll()==bytes;
    ownPlist.close();
    const int beforeCleanup=command({"print",target});
    if(beforeCleanup==0 && unchanged && allSettled && !monitor.runners().isEmpty()) {
        // Reuse production identity checks for cleanup too; never raw-bootout an unverified service.
        bool complete=false;
        platform->act(monitor.runners()[0].registration,ServiceAction::Stop,[&](bool ok,QString text) {
            if(!ok) errors<<text; complete=true;
        });
        allSettled &= waitFor([&] { return complete; });
        executor.cancel();
    }
    const bool unloaded=command({"print",target})==113;
    // Recheck bytes after any async inspection before removing only our own unchanged file.
    QFile finalPlist(plist);
    const bool finalUnchanged=!QFileInfo(plist).isSymLink() && finalPlist.open(QIODevice::ReadOnly) && finalPlist.readAll()==bytes;
    finalPlist.close();
    const bool removed=unloaded && unchanged && finalUnchanged && allSettled && QFile::remove(plist);
    if(removed) temporary.setAutoRemove(true);
    QJsonObject result{{"initial_stopped",initial},{"start_observed_running",started},{"stop_observed_stopped",stopped},
        {"github_requests",network.calls},{"cleanup_complete",removed},{"errors",errors.join("; ")}};
    if(!removed) { result["retained_plist"]=plist; result["retained_fixture"]=root; }
    QTextStream(stdout)<<QJsonDocument(result).toJson();
    return initial && started && stopped && network.calls==0 && removed ? 0 : 1;
}
