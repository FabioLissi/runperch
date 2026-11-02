#pragma once
#include "core/types.h"
namespace runperch {
inline constexpr int resourceHistoryWindowSeconds=900;
inline constexpr int resourceHistoryMaxSamples=180;
void recordResources(Runner&, const ServiceObservation&, QDateTime now=QDateTime::currentDateTimeUtc());
}
