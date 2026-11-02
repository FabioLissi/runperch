#include "platform/platform.h"
#include <QStandardPaths>
#include <QSysInfo>
#include <QTimer>

namespace runperch {
namespace {
class UnsupportedPlatform final : public Platform {
public:
    QString description() const override { return QSysInfo::prettyProductName(); }
    QStringList suggestedRunnerDirectories() const override { return {}; }
    QString githubExecutable() const override { return QStandardPaths::findExecutable("gh"); }
    void inspect(const Registration&, ServiceCallback cb) override {
        QTimer::singleShot(0, this, [cb] {
            ServiceObservation o; o.observedAt = QDateTime::currentDateTimeUtc();
            o.detail = "Native service inspection is not implemented on this platform yet."; cb(o);
        });
    }
    void act(const Registration&, ServiceAction, ActionCallback cb) override {
        QTimer::singleShot(0, this, [cb] { cb(false, "Native service control is not implemented on this platform yet."); });
    }
};
}
std::unique_ptr<Platform> createPlatform(CommandExecutor&) { return std::make_unique<UnsupportedPlatform>(); }
}
