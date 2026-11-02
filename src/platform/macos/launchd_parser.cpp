#include "platform/macos/launchd_parser.h"
#include <QRegularExpression>
namespace runperch {
ServiceObservation parseLaunchdStatus(const QByteArray& data, const QString& plist, const QString& root) {
    ServiceObservation out; out.observedAt=QDateTime::currentDateTimeUtc();
    const QString output=QString::fromUtf8(data);
    auto field=[&output](const QString& name) {
        QRegularExpression pattern("(?:^|\\n)[ \\t]*" + QRegularExpression::escape(name) + " = ([^\\n]*)");
        auto matches=pattern.globalMatch(output); QString value; int count=0;
        while(matches.hasNext()) { value=matches.next().captured(1).trimmed(); ++count; }
        return count==1 ? value : QString();
    };
    if(field("path")!=plist || field("program")!=root+"/runsvc.sh" || field("working directory")!=root) {
        out.detail="Loaded LaunchAgent identity does not match this runner; controls disabled."; return out;
    }
    QRegularExpression pid("(?:^|\\n)\\s*pid = ([1-9][0-9]*)\\s*(?:\\n|$)");
    auto match=pid.match(output);
    if(!match.hasMatch()) { out.detail="LaunchAgent loaded without a running PID; state unknown."; return out; }
    out.state=ServiceState::Running; out.pid=match.captured(1).toLongLong();
    out.controlsAvailable=true; out.detail="Existing LaunchAgent · GUI session preserved";
    return out;
}
}
