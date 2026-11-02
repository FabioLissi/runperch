#pragma once
#include "core/types.h"
#include <QWidget>
#include <QPolygonF>
namespace runperch {
enum class ResourceMetric { Cpu, Memory };
struct ResourcePlot { QList<QPolygonF> segments; double maximum=1; int samples=0; };
ResourcePlot resourcePlot(const QList<ResourceSample>&,ResourceMetric,const QRectF&,QDateTime now);
class MetricsChart final : public QWidget {
public:
    explicit MetricsChart(ResourceMetric,QWidget* parent=nullptr);
    void setSamples(const QList<ResourceSample>&);
protected:
    void paintEvent(QPaintEvent*) override;
private:
    ResourceMetric metric;
    QList<ResourceSample> samples;
};
}
