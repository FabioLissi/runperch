#pragma once
#include "core/monitor.h"
#include <QFrame>
class QComboBox;
class QLabel;
class QPushButton;
namespace runperch {
class MetricsChart;
QRect popoverGeometry(QSize size,QRect anchor,QRect available);
class RunnerPopover final : public QFrame {
    Q_OBJECT
public:
    explicit RunnerPopover(Monitor&,QWidget* parent=nullptr,Qt::WindowFlags flags=Qt::Popup);
    void present(QRect anchor);
    void refresh();
protected:
    bool eventFilter(QObject*,QEvent*) override;
    void keyPressEvent(QKeyEvent*) override;
signals:
    void overviewRequested();
    void controlRequested(QString root,quint64 instance,ServiceAction);
    void currentJobRequested(QString root,quint64 instance);
private:
    const Runner* selected() const;
    Monitor& monitor;
    QComboBox* selector;
    QLabel *state,*activity,*local,*timing,*cpu,*memory,*observed;
    MetricsChart *cpuChart,*memoryChart;
    QPushButton *start,*stop,*job;
    QString selectedRoot;
    quint64 selectedInstance=0;
};
}
