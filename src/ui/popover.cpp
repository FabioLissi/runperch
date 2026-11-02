#include "ui/themed_controls.h"
#include "ui/popover.h"
#include "ui/display.h"
#include "ui/theme.h"
#include <QPainter>
#include <QStyle>
#include "ui/metrics_chart.h"
#include <QComboBox>
#include <QApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QCursor>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QVBoxLayout>
#include <cmath>
namespace runperch {
namespace {
// Wrapped text may shrink below its longest word while retaining its natural
// preferred width and height-for-width. Ignored sizing would collapse labels
// beside stretches; hiding a scrollbar alone would clip oversized content.
class WrappedLabel final : public QLabel {
public:
    QSize minimumSizeHint() const override {
        auto size=QLabel::minimumSizeHint(); size.setWidth(0); return size;
    }
};
QLabel* line(QString name) { auto* l=new WrappedLabel; l->setObjectName(name); l->setTextFormat(Qt::PlainText); l->setWordWrap(true); return l; }
}
QRect popoverGeometry(QSize size,QRect anchor,QRect screen) {
    size=size.boundedTo(screen.size());
    const int x=qBound(screen.left(),anchor.center().x()-size.width()/2,screen.right()-size.width()+1);
    int y=anchor.bottom()+5;
    if(y+size.height()>screen.bottom()+1) y=anchor.top()-size.height()-4;
    y=qBound(screen.top(),y,screen.bottom()-size.height()+1);
    return QRect(QPoint(x,y),size);
}
RunnerPopover::RunnerPopover(Monitor& m,QWidget* parent,Qt::WindowFlags flags):QFrame(parent,flags),monitor(m) {
    setObjectName("runnerPopover"); setWindowTitle("Runperch quick view"); setAttribute(Qt::WA_QuitOnClose,false);
    setAttribute(Qt::WA_TranslucentBackground);
    connect(&theme(),&ThemeController::changed,this,[this] { refresh(); update(); });
    auto* outer=new QVBoxLayout(this); outer->setContentsMargins(1,1,1,1);
    auto* scroll=new QScrollArea; scroll->setObjectName("popoverScroll"); scroll->viewport()->setObjectName("popoverViewport"); scroll->setWidgetResizable(true); scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    content=new QWidget; content->setObjectName("popoverContent"); auto* column=new QVBoxLayout(content);
    column->setContentsMargins(uiMetrics.inset,uiMetrics.inset,uiMetrics.inset,uiMetrics.inset); column->setSpacing(uiMetrics.gap); column->setAlignment(Qt::AlignTop);
    auto* header=new QHBoxLayout; auto* brand=line("popoverBrand"); brand->setText("Runperch"); header->addWidget(brand); header->addStretch();
    auto* open=new QPushButton("Open overview ↗"); open->setObjectName("popoverOverview"); header->addWidget(open); column->addLayout(header);
    selector=new ThemedComboBox; selector->setObjectName("popoverRunners"); selector->setAccessibleName("Runner"); selector->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon); selector->setMinimumContentsLength(10); column->addWidget(selector);
    state=line("popoverState"); activity=line("popoverActivity"); local=line("popoverLocal"); timing=line("popoverTiming");
    auto* statusRow=new QHBoxLayout; statusRow->addWidget(state); statusRow->addStretch(); statusRow->addWidget(local);
    column->addLayout(statusRow); column->addWidget(activity); column->addWidget(timing);
    auto* charts=new QHBoxLayout; charts->setSpacing(uiMetrics.gap);
    auto* cpuCard=new QFrame; cpuCard->setObjectName("metricCard"); auto* memoryCard=new QFrame; memoryCard->setObjectName("metricCard");
    auto* cpuColumn=new QVBoxLayout(cpuCard); auto* memoryColumn=new QVBoxLayout(memoryCard);
    for(auto* layout:{cpuColumn,memoryColumn}) { layout->setContentsMargins(uiMetrics.gap,uiMetrics.gap,uiMetrics.gap,uiMetrics.gap); layout->setSpacing(0); }
    cpu=line("popoverCpu"); memory=line("popoverMemory"); cpuChart=new MetricsChart(ResourceMetric::Cpu); memoryChart=new MetricsChart(ResourceMetric::Memory);
    cpuChart->setObjectName("cpuHistory"); memoryChart->setObjectName("memoryHistory");
    cpuColumn->addWidget(cpu); cpuColumn->addWidget(cpuChart); memoryColumn->addWidget(memory); memoryColumn->addWidget(memoryChart);
    charts->addWidget(cpuCard,1); charts->addWidget(memoryCard,1); column->addLayout(charts);
    auto* caption=line("historyCaption"); caption->setText("15-minute history · fills while Runperch runs"); column->addWidget(caption);
    observed=line("popoverObserved"); column->addWidget(observed);
    job=new QPushButton("Open current job ↗"); job->setObjectName("popoverJob"); column->addWidget(job);
    auto* controls=new QHBoxLayout; start=new QPushButton("Start service"); stop=new QPushButton("Stop now…");
    start->setObjectName("popoverStart"); stop->setObjectName("popoverStop"); controls->addWidget(start); controls->addWidget(stop); column->addLayout(controls);
    scroll->setWidget(content); outer->addWidget(scroll);
    connect(open,&QPushButton::clicked,this,[this] { hide(); emit overviewRequested(); });
    connect(selector,&QComboBox::currentIndexChanged,this,[this](int i) {
        if(i<0 || i>=monitor.runners().size()) return;
        const auto& r=monitor.runners()[i]; selectedRoot=r.registration.root; selectedInstance=r.instanceId; refresh();
    });
    connect(start,&QPushButton::clicked,this,[this] { if(auto* r=selected()) { auto root=r->registration.root; auto id=r->instanceId; hide(); emit controlRequested(root,id,ServiceAction::Start); } });
    connect(stop,&QPushButton::clicked,this,[this] { if(auto* r=selected()) { auto root=r->registration.root; auto id=r->instanceId; hide(); emit controlRequested(root,id,ServiceAction::Stop); } });
    connect(job,&QPushButton::clicked,this,[this] { if(auto* r=selected()) emit currentJobRequested(r->registration.root,r->instanceId); });
    connect(&monitor,&Monitor::changed,this,&RunnerPopover::refresh);
    qApp->installEventFilter(this); resize(uiMetrics.popoverWidth,uiMetrics.popoverMaxHeight); refresh();
}
const Runner* RunnerPopover::selected() const {
    for(const auto& r:monitor.runners()) if(r.registration.root==selectedRoot && r.instanceId==selectedInstance) return &r;
    return nullptr;
}
void RunnerPopover::fitContent(QRect anchor) {
    if(anchor.isEmpty()) anchor=QRect(QCursor::pos(),QSize(1,1));
    auto* screen=QGuiApplication::screenAt(anchor.center()); if(!screen) screen=QGuiApplication::primaryScreen();
    if(!screen) return;
    ensurePolished(); content->ensurePolished(); content->layout()->activate();
    const auto available=screen->availableGeometry(); const int width=qMin(uiMetrics.popoverWidth,available.width());
    int height=content->layout()->totalHeightForWidth(width-2)+2;
    if(height<2) height=content->sizeHint().height()+2;
    if(height>qMin(uiMetrics.popoverMaxHeight,available.height())) {
        height=content->layout()->totalHeightForWidth(width-2-style()->pixelMetric(QStyle::PM_ScrollBarExtent))+2;
    }
    setGeometry(popoverGeometry(QSize(width,qMin(height,uiMetrics.popoverMaxHeight)),anchor,available));
}
void RunnerPopover::present(QRect anchor) {
    refresh(); setProperty("trayAnchor",anchor); fitContent(anchor);
    show(); raise(); selector->setFocus(Qt::PopupFocusReason);
}
void RunnerPopover::paintEvent(QPaintEvent*) {
    QPainter painter(this); painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(theme().tokens().border); painter.setBrush(theme().tokens().background);
    painter.drawRoundedRect(QRectF(rect()).adjusted(0.5,0.5,-0.5,-0.5),uiMetrics.panelRadius,uiMetrics.panelRadius);
}
void RunnerPopover::keyPressEvent(QKeyEvent* event) {
    if(event->key()==Qt::Key_Escape) { hide(); event->accept(); return; }
    QFrame::keyPressEvent(event);
}
bool RunnerPopover::eventFilter(QObject* object,QEvent* event) {
    if(isVisible() && event->type()==QEvent::MouseButtonPress) {
        const auto* widget=qobject_cast<QWidget*>(object);
        const auto* active=QApplication::activePopupWidget();
        const auto point=static_cast<QMouseEvent*>(event)->globalPosition().toPoint();
        if(widget && widget!=this && !isAncestorOf(widget) && !(active && isAncestorOf(active))
           && !property("trayAnchor").toRect().contains(point)) hide();
    }
    return QFrame::eventFilter(object,event);
}
void RunnerPopover::refresh() {
    bool changed=selector->count()!=monitor.runners().size();
    for(int i=0;!changed && i<selector->count();++i) {
        const auto& r=monitor.runners()[i];
        changed=selector->itemText(i)!=r.registration.name || selector->itemData(i).toString()!=r.registration.root
            || selector->itemData(i,Qt::UserRole+1).toULongLong()!=r.instanceId;
    }
    if(changed) {
        QSignalBlocker blocker(selector); selector->clear(); int selection=-1;
        for(const auto& r:monitor.runners()) {
            selector->addItem(r.registration.name,r.registration.root);
            selector->setItemData(selector->count()-1,QVariant::fromValue(r.instanceId),Qt::UserRole+1);
            if(r.registration.root==selectedRoot && r.instanceId==selectedInstance) selection=selector->count()-1;
        }
        if(selection<0 && !monitor.runners().isEmpty()) { selection=0; selectedRoot=monitor.runners()[0].registration.root; selectedInstance=monitor.runners()[0].instanceId; }
        selector->setCurrentIndex(selection); selector->setEnabled(selector->count()>0);
    }
    const auto* r=selected();
    if(!r) { state->setText("No runners configured"); activity->setText("Add an existing runner in the overview."); local->clear(); timing->clear(); observed->clear(); cpu->setText("CPU avg —"); memory->setText("Memory —"); cpuChart->setSamples({}); memoryChart->setSamples({}); start->setEnabled(false); stop->setEnabled(false); job->setEnabled(false); return; }
    const auto now=QDateTime::currentDateTimeUtc(); const bool fresh=isFresh(r->service.observedAt,now);
    const bool active=isFresh(r->remote.observedAt,now) && r->remote.job==JobState::Running;
    state->setText(status(*r));
    const auto tone=statusTone(status(*r));
    if(state->property("tone").toString()!=tone) { state->setProperty("tone",tone); state->style()->unpolish(state); state->style()->polish(state); state->update(); }
    activity->setText(active && !r->remote.jobName.isEmpty() ? r->remote.jobName : text(localWorkState(r->service,now)));
    if(active && !r->remote.stepName.isEmpty()) activity->setText(activity->text()+"\n"+r->remote.stepName);
    local->setText("Connection: "+text(isFresh(r->remote.observedAt,now) ? r->remote.connection : ConnectionState::Unknown));
    local->setToolTip("Local service state and GitHub runner connection are separate. Connection can be unknown while read-only job information is available.");
    timing->setText("Uptime "+(fresh && r->service.uptimeSeconds ? duration(*r->service.uptimeSeconds) : "—")+(active ? " · Job "+elapsed(r->remote.startedAt) : ""));
    const bool cpuValid=fresh && r->service.cpuCores && std::isfinite(*r->service.cpuCores) && *r->service.cpuCores>=0;
    cpu->setText(cpuValid ? QString("CPU avg %1 cores").arg(*r->service.cpuCores,0,'f',2) : "CPU avg —");
    memory->setText(fresh && r->service.rssBytes && *r->service.rssBytes>=0 ? QString("RSS %1 GiB").arg(*r->service.rssBytes/1073741824.0,0,'f',2) : "Memory —");
    cpuChart->setSamples(r->resources); memoryChart->setSamples(r->resources);
    observed->setText(r->serviceRefreshing ? "Checking local status…" : !r->service.observedAt.isValid() ? "Local status not observed" : "Local check "+r->service.observedAt.toLocalTime().toString("HH:mm:ss")+(fresh ? "" : " · stale"));
    observed->setToolTip("CPU is a decaying process average; RSS may double-count shared pages. No worker observed does not prove connected Idle.");
    const auto a=canControl(*r,ServiceAction::Start,now), b=canControl(*r,ServiceAction::Stop,now,StopAuthorization::ConfirmedImmediate);
    start->setEnabled(a.allowed); start->setToolTip(a.reason); stop->setEnabled(b.allowed); stop->setToolTip(b.reason);
    if(isVisible()) fitContent(property("trayAnchor").toRect());
    job->setEnabled(!currentJobUrl(*r,now).isEmpty()); job->setToolTip(job->isEnabled() ? "Open this active job in GitHub" : "A fresh active GitHub job observation is required; local worker activity has no job URL.");
}
}
