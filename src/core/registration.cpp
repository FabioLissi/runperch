#include "core/registration.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QUrl>
#include <cmath>

namespace runperch {
RegistrationResult parseRegistration(QByteArray data, const QString& root) {
    if(data.size() > 65536) return {{}, "Runner metadata exceeds 64 KiB."};
    if(data.startsWith("\xEF\xBB\xBF")) data.remove(0,3);
    QJsonParseError error;
    auto doc = QJsonDocument::fromJson(data, &error);
    if(error.error != QJsonParseError::NoError || !doc.isObject()) return {{}, "Invalid .runner JSON."};
    auto obj = doc.object();
    Registration r;
    r.root = root;
    r.name = obj.value("agentName").toString();
    if(r.name.trimmed().isEmpty() || r.name.size() > 256 || r.name.contains(QChar::Null))
        return {{}, "Runner name is missing or invalid."};
    const auto id = obj.value("agentId");
    if(!id.isDouble() || id.toDouble() < 1 || id.toDouble() > 9007199254740991.0
       || std::floor(id.toDouble()) != id.toDouble()) return {{}, "Runner ID is missing or invalid."};
    r.runnerId = id.toInteger();
    QUrl url(obj.value("gitHubUrl").toString(), QUrl::StrictMode);
    if(!url.isValid() || url.scheme() != "https" || url.host() != "github.com"
       || !url.userInfo().isEmpty() || url.port() != -1 || url.hasQuery() || url.hasFragment())
        return {{}, "Only https://github.com owner/repository or organization URLs are supported."};
    auto parts = url.path().split('/', Qt::SkipEmptyParts);
    static const QRegularExpression segment("^[A-Za-z0-9_.-]+$");
    if(parts.size() < 1 || parts.size() > 2) return {{}, "Invalid repository or organization URL."};
    for(const auto& p : parts) if(!segment.match(p).hasMatch() || p == "." || p == "..")
        return {{}, "Invalid GitHub owner/repository."};
    r.owner = parts[0];
    if(parts.size() == 2) r.repository = parts[1];
    r.githubUrl = "https://github.com/" + parts.join('/');
    return {r, {}};
}
RegistrationResult readRegistration(const QString& input) {
    const QFileInfo root(input);
    if(!root.isDir()) return {{}, "Choose an existing runner installation directory."};
    const auto canonical = root.canonicalFilePath();
    QFileInfo config(QDir(canonical).filePath(".runner"));
    if(config.isSymLink() || !config.isFile() || config.size() > 65536)
        return {{}, ".runner must be a regular metadata file no larger than 64 KiB."};
    QFile file(config.filePath());
    if(!file.open(QIODevice::ReadOnly)) return {{}, "Cannot read .runner metadata."};
    auto result = parseRegistration(file.read(65537), canonical);
    if(!result.value) return result;
    QFileInfo hint(QDir(canonical).filePath(".service"));
    if(hint.exists()) {
        if(hint.isSymLink() || !hint.isFile() || hint.size() > 8192)
            return {{}, ".service must be a regular metadata file no larger than 8 KiB."};
        QFile service(hint.filePath());
        if(service.open(QIODevice::ReadOnly)) result.value->serviceHint = QString::fromUtf8(service.read(8193)).trimmed();
    }
    return result;
}
}
