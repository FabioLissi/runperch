#include "ui/metrics_chart.h"
#include "ui/theme.h"
#include "core/resource_history.h"
#include <QPainter>
#include <QPainterPath>
#include <cmath>
namespace runperch {
ResourcePlot resourcePlot(const QList<ResourceSample>& samples,ResourceMetric metric,const QRectF& area,QDateTime now) {
    ResourcePlot out;
    const auto start=now.addSecs(-resourceHistoryWindowSeconds);
    auto value=[metric](const ResourceSample& s)->std::optional<double> {
        if(metric==ResourceMetric::Cpu) return s.cpuCores;
        return s.rssBytes ? std::optional<double>(*s.rssBytes/1073741824.0) : std::nullopt;
    };
    for(const auto& s:samples) if(s.observedAt>=start && s.observedAt<=now) {
        const auto v=value(s); if(v && std::isfinite(*v) && *v>=0) out.maximum=qMax(out.maximum,*v);
    }
    QDateTime previous; qint64 pid=0;
    for(const auto& s:samples) {
        if(!s.observedAt.isValid() || s.observedAt<start || s.observedAt>now) continue;
        const auto v=value(s);
        if(!v || !std::isfinite(*v) || *v<0) { previous={}; continue; }
        if(!previous.isValid() || s.observedAt<=previous || previous.secsTo(s.observedAt)>observationFreshnessSeconds || pid!=s.pid) out.segments.append(QPolygonF{});
        const auto x=area.left()+start.msecsTo(s.observedAt)/(resourceHistoryWindowSeconds*1000.0)*area.width();
        const auto y=area.bottom()-*v/out.maximum*area.height();
        out.segments.last().append(QPointF(x,y)); ++out.samples; previous=s.observedAt; pid=s.pid;
    }
    return out;
}
MetricsChart::MetricsChart(ResourceMetric m,QWidget* parent):QWidget(parent),metric(m) {
    setFixedHeight(uiMetrics.chartHeight);
    connect(&theme(),&ThemeController::changed,this,qOverload<>(&MetricsChart::update)); setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);
    setAccessibleName(metric==ResourceMetric::Cpu ? "CPU average history" : "Resident memory history");
    setToolTip("Recorded samples over the last 15 minutes while Runperch is open. Gaps mean unavailable observations or service restarts. Scale starts at zero; no smoothing or interpolation.");
}
void MetricsChart::setSamples(const QList<ResourceSample>& s) { samples=s; update(); }
void MetricsChart::paintEvent(QPaintEvent*) {
    QPainter p(this); p.setRenderHint(QPainter::Antialiasing);
    const QRectF plot(4,22,qMax(0,width()-8),qMax(0,height()-30));
    const auto data=resourcePlot(samples,metric,plot,QDateTime::currentDateTimeUtc());
    p.setPen(theme().tokens().muted); auto font=p.font(); font.setPixelSize(uiMetrics.caption); p.setFont(font);
    p.drawText(QRectF(4,0,width()-8,18),Qt::AlignRight,QString("0–%1 %2").arg(data.maximum,0,'f',2).arg(metric==ResourceMetric::Cpu ? "cores" : "GiB"));
    p.setPen(QPen(theme().tokens().border,1)); p.drawLine(plot.bottomLeft(),plot.bottomRight());
    if(!data.samples) { p.setPen(theme().tokens().muted); p.drawText(plot,Qt::AlignCenter,"Waiting for samples"); return; }
    const QColor color=metric==ResourceMetric::Cpu ? theme().tokens().accent : theme().tokens().memory;
    for(const auto& segment:data.segments) {
        if(segment.size()>1) {
            QPainterPath fill; fill.moveTo(segment.first().x(),plot.bottom());
            for(auto point:segment) fill.lineTo(point);
            fill.lineTo(segment.last().x(),plot.bottom()); fill.closeSubpath();
            auto shade=color; shade.setAlpha(theme().tokens().chartFillAlpha); p.fillPath(fill,shade);
            p.setPen(QPen(color,1.8)); p.drawPolyline(segment);
        }
        p.setPen(Qt::NoPen); p.setBrush(color);
        for(auto point:segment) p.drawEllipse(point,2,2);
    }
}
}
