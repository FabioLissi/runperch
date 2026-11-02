#include "core/catalog.h"
#include <QSettings>
namespace runperch {
QStringList savedRunnerDirectories() { return QSettings().value("runnerDirectories").toStringList(); }
void saveRunnerDirectories(const QStringList& roots) { QSettings().setValue("runnerDirectories", roots); }
int savedRefreshInterval() { return qBound(5, QSettings().value("refreshSeconds", 15).toInt(), 3600); }
void saveRefreshInterval(int seconds) { QSettings().setValue("refreshSeconds", qBound(5,seconds,3600)); }
}
