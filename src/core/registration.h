#pragma once
#include "core/types.h"
#include <QByteArray>

namespace runperch {
struct RegistrationResult { std::optional<Registration> value; QString error; };
RegistrationResult parseRegistration(QByteArray data, const QString& canonicalRoot);
RegistrationResult readRegistration(const QString& root);
}
