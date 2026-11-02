#pragma once
#include <QObject>
#include <QStringList>
#include <functional>

namespace runperch {
struct CommandResult {
    bool ok = false;
    int exitCode = -1;
    QByteArray output;
    bool timedOut = false;
    bool outputLimited = false;
};
using CommandCallback = std::function<void(CommandResult)>;
class CommandExecutor : public QObject {
public:
    using QObject::QObject;
    virtual void run(const QString& program, const QStringList& args, CommandCallback callback,
                     int timeoutMs = 8000, qsizetype outputLimit = 2 * 1024 * 1024) = 0;
};
class ProcessExecutor final : public CommandExecutor {
public:
    using CommandExecutor::CommandExecutor;
    ~ProcessExecutor() override;
    void cancel();
    void run(const QString&, const QStringList&, CommandCallback, int = 8000,
             qsizetype = 2 * 1024 * 1024) override;
};
}
