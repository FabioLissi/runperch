#include "core/resource_history.h"
#include <cmath>
namespace runperch {
void recordResources(Runner& runner,const ServiceObservation& observation,QDateTime now) {
    auto& history=runner.resources;
    while(!history.isEmpty() && history.first().observedAt<now.addSecs(-resourceHistoryWindowSeconds)) history.removeFirst();
    if(!isFresh(observation.observedAt,now) || (!history.isEmpty() && observation.observedAt<=history.last().observedAt)) return;
    ResourceSample sample; sample.observedAt=observation.observedAt; sample.pid=observation.pid;
    if(observation.state==ServiceState::Running) {
        if(observation.cpuCores && std::isfinite(*observation.cpuCores) && *observation.cpuCores>=0) sample.cpuCores=observation.cpuCores;
        if(observation.rssBytes && *observation.rssBytes>=0) sample.rssBytes=observation.rssBytes;
    }
    history.append(sample);
    while(history.size()>resourceHistoryMaxSamples) history.removeFirst();
}
}
