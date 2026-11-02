#pragma once
#include <QDateTime>
#include <QJsonObject>
#include <QString>
#include <optional>

namespace runperch {
enum class ServiceState { Unknown, Running, Stopped };
enum class ConnectionState { Unknown, Online, Offline };
enum class JobState { Unknown, Idle, Running };
enum class ServiceAction { Start, Stop };
enum class StopAuthorization { IdleOnly, ConfirmedImmediate };
enum class LocalWorkState { Unknown, NoneObserved, Active };

struct Registration {
    QString root;
    QString name;
    QString githubUrl;
    QString owner;
    QString repository;
    qint64 runnerId = 0;
    QString serviceHint;
    bool operator==(const Registration&) const = default;
};
struct ServiceObservation {
    ServiceState state = ServiceState::Unknown;
    QDateTime observedAt;
    QString detail;
    bool controlsAvailable = false;
    qint64 pid = 0;
    std::optional<double> cpuCores;
    std::optional<qint64> rssBytes;
    std::optional<qint64> freeDiskBytes;
    std::optional<qint64> uptimeSeconds;
    std::optional<int> activeWorkers;
};
struct RemoteObservation {
    ConnectionState connection = ConnectionState::Unknown;
    JobState job = JobState::Unknown;
    QDateTime observedAt;
    QString detail;
    QString jobName;
    QString workflowName;
    QString stepName;
    QString jobUrl;
    qint64 jobId = 0;
    QDateTime startedAt;
    QString lastJobName;
    QString lastJobUrl;
    QString lastConclusion;
    qint64 lastJobId = 0;
    QDateTime lastCompletedAt;
    bool jobsReadable = false;
    bool workflowsReadable = false;
    bool runnerStatusReadable = false;
    QString permissionDetail;
};
struct ResourceSample {
    QDateTime observedAt;
    qint64 pid=0;
    std::optional<double> cpuCores;
    std::optional<qint64> rssBytes;
};
struct Runner {
    quint64 instanceId = 0;
    Registration registration;
    ServiceObservation service;
    RemoteObservation remote;
    QList<ResourceSample> resources;
    bool refreshing = false;
    bool serviceRefreshing = false;
    bool remoteRefreshing = false;
    bool changingService = false;
};
struct ControlDecision { bool allowed = false; QString reason; };
inline constexpr int observationFreshnessSeconds = 30;
bool isFresh(QDateTime observedAt, QDateTime now = QDateTime::currentDateTimeUtc(),
             int maxAgeSeconds = observationFreshnessSeconds);
QString text(ServiceState state);
QString text(ConnectionState state);
QString text(JobState state);
QString text(LocalWorkState state);
LocalWorkState localWorkState(const ServiceObservation&, QDateTime now = QDateTime::currentDateTimeUtc());
QString status(const Runner& runner);
ControlDecision canControl(const Runner& runner, ServiceAction action,
                           QDateTime now, StopAuthorization = StopAuthorization::IdleOnly,
                           int maxAgeSeconds = observationFreshnessSeconds);
QJsonObject toJson(const Runner& runner);
}
