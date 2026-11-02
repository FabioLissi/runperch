#pragma once
#include "core/github.h"
#include "platform/platform.h"
#include <QTimer>

namespace runperch {
class Monitor final : public QObject {
    Q_OBJECT
public:
    Monitor(Platform&, RemoteProvider&, QObject* parent = nullptr);
    bool add(const QString& directory, QString* error = nullptr);
    void remove(int index);
    const QList<Runner>& runners() const { return items; }
    QStringList directories() const;
    void refresh(bool localOnly = false);
    void setInterval(int seconds);
    void start();
    void control(int index, ServiceAction);
signals:
    void changed();
    void refreshFinished();
    void message(QString text, bool error);
private:
    void complete(const QString& root, quint64 generation, quint64 instanceId, bool remote, ServiceObservation, RemoteObservation);
    void remember(const Runner&);
    int find(const QString& root) const;
    Platform& platform;
    RemoteProvider& github;
    QList<Runner> items;
    QTimer timer;
    quint64 generation = 0;
    quint64 nextInstanceId = 0;
    int pending = 0;
};
}
