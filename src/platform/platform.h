#pragma once
#include "core/types.h"
#include <QObject>
#include <QStringList>
#include <functional>
#include <memory>

namespace runperch {
class CommandExecutor;
using ServiceCallback = std::function<void(ServiceObservation)>;
using ActionCallback = std::function<void(bool, QString)>;
class Platform : public QObject {
public:
    using QObject::QObject;
    virtual QString description() const = 0;
    virtual QStringList suggestedRunnerDirectories() const = 0;
    virtual QString githubExecutable() const = 0;
    virtual void inspect(const Registration&, ServiceCallback) = 0;
    virtual void act(const Registration&, ServiceAction, ActionCallback) = 0;
};
std::unique_ptr<Platform> createPlatform(CommandExecutor& executor);
}
