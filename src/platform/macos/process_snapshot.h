#pragma once
#include "core/command.h"
#include <QString>
#include <optional>
namespace runperch {
struct ProcessStats {
    std::optional<double> cpuCores;
    std::optional<qint64> rssBytes;
    std::optional<qint64> uptimeSeconds;
    std::optional<int> activeWorkers;
};
ProcessStats parseProcessSnapshot(const QByteArray&,qint64 rootPid,const QString& workerExecutable);
void readProcessSnapshot(CommandExecutor&,qint64 rootPid,const QString& workerExecutable,
                         std::function<void(ProcessStats)>);
}
