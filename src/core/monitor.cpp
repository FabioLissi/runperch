#include "core/monitor.h"
#include "core/registration.h"
#include "core/resource_history.h"
#include <QJsonDocument>
#include <QSettings>

namespace runperch {
Monitor::Monitor(Platform& p, RemoteProvider& g, QObject* parent) : QObject(parent), platform(p), github(g) {
    timer.setInterval(15000);
    connect(&timer, &QTimer::timeout, this, [this] { refresh(); });
}
int Monitor::find(const QString& root) const {
    for(int i=0;i<items.size();++i) if(items[i].registration.root == root) return i;
    return -1;
}
bool Monitor::add(const QString& path, QString* error) {
    auto parsed = readRegistration(path);
    if(!parsed.value) { if(error) *error = parsed.error; return false; }
    if(find(parsed.value->root) >= 0) return true;
    Runner runner; runner.instanceId=++nextInstanceId; runner.registration = *parsed.value; items.append(runner);
    emit changed(); return true;
}
void Monitor::remove(int i) { if(i>=0 && i<items.size() && !items[i].changingService) { items.removeAt(i); emit changed(); } }
QStringList Monitor::directories() const { QStringList out; for(const auto& r : items) out << r.registration.root; return out; }
void Monitor::setInterval(int seconds) { timer.setInterval(qBound(5,seconds,3600)*1000); }
void Monitor::start() { timer.start(); refresh(); }
void Monitor::refresh(bool localOnly) {
    if(pending != 0) return;
    for(const auto& r : items) if(r.changingService) return;
    const quint64 cycle = ++generation;
    pending = items.size() * (localOnly ? 1 : 2);
    if(pending == 0) { emit refreshFinished(); return; }
    // Set all flags before starting: adapters may complete synchronously.
    for(auto& r : items) { r.serviceRefreshing=true; r.remoteRefreshing=!localOnly; r.refreshing=true; }
    emit changed();
    const auto snapshot = items;
    for(const auto& r : snapshot) {
        const auto root = r.registration.root;
        auto registration = readRegistration(root);
        if(!registration.value || *registration.value != r.registration) {
            ServiceObservation s; s.observedAt = QDateTime::currentDateTimeUtc();
            s.detail = "Runner registration changed or is unavailable. Remove and re-add it.";
            complete(root, cycle, r.instanceId, false, s, {});
            if(!localOnly) complete(root, cycle, r.instanceId, true, {}, {});
            continue;
        }
        platform.inspect(*registration.value, [this,root,cycle,id=r.instanceId](ServiceObservation o) { complete(root,cycle,id,false,o,{}); });
        if(!localOnly) github.observe(*registration.value, [this,root,cycle,id=r.instanceId](RemoteObservation o) { complete(root,cycle,id,true,{},o); });
    }
}
void Monitor::remember(const Runner& r) {
    const bool active=r.remote.jobId>0;
    const auto id=active ? r.remote.jobId : r.remote.lastJobId;
    if(id<=0) return;
    QSettings settings;
    auto records = QJsonDocument::fromJson(settings.value("history").toByteArray()).array();
    const QString key = r.registration.githubUrl + "/" + QString::number(id);
    QJsonObject entry{{"key",key},{"runner",r.registration.name},{"job",active ? r.remote.jobName : r.remote.lastJobName},
        {"workflow",r.remote.workflowName},{"url",active ? r.remote.jobUrl : r.remote.lastJobUrl},
        {"result",active ? "Running when observed" : r.remote.lastConclusion},
        {"last_seen",QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},
        {"started_at",r.remote.startedAt.toString(Qt::ISODate)},
        {"completed_at",active ? QString() : r.remote.lastCompletedAt.toString(Qt::ISODate)}};
    for(int i=0;i<records.size();++i) if(records[i].toObject().value("key").toString() == key) { records.removeAt(i); break; }
    records.prepend(entry);
    while(records.size() > 200) records.removeLast();
    settings.setValue("history", QJsonDocument(records).toJson(QJsonDocument::Compact));
}
void Monitor::complete(const QString& root, quint64 cycle, quint64 id, bool remote, ServiceObservation s, RemoteObservation g) {
    if(cycle != generation || pending == 0) return;
    const int i = find(root);
    if(i >= 0 && items[i].instanceId==id) {
        if(remote) { items[i].remote = std::move(g); items[i].remoteRefreshing=false; remember(items[i]); }
        else { recordResources(items[i],s); items[i].service = std::move(s); items[i].serviceRefreshing=false; }
        items[i].refreshing=items[i].serviceRefreshing || items[i].remoteRefreshing;
    }
    --pending;
    emit changed();
    if(pending == 0) emit refreshFinished();
}
void Monitor::control(int i, ServiceAction action, StopAuthorization authorization) {
    if(i<0 || i>=items.size()) return;
    auto decision = canControl(items[i],action,QDateTime::currentDateTimeUtc(),authorization);
    if(!decision.allowed) { emit message(decision.reason,true); return; }
    auto registration=readRegistration(items[i].registration.root);
    if(!registration.value || *registration.value!=items[i].registration) {
        emit message("Runner registration changed. Remove and re-add it before controlling its service.",true); return;
    }
    const auto root = items[i].registration.root;
    items[i].changingService = true;
    emit changed();
    // Observe again at the shared write path, rather than relying on UI button state.
    const bool requireRemote=action==ServiceAction::Stop && authorization==StopAuthorization::IdleOnly;
    auto remaining = std::make_shared<int>(requireRemote ? 2 : 1);
    auto check = std::make_shared<Runner>(items[i]);
    check->changingService = false;
    auto finish = [this, root, action, authorization, remaining, check] {
        if(--*remaining != 0) return;
        const int index = find(root);
        if(index<0 || items[index].instanceId!=check->instanceId) return;
        auto registration=readRegistration(root);
        if(!registration.value || *registration.value!=check->registration) {
            items[index].changingService=false;
            emit message("Runner registration changed during the status check; no service action was performed.",true);
            emit changed(); return;
        }
        auto allowed = canControl(*check,action,QDateTime::currentDateTimeUtc(),authorization);
        if(!allowed.allowed) { items[index].changingService=false; emit message(allowed.reason,true); emit changed(); return; }
        platform.act(check->registration, action, [this,root,check](bool ok, QString text) {
            const int at=find(root); if(at<0 || items[at].instanceId!=check->instanceId) return;
            items[at].serviceRefreshing=true; items[at].refreshing=true;
            emit changed();
            platform.inspect(check->registration,[this,root,check,ok,text](ServiceObservation observation) {
                const int current=find(root); if(current<0 || items[current].instanceId!=check->instanceId) return;
                const auto registration=readRegistration(root);
                if(!registration.value || *registration.value!=check->registration) {
                    observation={}; observation.detail="Runner registration changed; remove and re-add it.";
                }
                recordResources(items[current],observation);
                items[current].service=std::move(observation);
                items[current].changingService=false; items[current].serviceRefreshing=false;
                items[current].refreshing=items[current].remoteRefreshing; emit changed(); emit message(text,!ok);
            });
        });
    };
    platform.inspect(check->registration, [check,finish](ServiceObservation o) { check->service=o; finish(); });
    if(requireRemote) github.observe(check->registration, [check,finish](RemoteObservation o) { check->remote=o; finish(); });
}
}
