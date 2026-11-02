#include "core/command.h"
#include <QProcess>
#include <QProcessEnvironment>
#include <QTimer>
#include <memory>

namespace runperch {
ProcessExecutor::~ProcessExecutor() { cancel(); }
void ProcessExecutor::cancel() {
    for(auto* p : findChildren<QProcess*>(QString(),Qt::FindDirectChildrenOnly)) {
        p->disconnect();
        for(auto* timer : p->findChildren<QTimer*>()) timer->stop();
        if(p->state()!=QProcess::NotRunning) { p->kill(); p->waitForFinished(200); }
    }
}
void ProcessExecutor::run(const QString& program, const QStringList& args,
                         CommandCallback cb, int timeout, qsizetype limit) {
    auto* p = new QProcess(this);
    auto* timer = new QTimer(p);
    timer->setSingleShot(true);
    auto result = std::make_shared<CommandResult>();
    auto done = std::make_shared<bool>(false);
    auto total = std::make_shared<qsizetype>(0);
    auto finish = [p, timer, result, done, cb] {
        if(*done) return;
        *done = true;
        timer->stop();
        p->deleteLater();
        cb(*result);
    };
    auto read = [p, result, total, limit] {
        const auto out = p->readAllStandardOutput();
        const auto err = p->readAllStandardError();
        *total += out.size() + err.size();
        // Never retain stderr: credentials or server diagnostics may contain secrets.
        result->output.append(out.left(qMax<qsizetype>(0, limit - result->output.size())));
        if(*total > limit) { result->outputLimited = true; p->kill(); }
    };
    connect(p, &QProcess::readyReadStandardOutput, p, read);
    connect(p, &QProcess::readyReadStandardError, p, read);
    connect(p, &QProcess::finished, p, [result, finish, read](int code, QProcess::ExitStatus exit) {
        read();
        result->exitCode = code;
        result->ok = exit == QProcess::NormalExit && code == 0 && !result->timedOut && !result->outputLimited;
        finish();
    });
    connect(p, &QProcess::errorOccurred, p, [result, finish](QProcess::ProcessError error) {
        if(error == QProcess::FailedToStart) { result->ok = false; finish(); }
    });
    connect(timer, &QTimer::timeout, p, [p, result] { result->timedOut = true; p->kill(); });
    auto env = QProcessEnvironment::systemEnvironment();
    env.insert("GH_PROMPT_DISABLED", "1");
    env.insert("GH_PAGER", "cat");
    p->setProcessEnvironment(env);
    p->setStandardInputFile(QProcess::nullDevice());
    p->start(program, args);
    timer->start(qMax(1, timeout));
}
}
