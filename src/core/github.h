#pragma once
#include "core/types.h"
#include "core/command.h"
#include <QJsonArray>

namespace runperch {
using RemoteCallback = std::function<void(RemoteObservation)>;
class RemoteProvider : public QObject {
public:
    using QObject::QObject;
    virtual void observe(const Registration&, RemoteCallback) = 0;
    virtual void setRunnerStatusAccessEnabled(bool) {}
};
RemoteObservation parseRunnerResponse(const QByteArray&, qint64 runnerId);
bool applyJobResponse(const QJsonObject& job, const Registration&, RemoteObservation&);
bool applyCompletedJob(const QJsonObject& job, const Registration&, RemoteObservation&);
class GitHubProvider final : public RemoteProvider {
public:
    GitHubProvider(CommandExecutor&, QString executable, QObject* parent = nullptr);
    void observe(const Registration&, RemoteCallback) override;
    void setRunnerStatusAccessEnabled(bool enabled) override { runnerStatusEnabled=enabled; }
private:
    void completedJobs(const Registration&,RemoteObservation,RemoteCallback);
    void jobs(const Registration&, RemoteObservation, RemoteCallback);
    void jobRuns(const Registration&, QJsonArray, int, RemoteObservation, RemoteCallback);
    void api(const QString&, CommandCallback);
    CommandExecutor& executor;
    QString executable;
    bool runnerStatusEnabled=false;
};
}
