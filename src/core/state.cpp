#include "core/types.h"
#include <QJsonValue>

namespace runperch {
bool isFresh(QDateTime time, QDateTime now, int maxAge) {
    return time.isValid() && time<=now && time.secsTo(now)<=maxAge;
}
QString text(ServiceState s) {
    switch(s) { case ServiceState::Running: return "Running";
    case ServiceState::Stopped: return "Stopped"; default: return "Unknown"; }
}
QString text(ConnectionState s) {
    switch(s) { case ConnectionState::Online: return "Online";
    case ConnectionState::Offline: return "Offline"; default: return "Unknown"; }
}
QString text(JobState s) {
    switch(s) { case JobState::Running: return "Running";
    case JobState::Idle: return "Idle"; default: return "Unknown"; }
}
QString status(const Runner& r) {
    auto now=QDateTime::currentDateTimeUtc();
    auto fresh=[now](QDateTime time) { return isFresh(time,now); };
    if(fresh(r.remote.observedAt) && r.remote.job == JobState::Running) return "Running";
    if(fresh(r.service.observedAt) && r.service.state == ServiceState::Stopped) return "Stopped";
    if(fresh(r.remote.observedAt) && r.remote.connection == ConnectionState::Offline) return "Offline";
    if(fresh(r.service.observedAt) && fresh(r.remote.observedAt)
       && r.service.state == ServiceState::Running && r.remote.connection == ConnectionState::Online
       && r.remote.job == JobState::Idle) return "Idle";
    if(fresh(r.service.observedAt) && r.service.state==ServiceState::Running) return "Service running";
    return "Unknown";
}
ControlDecision canControl(const Runner& r, ServiceAction a, QDateTime now, int maxAge) {
    if(r.changingService || r.refreshing) return {false, "Wait for the current operation."};
    if(!r.service.controlsAvailable) return {false, "This service cannot be controlled by this adapter."};
    auto fresh = [&](QDateTime time) {
        return isFresh(time,now,maxAge);
    };
    if(!fresh(r.service.observedAt)) return {false, "Refresh local service status first."};
    if(a == ServiceAction::Start) {
        if(r.service.state != ServiceState::Stopped) return {false, "Service must be confirmed stopped."};
        if(r.remote.job == JobState::Running) return {false, "A job is still reported running."};
        return {true, {}};
    }
    if(r.service.state != ServiceState::Running) return {false, "Service must be confirmed running."};
    if(!fresh(r.remote.observedAt)) return {false, "Refresh GitHub status first."};
    if(r.remote.connection != ConnectionState::Online || r.remote.job != JobState::Idle)
        return {false, "Stop is available only when GitHub confirms this runner is idle."};
    return {true, {}};
}
QJsonObject toJson(const Runner& r) {
    QJsonObject service{{"state", text(r.service.state)}, {"detail", r.service.detail},
        {"observed_at", r.service.observedAt.toString(Qt::ISODate)},
        {"controls_available", r.service.controlsAvailable}, {"pid", r.service.pid}};
    if(r.service.cpuCores) service["cpu_cores"] = *r.service.cpuCores;
    if(r.service.rssBytes) service["aggregate_rss_bytes"] = *r.service.rssBytes;
    if(r.service.freeDiskBytes) service["free_disk_bytes"] = *r.service.freeDiskBytes;
    return {{"name", r.registration.name}, {"root", r.registration.root},
        {"github_url", r.registration.githubUrl}, {"status", status(r)},
        {"service", service}, {"refreshing", r.refreshing},
        {"github", QJsonObject{{"connection", text(r.remote.connection)},
           {"job_state", text(r.remote.job)}, {"detail", r.remote.detail},
           {"observed_at", r.remote.observedAt.toString(Qt::ISODate)},
           {"job_name", r.remote.jobName}, {"workflow", r.remote.workflowName},
           {"step", r.remote.stepName}, {"job_url", r.remote.jobUrl},
           {"job_id", r.remote.jobId}, {"started_at", r.remote.startedAt.toString(Qt::ISODate)},
           {"latest_job_name",r.remote.lastJobName},{"latest_job_url",r.remote.lastJobUrl},
           {"latest_conclusion",r.remote.lastConclusion},{"latest_completed_at",r.remote.lastCompletedAt.toString(Qt::ISODate)},
           {"runner_status_readable",r.remote.runnerStatusReadable},{"jobs_readable",r.remote.jobsReadable},
           {"permission_detail",r.remote.permissionDetail}}}};
}
}
