#include "platform/macos/process_snapshot.h"
#include <QFileInfo>
#include <QHash>
#include <QMultiHash>
#include <QRegularExpression>
#include <QSet>
#include <cmath>
#include <limits>
namespace runperch {
namespace {
std::optional<qint64> elapsedSeconds(const QString& value) {
    const auto pieces=value.split('-');
    if(pieces.size()>2) return {};
    bool okay=true; qint64 days=0;
    if(pieces.size()==2) { days=pieces[0].toLongLong(&okay); if(!okay || days<0) return {}; }
    const auto clock=pieces.last().split(':');
    if(clock.size()<2 || clock.size()>3 || (pieces.size()==2 && clock.size()!=3)) return {};
    QList<qint64> numbers;
    static const QRegularExpression digits("^[0-9]+$");
    for(const auto& part:clock) {
        if(!digits.match(part).hasMatch()) return {};
        const auto number=part.toLongLong(&okay); if(!okay || number<0) return {}; numbers<<number;
    }
    const qint64 hours=numbers.size()==3 ? numbers[0] : 0;
    const qint64 minutes=numbers[numbers.size()-2], seconds=numbers.last();
    if(hours>23 || minutes>59 || seconds>59) return {};
    const auto tail=hours*3600+minutes*60+seconds;
    if(days>(std::numeric_limits<qint64>::max()-tail)/86400) return {};
    return days*86400+tail;
}
}
ProcessStats parseProcessSnapshot(const QByteArray& input,qint64 rootPid,const QString& workerExecutable) {
    if(rootPid<=0) return {};
    struct Row { double cpu; qint64 rss; qint64 elapsed; QString executable; };
    QHash<qint64,Row> rows; QMultiHash<qint64,qint64> children;
    static const QRegularExpression pattern("^\\s*([0-9]+)\\s+([0-9]+)\\s+([0-9]+(?:\\.[0-9]+)?)\\s+([0-9]+)\\s+([0-9:-]+)\\s+(.+?)\\s*$");
    for(const auto& line:QString::fromUtf8(input).split('\n')) {
        if(line.trimmed().isEmpty()) continue;
        const auto match=pattern.match(line); if(!match.hasMatch()) return {};
        bool pidOkay=false,parentOkay=false,cpuOkay=false,rssOkay=false;
        const auto pid=match.captured(1).toLongLong(&pidOkay), parent=match.captured(2).toLongLong(&parentOkay);
        const auto cpu=match.captured(3).toDouble(&cpuOkay);
        const auto rss=match.captured(4).toLongLong(&rssOkay);
        const auto elapsed=elapsedSeconds(match.captured(5));
        if(!pidOkay || !parentOkay || !cpuOkay || !rssOkay || !elapsed || !std::isfinite(cpu)
           || rss>std::numeric_limits<qint64>::max()/1024 || rows.contains(pid)) return {};
        rows.insert(pid,Row{cpu/100.0,rss*1024,*elapsed,match.captured(6)});
        children.insert(parent,pid);
    }
    if(!rows.contains(rootPid)) return {};
    QSet<qint64> members{rootPid}; QList<qint64> pending{rootPid};
    for(qsizetype i=0;i<pending.size();++i) for(auto child:children.values(pending[i])) {
        if(child==rootPid) return {};
        if(!members.contains(child)) { members.insert(child); pending<<child; }
    }
    const auto expected=workerExecutable.isEmpty() ? QString() : QFileInfo(workerExecutable).canonicalFilePath();
    double cpu=0; qint64 rss=0; int workers=0;
    for(auto pid:members) {
        const auto& row=rows[pid]; cpu+=row.cpu;
        if(row.rss>std::numeric_limits<qint64>::max()-rss || !std::isfinite(cpu)) return {};
        rss+=row.rss;
        if(pid!=rootPid && !expected.isEmpty() && QFileInfo(row.executable).canonicalFilePath()==expected) ++workers;
    }
    ProcessStats stats; stats.cpuCores=cpu; stats.rssBytes=rss; stats.uptimeSeconds=rows[rootPid].elapsed;
    if(!expected.isEmpty()) stats.activeWorkers=workers;
    return stats;
}
void readProcessSnapshot(CommandExecutor& executor,qint64 rootPid,const QString& workerExecutable,std::function<void(ProcessStats)> cb) {
    executor.run("/usr/bin/env",{"LC_ALL=C","/bin/ps","-ww","-axo","pid=,ppid=,pcpu=,rss=,etime=,comm="},
        [rootPid,workerExecutable,cb](CommandResult result) { cb(result.ok ? parseProcessSnapshot(result.output,rootPid,workerExecutable) : ProcessStats{}); });
}
}
