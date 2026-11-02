#include "core/github.h"
#include <QJsonDocument>
#include <QUrl>

namespace runperch {
RemoteObservation parseRunnerResponse(const QByteArray& data, qint64 id) {
    RemoteObservation result;
    result.observedAt = QDateTime::currentDateTimeUtc();
    const auto doc = QJsonDocument::fromJson(data);
    if(!doc.isObject() || !doc.object().value("runners").isArray()) {
        result.detail = "GitHub returned incomplete runner status."; return result;
    }
    const auto items = doc.object().value("runners").toArray();
    result.runnerStatusReadable=true;
    for(const auto& item : items) {
        auto r = item.toObject();
        if(r.value("id").toInteger() != id) continue;
        auto state = r.value("status").toString();
        if(state == "online") result.connection = ConnectionState::Online;
        else if(state == "offline") result.connection = ConnectionState::Offline;
        if(r.value("busy").isBool()) {
            if(r.value("busy").toBool()) result.job = JobState::Running;
            else if(result.connection == ConnectionState::Online) result.job = JobState::Idle;
        }
        result.detail = "GitHub runner API";
        return result;
    }
    result.detail = "Runner not found in this response; availability is unknown.";
    return result;
}
namespace {
bool matchingJob(const QJsonObject& job,const Registration& r) {
    if(job.value("runner_id").toInteger() != r.runnerId || job.value("id").toInteger()<=0) return false;
    if(job.value("runner_name").toString() != r.name) return false;
    QUrl url(job.value("html_url").toString(), QUrl::StrictMode);
    const QString expected = "/" + r.owner + "/" + r.repository + "/actions/runs/";
    if(url.scheme() != "https" || url.host() != "github.com" || !url.userInfo().isEmpty()
       || url.hasQuery() || url.hasFragment() || url.port() != -1 || !url.path().startsWith(expected)) return false;
    return true;
}
}
bool applyCompletedJob(const QJsonObject& job,const Registration& r,RemoteObservation& out) {
    if(!matchingJob(job,r) || job.value("status").toString()!="completed") return false;
    const auto completed=QDateTime::fromString(job.value("completed_at").toString(),Qt::ISODate);
    if(!completed.isValid() || (out.lastCompletedAt.isValid() && completed<=out.lastCompletedAt)) return false;
    out.lastJobId=job.value("id").toInteger(); out.lastJobName=job.value("name").toString();
    out.lastJobUrl=job.value("html_url").toString(); out.lastConclusion=job.value("conclusion").toString();
    out.lastCompletedAt=completed; return true;
}
bool applyJobResponse(const QJsonObject& job, const Registration& r, RemoteObservation& out) {
    if(!matchingJob(job,r) || job.value("status").toString()!="in_progress") return false;
    out.job = JobState::Running;
    out.jobId = job.value("id").toInteger();
    out.jobName = job.value("name").toString();
    out.workflowName = job.value("workflow_name").toString();
    out.jobUrl = job.value("html_url").toString();
    out.startedAt = QDateTime::fromString(job.value("started_at").toString(), Qt::ISODate);
    out.stepName.clear();
    for(const auto& v : job.value("steps").toArray()) {
        const auto step = v.toObject();
        if(step.value("status").toString() == "in_progress") { out.stepName = step.value("name").toString(); break; }
    }
    out.detail = "Active job matched by runner ID and name. " + out.detail;
    return true;
}
GitHubProvider::GitHubProvider(CommandExecutor& e, QString path, QObject* parent)
    : RemoteProvider(parent), executor(e), executable(std::move(path)) {}
void GitHubProvider::api(const QString& path, CommandCallback cb) {
    // Always read-only GET. Never pass a user-supplied host or shell command.
    executor.run(executable, {"api", "--method", "GET", "--hostname", "github.com",
                             "-H", "Accept: application/vnd.github+json", path}, std::move(cb));
}
void GitHubProvider::observe(const Registration& r, RemoteCallback cb) {
    if(executable.isEmpty()) {
        RemoteObservation out; out.observedAt = QDateTime::currentDateTimeUtc();
        out.detail = "GitHub CLI not found. Install gh and authenticate in Terminal."; cb(out); return;
    }
    if(!runnerStatusEnabled) {
        RemoteObservation out; out.observedAt=QDateTime::currentDateTimeUtc();
        out.detail="Job observations only; runner-management access is not requested.";
        if(!r.repository.isEmpty()) jobs(r,out,cb);
        else { out.detail="Organization-scoped runner: local monitoring available; no repository selected for job observations."; cb(out); }
        return;
    }
    const QString endpoint = r.repository.isEmpty() ? "orgs/" + r.owner + "/actions/runners?per_page=100"
        : "repos/" + r.owner + "/" + r.repository + "/actions/runners?per_page=100";
    api(endpoint, [this, r, cb](CommandResult result) {
        RemoteObservation out;
        if(result.ok) out = parseRunnerResponse(result.output, r.runnerId);
        else { out.observedAt = QDateTime::currentDateTimeUtc();
            out.detail = "Runner availability is not visible to this account."; }
        if(!r.repository.isEmpty()) {
            if(result.ok) { jobs(r,out,cb); return; }
            api("repos/"+r.owner+"/"+r.repository,[this,r,out,cb](CommandResult info) mutable {
                const auto permissions=QJsonDocument::fromJson(info.output).object().value("permissions").toObject();
                if(info.ok && permissions.value("admin").isBool() && !permissions.value("admin").toBool())
                    out.permissionDetail="This account is not a repository administrator. Signing in again cannot grant that role.";
                else out.permissionDetail="Runner visibility requires the appropriate repository role and runner-read token permissions.";
                jobs(r,out,cb);
            });
        } else cb(out);
    });
}
void GitHubProvider::jobs(const Registration& r, RemoteObservation out, RemoteCallback cb) {
    api("repos/" + r.owner + "/" + r.repository + "/actions/runs?status=in_progress&per_page=3",
        [this, r, out, cb](CommandResult result) mutable {
            const auto doc = QJsonDocument::fromJson(result.output);
            if(!result.ok || !doc.isObject() || !doc.object().value("workflow_runs").isArray()) { cb(out); return; }
            out.workflowsReadable=true;
            jobRuns(r, doc.object().value("workflow_runs").toArray(), 0, out,[this,r,cb](RemoteObservation active) {
                if(active.jobId>0) cb(active);
                else completedJobs(r,active,cb);
            });
        });
}
void GitHubProvider::completedJobs(const Registration& r,RemoteObservation out,RemoteCallback cb) {
    api("repos/"+r.owner+"/"+r.repository+"/actions/runs?status=completed&per_page=3",
        [this,r,out,cb](CommandResult result) mutable {
            const auto doc=QJsonDocument::fromJson(result.output);
            if(!result.ok || !doc.isObject() || !doc.object().value("workflow_runs").isArray()) { cb(out); return; }
            out.workflowsReadable=true;
            jobRuns(r,doc.object().value("workflow_runs").toArray(),0,out,cb);
        });
}
void GitHubProvider::jobRuns(const Registration& r, QJsonArray runs, int index, RemoteObservation out, RemoteCallback cb) {
    if(index >= runs.size() || index >= 3) { cb(out); return; }
    auto run = runs[index].toObject();
    auto id = run.value("id").toInteger();
    if(id <= 0) { jobRuns(r, runs, index+1, out, cb); return; }
    api("repos/" + r.owner + "/" + r.repository + "/actions/runs/" + QString::number(id) + "/jobs?per_page=100",
        [this, r, runs, index, out, cb, run](CommandResult result) mutable {
            const auto doc = QJsonDocument::fromJson(result.output);
            if(result.ok && doc.isObject() && doc.object().value("jobs").isArray()) {
              out.jobsReadable=true;
              for(const auto& v : doc.object().value("jobs").toArray()) {
                if(applyJobResponse(v.toObject(), r, out)) {
                    out.workflowName = run.value("name").toString();
                    cb(out); return;
                } else {
                    applyCompletedJob(v.toObject(),r,out);
                }
              }
            }
            jobRuns(r, runs, index+1, out, cb);
        });
}
}
