#pragma once
#include "core/command.h"
#include <QPointer>
#include <QProcess>
#include <QTimer>

namespace runperch {
QString extractDeviceCode(const QByteArray& transcript);
class GitHubAuth final : public QObject {
    Q_OBJECT
public:
    GitHubAuth(CommandExecutor&, QString executable, QObject* parent=nullptr);
    ~GitHubAuth() override;
    QString login() const { return account; }
    QString detail() const { return message; }
    QString deviceCode() const { return code; }
    bool busy() const { return process && process->state()!=QProcess::NotRunning; }
    bool available() const { return !executable.isEmpty(); }
    void check();
    void begin();
    void cancel();
signals:
    void changed();
    void connected();
private:
    CommandExecutor& executor;
    QString executable;
    QString account, message, code;
    QPointer<QProcess> process;
    QByteArray transcript;
    bool cancelled=false;
    bool checking=false;
    bool verifyAfterLogin=false;
    quint64 generation=0;
};
}
