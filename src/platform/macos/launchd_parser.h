#pragma once
#include "core/types.h"
namespace runperch {
// `launchctl print` is a diagnostic format, not a stable API. Fail closed if
// identity fields change or are absent. See design/architecture.md.
ServiceObservation parseLaunchdStatus(const QByteArray& output, const QString& plist,
                                      const QString& root);
}
