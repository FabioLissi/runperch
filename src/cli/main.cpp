#include "core/catalog.h"
#include "core/command.h"
#include "core/monitor.h"
#include "runperch_version.h"
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTextStream>

using namespace runperch;
int main(int argc, char** argv) {
    QCoreApplication app(argc,argv);
    app.setApplicationName("Runperch"); app.setOrganizationName("Runperch");
    app.setApplicationVersion(RUNPERCH_VERSION);
    QCommandLineParser parser;
    parser.setApplicationDescription("Inspect existing GitHub Actions runners. Read-only; no runner services are changed.");
    parser.addHelpOption(); parser.addVersionOption();
    parser.addOption({{"r","runner"},"Existing runner directory (repeatable).", "directory"});
    parser.addOption({"discover","Include platform-suggested runner installations."});
    parser.addOption({"local-only","Do not query GitHub."});
    parser.addOption({"runner-status","Also query the optional GitHub runner-management API (requires extra permissions)."});
    parser.process(app);
    ProcessExecutor executor;
    QObject::connect(&app,&QCoreApplication::aboutToQuit,&executor,&ProcessExecutor::cancel);
    auto platform=createPlatform(executor);
    GitHubProvider github(executor,platform->githubExecutable());
    github.setRunnerStatusAccessEnabled(parser.isSet("runner-status"));
    Monitor monitor(*platform,github);
    QStringList paths=parser.values("runner");
    if(parser.isSet("discover")) paths.append(platform->suggestedRunnerDirectories());
    if(paths.isEmpty()) paths=savedRunnerDirectories();
    paths.removeDuplicates();
    for(const auto& path : paths) {
        QString error;
        if(!monitor.add(path,&error)) { QTextStream(stderr) << error << '\n'; return 2; }
    }
    if(monitor.runners().isEmpty()) {
        QTextStream(stderr) << "No runners configured. Use --runner DIRECTORY or --discover.\n"; return 2;
    }
    auto output=[&] {
        QJsonArray list; for(const auto& r : monitor.runners()) list.append(toJson(r));
        QTextStream(stdout) << QJsonDocument(QJsonObject{{"version",RUNPERCH_VERSION},
            {"platform",platform->description()},{"runners",list}}).toJson();
    };
    QObject::connect(&monitor,&Monitor::refreshFinished,&app,[&] { output(); app.exit(0); });
    QTimer::singleShot(45000,&app,[&] { output(); QTextStream(stderr) << "Observation deadline reached; incomplete state is unknown.\n"; app.exit(2); });
    QTimer::singleShot(0,&app,[&] { monitor.refresh(parser.isSet("local-only")); });
    return app.exec();
}
