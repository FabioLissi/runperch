#include "ui/popover.h"
#include "ui/display.h"
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
QLabel* line(QString name) { auto* l=new QLabel; l->setObjectName(name); l->setTextFormat(Qt::PlainText); l->setWordWrap(true); return l; }
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
    setStyleSheet(R"(
        QFrame#runnerPopover { background:#fbfaf7; border:1px solid #cdd8cc; border-radius:10px; }
        QWidget { color:#19362d; font-size:13px; background:#fbfaf7; }
        QLabel { background:transparent; }
        QLabel#popoverBrand { font-size:18px; font-weight:700; }
        QLabel#popoverState { color:#12654f; font-weight:650; font-size:15px; }
        QLabel#popoverActivity { font-size:15px; font-weight:600; }
        QLabel#popoverLocal, QLabel#popoverTiming, QLabel#popoverObserved, QLabel#historyCaption { color:#66776d; font-size:11px; }
        QLabel#popoverCpu, QLabel#popoverMemory { font-size:15px; font-weight:650; }
        QComboBox { background:#fffefa; border:1px solid #cbd7cd; border-radius:5px; padding:7px; }
        QPushButton { background:#fffefa; border:1px solid #cbd7cd; border-radius:5px; padding:7px; }
        QPushButton:hover { background:#e5eee6; } QPushButton:disabled { color:#98a49b; }
        QPushButton#popoverStop { color:#a04d36; }
        QScrollArea { border:0; background:transparent; }
    )");
    auto* outer=new QVBoxLayout(this); outer->setContentsMargins(1,1,1,1);
    auto* scroll=new QScrollArea; scroll->setWidgetResizable(true); scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto* content=new QWidget; auto* column=new QVBoxLayout(content); column->setContentsMargins(18,16,18,16); column->setSpacing(12);
    auto* header=new QHBoxLayout; auto* brand=line("popoverBrand"); brand->setText("Runperch"); header->addWidget(brand); header->addStretch();
    auto* open=new QPushButton("Open overview ↗"); open->setObjectName("popoverOverview"); header->addWidget(open); column->addLayout(header);
    selector=new QComboBox; selector->setObjectName("popoverRunners"); selector->setAccessibleName("Runner"); selector->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon); selector->setMinimumContentsLength(10); column->addWidget(selector);
    state=line("popoverState"); activity=line("popoverActivity"); local=line("popoverLocal"); timing=line("popoverTiming");
    for(auto* l:{state,activity,local,timing}) column->addWidget(l);
    auto* charts=new QHBoxLayout; charts->setSpacing(16);
    auto* cpuColumn=new QVBoxLayout; auto* memoryColumn=new QVBoxLayout;
    cpu=line("popoverCpu"); memory=line("popoverMemory"); cpuChart=new MetricsChart(ResourceMetric::Cpu); memoryChart=new MetricsChart(ResourceMetric::Memory);
    cpuChart->setObjectName("cpuHistory"); memoryChart->setObjectName("memoryHistory");
    cpuColumn->addWidget(cpu); cpuColumn->addWidget(cpuChart); memoryColumn->addWidget(memory); memoryColumn->addWidget(memoryChart);
    charts->addLayout(cpuColumn,1); charts->addLayout(memoryColumn,1); column->addLayout(charts);
    auto* caption=line("historyCaption"); caption->setText("Last 15 minutes · recorded while open"); column->addWidget(caption);
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
    qApp->installEventFilter(this); resize(400,550); refresh();
}
const Runner* RunnerPopover::selected() const {
    for(const auto& r:monitor.runners()) if(r.registration.root==selectedRoot && r.instanceId==selectedInstance) return &r;
    return nullptr;
}
void RunnerPopover::present(QRect anchor) {
    refresh();
    setProperty("trayAnchor",anchor);
    if(anchor.isEmpty()) anchor=QRect(QCursor::pos(),QSize(1,1));
    auto* screen=QGuiApplication::screenAt(anchor.center()); if(!screen) screen=QGuiApplication::primaryScreen();
    if(screen) setGeometry(popoverGeometry(QSize(400,550),anchor,screen->availableGeometry()));
    show(); raise(); selector->setFocus(Qt::PopupFocusReason);
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
        selector->setCurrentIndex(selection); selector->setEnabled(selector->count()>1);
    }
    const auto* r=selected();
    if(!r) { state->setText("No runners configured"); activity->setText("Add an existing runner in the overview."); local->clear(); timing->clear(); observed->clear(); cpu->setText("CPU avg —"); memory->setText("Memory —"); cpuChart->setSamples({}); memoryChart->setSamples({}); start->setEnabled(false); stop->setEnabled(false); job->setEnabled(false); return; }
    const auto now=QDateTime::currentDateTimeUtc(); const bool fresh=isFresh(r->service.observedAt,now);
    const bool active=isFresh(r->remote.observedAt,now) && r->remote.job==JobState::Running;
    state->setText(status(*r));
    activity->setText(active && !r->remote.jobName.isEmpty() ? r->remote.jobName : text(localWorkState(r->service,now)));
    if(active && !r->remote.stepName.isEmpty()) activity->setText(activity->text()+"\n"+r->remote.stepName);
    local->setText("Service: "+text(fresh ? r->service.state : ServiceState::Unknown)+" · Connection: "+text(isFresh(r->remote.observedAt,now) ? r->remote.connection : ConnectionState::Unknown));
    local->setToolTip("Local service state and GitHub runner connection are separate. Connection can be unknown while read-only job information is available.");
    timing->setText("Uptime "+(fresh && r->service.uptimeSeconds ? duration(*r->service.uptimeSeconds) : "—")+" · Job "+(active ? elapsed(r->remote.startedAt) : "—"));
    const bool cpuValid=fresh && r->service.cpuCores && std::isfinite(*r->service.cpuCores) && *r->service.cpuCores>=0;
    cpu->setText(cpuValid ? QString("CPU avg %1 cores").arg(*r->service.cpuCores,0,'f',2) : "CPU avg —");
    memory->setText(fresh && r->service.rssBytes && *r->service.rssBytes>=0 ? QString("RSS %1 GiB").arg(*r->service.rssBytes/1073741824.0,0,'f',2) : "Memory —");
    cpuChart->setSamples(r->resources); memoryChart->setSamples(r->resources);
    observed->setText(r->serviceRefreshing ? "Checking local status…" : !r->service.observedAt.isValid() ? "Local status not observed" : "Local check "+r->service.observedAt.toLocalTime().toString("HH:mm:ss")+(fresh ? "" : " · stale"));
    observed->setToolTip("CPU is a decaying process average; RSS may double-count shared pages. No worker observed does not prove connected Idle.");
    const auto a=canControl(*r,ServiceAction::Start,now), b=canControl(*r,ServiceAction::Stop,now,StopAuthorization::ConfirmedImmediate);
    start->setEnabled(a.allowed); start->setToolTip(a.reason); stop->setEnabled(b.allowed); stop->setToolTip(b.reason);
    job->setEnabled(!currentJobUrl(*r,now).isEmpty()); job->setToolTip(job->isEnabled() ? "Open this active job in GitHub" : "A fresh active GitHub job observation is required; local worker activity has no job URL.");
}
}
