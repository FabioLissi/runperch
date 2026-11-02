#include "core/auth.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcessEnvironment>
#include <QRegularExpression>

namespace runperch {
QString extractDeviceCode(const QByteArray& input) {
    QString text=QString::fromUtf8(input);
    text.remove(QRegularExpression("\\x1b\\[[0-9;]*[A-Za-z]"));
    const QRegularExpression pattern("one-time code(?:\\s*\\(|[:\\s]+)([A-Z0-9]{4}-[A-Z0-9]{4})",QRegularExpression::CaseInsensitiveOption);
    return pattern.match(text).captured(1).toUpper();
}
GitHubAuth::GitHubAuth(CommandExecutor& e,QString path,QObject* parent)
    : QObject(parent),executor(e),executable(std::move(path)) {}
GitHubAuth::~GitHubAuth() {
    if(process) { process->disconnect(); process->kill(); process->waitForFinished(200); }
}
void GitHubAuth::check() {
    if(checking || busy()) return;
    if(executable.isEmpty()) { message="GitHub CLI is not installed. Install gh to connect."; emit changed(); return; }
    checking=true;
    const auto ticket=++generation;
    QPointer<GitHubAuth> self(this);
    executor.run(executable,{"api","--hostname","github.com","--method","GET","user"},[self,ticket](CommandResult result) {
        if(!self || self->generation!=ticket) return;
        self->checking=false;
        if(self->busy()) return;
        const auto o=QJsonDocument::fromJson(result.output).object();
        const QString name=o.value("login").toString();
        const QRegularExpression valid("^[A-Za-z0-9-]{1,39}$");
        if(result.ok && valid.match(name).hasMatch()) {
            self->account=name; self->message="Signed in as " + name + ". Repository permissions are checked separately.";
            if(self->verifyAfterLogin) emit self->connected();
        } else { self->account.clear(); self->message="GitHub account could not be verified. Connect or retry the account check."; }
        self->verifyAfterLogin=false; emit self->changed();
    });
}
void GitHubAuth::begin() {
    if(busy()) return;
    if(executable.isEmpty()) { check(); return; }
    auto env=QProcessEnvironment::systemEnvironment();
    if(env.contains("GH_TOKEN") || env.contains("GITHUB_TOKEN")) {
        message="An environment token controls GitHub access. Resolve that override before browser sign-in."; emit changed(); return;
    }
    ++generation; checking=false; verifyAfterLogin=false;
    cancelled=false; code.clear(); transcript.clear();
    message="Requesting a temporary GitHub device code…";
    auto* p=new QProcess(this); process=p;
    env.insert("GH_PROMPT_DISABLED","1"); env.insert("NO_COLOR","1"); env.insert("LC_ALL","C"); env.remove("GH_DEBUG");
    p->setProcessEnvironment(env); p->setStandardInputFile(QProcess::nullDevice());
    p->setProcessChannelMode(QProcess::MergedChannels);
    auto* deadline=new QTimer(p); deadline->setSingleShot(true);
    connect(deadline,&QTimer::timeout,p,[this,p] { cancelled=true; message="Authorization timed out. Try again when ready."; p->kill(); });
    connect(p,&QProcess::readyReadStandardOutput,p,[this,p] {
        transcript.append(p->readAllStandardOutput());
        if(cancelled) { transcript.clear(); return; }
        if(transcript.size()>65536) { cancelled=true; message="Unexpected authorization output; sign-in stopped."; p->kill(); return; }
        const auto found=extractDeviceCode(transcript);
        if(!found.isEmpty() && found!=code) { code=found; message="Copy the code, open GitHub and approve in your browser."; emit changed(); }
    });
    connect(p,&QProcess::errorOccurred,p,[this,p](QProcess::ProcessError e) {
        if(e==QProcess::FailedToStart) {
            message="GitHub CLI could not start."; code.clear(); transcript.clear(); process=nullptr; p->deleteLater(); emit changed();
        }
    });
    connect(p,&QProcess::finished,p,[this,p,deadline](int exit,QProcess::ExitStatus status) {
        deadline->stop();
        const bool success=!cancelled && exit==0 && status==QProcess::NormalExit;
        process=nullptr; code.clear(); transcript.clear(); p->deleteLater();
        if(success) {
            message="Authorization completed. Verifying the active account…"; verifyAfterLogin=true; check();
        } else if(!cancelled) message="GitHub authorization did not complete. Check browser approval and try again.";
        emit changed();
    });
    // Official noninteractive browser/device flow. Preserve Git protocol/helper settings and SSH keys.
    p->start(executable,{"auth","login","--web","--hostname","github.com","--skip-ssh-key","--clipboard=false"});
    deadline->start(600000); emit changed();
}
void GitHubAuth::cancel() {
    if(!busy()) return;
    cancelled=true; code.clear(); message="Sign-in stopped. Any completed authorization remains managed by gh.";
    process->kill(); emit changed();
}
}
