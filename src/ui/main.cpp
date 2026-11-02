#include "ui/window.h"
#include "core/catalog.h"
#include "runperch_version.h"
#include <QApplication>
#include <QCommandLineParser>
#include <QMessageBox>
#include <QTimer>
#include <QTextStream>
#include <QSettings>
#include <QTemporaryDir>

using namespace runperch;
int main(int argc,char** argv) {
    QApplication app(argc,argv);
    app.setOrganizationName("Runperch"); app.setApplicationName("Runperch"); app.setApplicationVersion(RUNPERCH_VERSION);
    app.setQuitOnLastWindowClosed(!QSystemTrayIcon::isSystemTrayAvailable());
    QCommandLineParser parser; parser.setApplicationDescription("Your runners, in view.");
    parser.addHelpOption(); parser.addVersionOption();
    parser.addOption({{"r","runner"},"Monitor an existing runner directory (repeatable).","directory"});
    parser.addOption({"screenshot","Save the application window after the first observation and exit (development check).","file"});
    parser.process(app);
    QTemporaryDir screenshotSettings;
    if(parser.isSet("screenshot")) {
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,screenshotSettings.path());
    }
    ProcessExecutor executor;
    QObject::connect(&app,&QCoreApplication::aboutToQuit,&executor,&ProcessExecutor::cancel);
    auto platform=createPlatform(executor);
    GitHubProvider github(executor,platform->githubExecutable());
    Monitor monitor(*platform,github);
    monitor.setInterval(savedRefreshInterval());
    QStringList paths=savedRunnerDirectories(); paths.append(parser.values("runner")); paths.removeDuplicates();
    QStringList invalid;
    for(const auto& path : paths) { QString error; if(!monitor.add(path,&error)) invalid << error; }
    Window window(monitor,*platform); window.show();
    if(!invalid.isEmpty()) QTimer::singleShot(0,&window,[&] { QMessageBox::warning(&window,"Runner configuration",invalid.join('\n')); });
    if(parser.isSet("screenshot")) {
        auto capture=[&] { app.exit(window.grab().save(parser.value("screenshot")) ? 0 : 2); };
        QObject::connect(&monitor,&Monitor::refreshFinished,&window,[&window,capture] { QTimer::singleShot(250,&window,capture); });
        QTimer::singleShot(45000,&window,capture);
    }
    if(monitor.runners().isEmpty() && !parser.isSet("screenshot")) {
        const auto candidates=platform->suggestedRunnerDirectories();
        if(!candidates.isEmpty()) QTimer::singleShot(0,&window,[&window,&monitor,candidates] {
            if(QMessageBox::question(&window,"Existing runners found",
                QString("Found %1 existing runner installation(s). Add them for monitoring? Registration and services will not be changed.").arg(candidates.size()),
                QMessageBox::Yes|QMessageBox::No,QMessageBox::Yes)==QMessageBox::Yes) {
                for(const auto& path : candidates) monitor.add(path);
                saveRunnerDirectories(monitor.directories()); monitor.refresh();
            }
        });
    }
    QTimer::singleShot(0,&monitor,[&] { monitor.start(); });
    return app.exec();
}
