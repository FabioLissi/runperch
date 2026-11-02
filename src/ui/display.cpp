#include "ui/display.h"
#include <QRegularExpression>
namespace runperch {
QString duration(qint64 seconds) {
    if(seconds<0) return "—";
    if(seconds>=86400) return QString("%1d %2h %3m").arg(seconds/86400).arg(seconds/3600%24).arg(seconds/60%60);
    if(seconds>=3600) return QString("%1h %2m").arg(seconds/3600).arg(seconds/60%60);
    return seconds<60 ? QString("%1s").arg(seconds) : QString("%1m %2s").arg(seconds/60).arg(seconds%60);
}
QString elapsed(QDateTime start) { return start.isValid() ? duration(qMax<qint64>(0,start.secsTo(QDateTime::currentDateTimeUtc()))) : "—"; }
QUrl currentJobUrl(const Runner& r,QDateTime now) {
    if(!isFresh(r.remote.observedAt,now) || r.remote.job!=JobState::Running || r.remote.jobId<=0) return {};
    const QUrl url(r.remote.jobUrl,QUrl::StrictMode);
    const QString prefix="/"+r.registration.owner+"/"+r.registration.repository+"/actions/runs/";
    if(!url.isValid() || url.scheme()!="https" || url.host()!="github.com" || !url.userInfo().isEmpty()
       || url.port(-1)!=-1 || !url.path().startsWith(prefix) || url.hasFragment() || url.hasQuery()) return {};
    const QRegularExpression route("^"+QRegularExpression::escape(prefix)+"[1-9][0-9]*/job/([1-9][0-9]*)$");
    const auto match=route.match(url.path()); bool validId=false;
    const auto jobId=match.captured(1).toLongLong(&validId);
    if(!match.hasMatch() || !validId || jobId!=r.remote.jobId) return {};
    return url;
}
}
