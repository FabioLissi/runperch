#pragma once
#include <QStringList>
namespace runperch {
QStringList savedRunnerDirectories();
void saveRunnerDirectories(const QStringList&);
int savedRefreshInterval();
void saveRefreshInterval(int seconds);
}
