#pragma once
#include "core/types.h"
#include <QUrl>
namespace runperch {
QString duration(qint64 seconds);
QString elapsed(QDateTime start);
QUrl currentJobUrl(const Runner&,QDateTime now=QDateTime::currentDateTimeUtc());
}
