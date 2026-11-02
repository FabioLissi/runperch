#include "platform/platform.h"
#include "core/command.h"
#include "platform/macos/launchd_parser.h"
#include "platform/macos/process_snapshot.h"
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QStorageInfo>
#include <QSysInfo>
#include <unistd.h>

namespace runperch {
namespace {
struct ServiceIdentity { QString path; QString label; QString domain; };
class LaunchdPlatform final : public Platform {
public:
    explicit LaunchdPlatform(CommandExecutor& e) : executor(e) {}
    QString description() const override { return "macOS · " + QSysInfo::currentCpuArchitecture(); }
    QStringList suggestedRunnerDirectories() const override {
        QStringList found;
        QDir home(QDir::homePath());
        const auto dirs = home.entryList({"actions-runner*"}, QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
        for(const auto& d : dirs) if(QFileInfo::exists(home.filePath(d + "/.runner"))) found << home.filePath(d);
        return found;
    }
    QString githubExecutable() const override {
        auto gh = QStandardPaths::findExecutable("gh");
        if(!gh.isEmpty()) return gh;
        for(const auto& path : {"/opt/homebrew/bin/gh", "/usr/local/bin/gh"})
            if(QFileInfo(path).isExecutable()) return path;
        return {};
    }
    void identity(const Registration& r, std::function<void(std::optional<ServiceIdentity>, QString)> cb) {
        const auto agents = QDir(QDir::homePath()).filePath("Library/LaunchAgents");
        QFileInfo file(r.serviceHint);
        if(!file.isAbsolute()) file.setFile(QDir(agents).filePath(r.serviceHint.endsWith(".plist")
             ? r.serviceHint : r.serviceHint + ".plist"));
        if(r.serviceHint.isEmpty() || file.isSymLink() || !file.isFile() || file.size() > 65536
           || QFileInfo(file.absolutePath()).canonicalFilePath() != QFileInfo(agents).canonicalFilePath()) {
            cb({}, "No supported user LaunchAgent. Add the existing runner's launchd service first."); return;
        }
        const auto canonical = file.canonicalFilePath();
        executor.run("/usr/bin/plutil", {"-convert", "json", "-o", "-", canonical},
            [r, cb, canonical](CommandResult result) {
                const auto doc = QJsonDocument::fromJson(result.output);
                if(!result.ok || !doc.isObject()) { cb({}, "Cannot read LaunchAgent metadata."); return; }
                const auto o = doc.object();
                auto args = o.value("ProgramArguments").toArray();
                const auto label = o.value("Label").toString();
                const auto root = QFileInfo(o.value("WorkingDirectory").toString()).canonicalFilePath();
                const auto expected = QFileInfo(QDir(r.root).filePath("runsvc.sh")).canonicalFilePath();
                static const QRegularExpression valid("^actions\\.runner\\.[A-Za-z0-9_.-]+$");
                if(!valid.match(label).hasMatch() || root != r.root || expected.isEmpty()
                   || args.size() != 1 || QFileInfo(args[0].toString()).canonicalFilePath() != expected
                   || QFileInfo(QDir(r.root).filePath("runsvc.sh")).isSymLink()) {
                    cb({}, "LaunchAgent identity does not match this official runner installation."); return;
                }
                cb(ServiceIdentity{canonical, label, QString("gui/%1").arg(getuid())}, {});
            });
    }
    void inspect(const Registration& r, ServiceCallback cb) override {
        identity(r, [this, cb, r](std::optional<ServiceIdentity> id, QString error) {
            ServiceObservation base;
            base.observedAt = QDateTime::currentDateTimeUtc();
            const QStorageInfo volume(r.root);
            if(volume.isValid() && volume.isReady()) base.freeDiskBytes = volume.bytesAvailable();
            if(!id) { base.detail = error; cb(base); return; }
            executor.run("/bin/launchctl", {"print", id->domain + "/" + id->label},
              [this, cb, base, id, r](CommandResult result) mutable {
                if(!result.ok) {
                    // Confirm the GUI domain itself is accessible before calling an absent service stopped.
                    executor.run("/bin/launchctl", {"print", id->domain}, [cb, base, result](CommandResult domain) mutable {
                        if(domain.ok && result.exitCode == 113) {
                            base.state = ServiceState::Stopped;
                            base.controlsAvailable = true;
                            base.detail = "LaunchAgent is not loaded in your GUI session.";
                        } else base.detail = "launchd status unavailable; service state is unknown.";
                        cb(base);
                    });
                    return;
                }
                auto parsed=parseLaunchdStatus(result.output,id->path,r.root);
                parsed.freeDiskBytes=base.freeDiskBytes; base=parsed;
                if(base.state!=ServiceState::Running) { cb(base); return; }
                const auto worker=QDir(r.root).filePath("bin/Runner.Worker");
                readProcessSnapshot(executor,base.pid,worker,[base,cb](ProcessStats stats) mutable {
                    base.cpuCores=stats.cpuCores; base.rssBytes=stats.rssBytes;
                    base.uptimeSeconds=stats.uptimeSeconds; base.activeWorkers=stats.activeWorkers;
                    cb(base);
                });
              });
        });
    }
    void act(const Registration& r, ServiceAction action, ActionCallback cb) override {
        // Revalidate installation/plist identity immediately before every mutation.
        identity(r, [this, action, cb, r](std::optional<ServiceIdentity> id, QString error) {
            if(!id) { cb(false, error); return; }
            auto perform=[this,action,cb,id] {
              executor.run("/bin/launchctl", {action == ServiceAction::Start ? "bootstrap" : "bootout", id->domain, id->path},
               [cb](CommandResult result) {
                    cb(result.ok, result.ok ? "Service request accepted; refreshing status."
                                             : "launchd rejected the request. Check the service in Terminal.");
               });
            };
            if(action==ServiceAction::Start) { perform(); return; }
            executor.run("/bin/launchctl",{"print",id->domain+"/"+id->label},[perform,cb,id,r](CommandResult result) {
                if(!result.ok || !parseLaunchdStatus(result.output,id->path,r.root).controlsAvailable) {
                    cb(false,"Loaded service identity changed or is unavailable. No stop was performed."); return;
                }
                perform();
            });
        });
    }
private:
    CommandExecutor& executor;
};
}
std::unique_ptr<Platform> createPlatform(CommandExecutor& e) { return std::make_unique<LaunchdPlatform>(e); }
}
