#include "ui/window.h"
#include "core/catalog.h"
#include "runperch_version.h"
#include <QApplication>
#include <QCloseEvent>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStatusBar>
#include <QTableWidget>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

namespace runperch {
namespace {
QLabel* label(const QString& value, const QString& name={}) {
    auto* l=new QLabel(value); l->setWordWrap(true); l->setTextFormat(Qt::PlainText);
    l->setObjectName(name); l->setTextInteractionFlags(Qt::TextSelectableByMouse); return l;
}
QString elapsed(QDateTime start) {
    if(!start.isValid()) return "—";
    auto seconds=qMax<qint64>(0,start.secsTo(QDateTime::currentDateTimeUtc()));
    return seconds<60 ? QString("%1s").arg(seconds) : QString("%1m %2s").arg(seconds/60).arg(seconds%60);
}
QColor stateColor(const QString& state) {
    if(state=="Running") return QColor("#a65d06");
    if(state=="Idle") return QColor("#24754c");
    return QColor("#66716b");
}
}
Window::Window(Monitor& m, Platform& p, QWidget* parent) : QMainWindow(parent),monitor(m),platform(p) {
    setWindowTitle("Runperch"); resize(1120,780); setMinimumSize(760,600);
    setWindowIcon(QIcon(":/assets/logo.png"));
    auto* central=new QWidget; setCentralWidget(central);
    auto* row=new QHBoxLayout(central); row->setContentsMargins(0,0,0,0); row->setSpacing(0);
    auto* side=new QWidget; side->setObjectName("sidebar"); side->setFixedWidth(185);
    auto* nav=new QVBoxLayout(side); nav->setContentsMargins(18,28,18,18);
    auto* logo=label("", "logo"); logo->setPixmap(QPixmap(":/assets/logo.png").scaled(54,54,Qt::KeepAspectRatio,Qt::SmoothTransformation));
    nav->addWidget(logo); nav->addWidget(label("Runperch","brand")); nav->addWidget(label("Your runners, in view.","muted")); nav->addSpacing(30);
    auto* overview=new QPushButton("Overview"); overview->setObjectName("selectedNav"); nav->addWidget(overview);
    auto* historyButton=new QPushButton("History"); nav->addWidget(historyButton);
    auto* settingsButton=new QPushButton("Settings"); nav->addWidget(settingsButton);
    nav->addStretch(); nav->addWidget(label("This computer","muted")); nav->addWidget(label(platform.description(),"muted"));
    nav->addSpacing(12); nav->addWidget(label(QString("Version %1").arg(RUNPERCH_VERSION),"muted")); row->addWidget(side);
    auto* content=new QWidget; auto* column=new QVBoxLayout(content); column->setContentsMargins(25,25,25,12); column->setSpacing(12);
    auto* heading=new QHBoxLayout; heading->addWidget(label("Runners","heading")); heading->addStretch();
    auto* refresh=new QPushButton("Refresh"); heading->addWidget(refresh);
    auto* add=new QPushButton("+ Add existing runner"); heading->addWidget(add); column->addLayout(heading);
    summary=label("No runners added","muted"); column->addWidget(summary);
    table=new QTableWidget(0,4); table->setHorizontalHeaderLabels({"Runner","Status","Current job","Elapsed"});
    table->setObjectName("runnerTable");
    table->setSelectionBehavior(QAbstractItemView::SelectRows); table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers); table->setShowGrid(false);
    table->verticalHeader()->hide(); table->verticalHeader()->setDefaultSectionSize(50);
    table->horizontalHeader()->setSectionResizeMode(0,QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(1,QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(2,QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(3,QHeaderView::ResizeToContents);
    table->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft|Qt::AlignVCenter);
    table->setMinimumHeight(135); table->setMaximumHeight(220); column->addWidget(table);
    auto* detailHeader=new QHBoxLayout; title=label("Add your first runner","heading"); detailHeader->addWidget(title,1);
    githubButton=new QPushButton("Open in GitHub ↗"); detailHeader->addWidget(githubButton); column->addLayout(detailHeader);
    identity=label("Choose a configured GitHub Actions runner folder. Its registration and service setup stay intact.","muted"); column->addWidget(identity);
    states=label(""); states->setObjectName("runnerStates"); column->addWidget(states);
    column->addWidget(label("LATEST REPORTED ACTIVITY","caption")); activity=label("Waiting for an observation","activity"); column->addWidget(activity);
    resources=label("","metrics"); resources->setMinimumHeight(65); column->addWidget(resources);
    explanation=label("Runperch reads status only. Start and stop always require an explicit action.","muted"); column->addWidget(explanation);
    observed=label("","muted"); column->addWidget(observed);
    column->addStretch(1);
    auto* controls=new QHBoxLayout;
    auto* logs=new QPushButton("Open logs folder"); controls->addWidget(logs);
    auto* remove=new QPushButton("Remove from Runperch"); controls->addWidget(remove); controls->addStretch();
    startButton=new QPushButton("Start service"); controls->addWidget(startButton);
    stopButton=new QPushButton("Stop service…"); controls->addWidget(stopButton); column->addLayout(controls);
    column->addWidget(label("Monitoring existing runners · Closing Runperch does not stop them","footer"));
    row->addWidget(content,1);
    setStyleSheet(R"(
        QMainWindow, QWidget { background:#fbfaf7; color:#19362d; font-size:13px; }
        QLabel { background:transparent; }
        QWidget#sidebar { background:#efeee8; border-right:1px solid #dedfd7; }
        QLabel#brand { font-size:26px; font-weight:700; }
        QLabel#heading { font-size:23px; font-weight:650; }
        QLabel#activity { font-size:19px; font-weight:600; }
        QLabel#muted, QLabel#caption { color:#64736b; }
        QLabel#caption { font-size:11px; }
        QLabel#metrics { border-top:1px solid #dedfd7; border-bottom:1px solid #dedfd7; padding:12px 0; font-size:15px; }
        QLabel#footer { color:#718077; font-size:11px; border-top:1px solid #dedfd7; padding-top:12px; }
        QPushButton { border:1px solid #cbd2c9; border-radius:5px; padding:8px 10px; background:#fffefa; }
        QPushButton:hover { background:#e7eee6; }
        QPushButton:disabled { color:#969f98; background:#f1f0eb; }
        QWidget#sidebar QPushButton { text-align:left; border:none; padding:11px 10px; background:transparent; }
        QWidget#sidebar QPushButton#selectedNav { background:#d9e6dc; font-weight:600; }
        QTableWidget { border:1px solid #dedfd7; border-radius:5px; background:#fffefa; selection-background-color:#dce8de; selection-color:#16372c; }
        QHeaderView::section { background:#f3f3ec; border:none; border-bottom:1px solid #dedfd7; padding:8px; text-align:left; }
        QMenu { background:#fbfaf7; border:1px solid #d2d8cf; padding:6px; }
        QMenu::item { padding:7px 22px; } QMenu::item:selected { background:#dce8de; }
        QSpinBox { padding:6px; background:white; }
    )");
    menu=new QMenu(this); tray=new QSystemTrayIcon(this);
    QIcon trayIcon(":/assets/logo.png"); trayIcon.setIsMask(true); tray->setIcon(trayIcon); tray->setContextMenu(menu);
    if(QSystemTrayIcon::isSystemTrayAvailable()) tray->show();
    connect(tray,&QSystemTrayIcon::activated,this,[this](QSystemTrayIcon::ActivationReason reason) {
        if(reason==QSystemTrayIcon::DoubleClick || reason==QSystemTrayIcon::Trigger) { show(); raise(); activateWindow(); }
    });
    connect(menu,&QMenu::aboutToShow,this,&Window::trayMenu);
    connect(add,&QPushButton::clicked,this,&Window::addRunner);
    connect(refresh,&QPushButton::clicked,this,[this] { monitor.refresh(); });
    connect(historyButton,&QPushButton::clicked,this,&Window::history);
    connect(settingsButton,&QPushButton::clicked,this,&Window::settings);
    connect(table,&QTableWidget::itemSelectionChanged,this,[this] {
        int i=table->currentRow(); if(i>=0 && i<monitor.runners().size()) selectedRoot=monitor.runners()[i].registration.root;
        details();
    });
    connect(githubButton,&QPushButton::clicked,this,[this] {
        int i=table->currentRow(); if(i>=0 && i<monitor.runners().size()) {
            auto& r=monitor.runners()[i]; QDesktopServices::openUrl(QUrl(r.remote.jobUrl.isEmpty() ? r.registration.githubUrl : r.remote.jobUrl));
        }
    });
    connect(logs,&QPushButton::clicked,this,[this] {
        int i=table->currentRow(); if(i>=0 && i<monitor.runners().size())
            QDesktopServices::openUrl(QUrl::fromLocalFile(QDir(monitor.runners()[i].registration.root).filePath("_diag")));
    });
    connect(remove,&QPushButton::clicked,this,[this] { monitor.remove(table->currentRow()); persist(); });
    connect(startButton,&QPushButton::clicked,this,[this] { control(ServiceAction::Start); });
    connect(stopButton,&QPushButton::clicked,this,[this] { control(ServiceAction::Stop); });
    connect(&monitor,&Monitor::changed,this,&Window::rebuild);
    connect(&monitor,&Monitor::message,this,[this](const QString& message,bool error) {
        if(error) QMessageBox::warning(this,"Runperch",message); else statusBar()->showMessage(message,8000);
    });
    auto* clock=new QTimer(this); clock->setInterval(1000);
    connect(clock,&QTimer::timeout,this,[this] {
        bool expired=false;
        for(int i=0;i<monitor.runners().size();++i) {
            if(auto* item=table->item(i,1)) expired |= item->text()!=status(monitor.runners()[i]);
        }
        if(expired) rebuild(); else details();
    }); clock->start();
    rebuild();
}
void Window::persist() { saveRunnerDirectories(monitor.directories()); }
void Window::addRunner() {
    auto root=QFileDialog::getExistingDirectory(this,"Choose an existing GitHub Actions runner installation");
    if(root.isEmpty()) return;
    QString error; if(!monitor.add(root,&error)) { QMessageBox::warning(this,"Runner not added",error); return; }
    selectedRoot=QFileInfo(root).canonicalFilePath(); persist(); monitor.refresh();
}
void Window::rebuild() {
    QSignalBlocker blocker(table);
    table->setRowCount(monitor.runners().size()); int selected=-1; int running=0,idle=0,stopped=0,unknown=0;
    for(int i=0;i<monitor.runners().size();++i) {
        const auto& r=monitor.runners()[i]; const auto state=status(r);
        running += state=="Running"; idle += state=="Idle"; stopped += state=="Stopped"; unknown += state=="Unknown" || state=="Offline";
        QStringList values{r.registration.name,state,r.remote.jobName.isEmpty() ? r.remote.detail : r.remote.jobName,elapsed(r.remote.startedAt)};
        for(int c=0;c<values.size();++c) {
            auto* item=new QTableWidgetItem(values[c]); item->setToolTip(values[c]);
            if(c==1) item->setForeground(stateColor(state)); table->setItem(i,c,item);
        }
        if(r.registration.root==selectedRoot) selected=i;
    }
    if(selected<0 && !monitor.runners().isEmpty()) selected=0;
    if(selected>=0) { table->selectRow(selected); selectedRoot=monitor.runners()[selected].registration.root; }
    summary->setText(QString("%1 running   ·   %2 idle   ·   %3 stopped   ·   %4 unknown / offline").arg(running).arg(idle).arg(stopped).arg(unknown));
    details(); trayMenu();
}
void Window::details() {
    int i=table->currentRow();
    if(i<0 || i>=monitor.runners().size()) {
        title->setText("Add your first runner"); identity->setText("Choose an existing runner installation with + Add existing runner.");
        states->clear(); activity->setText("No runner selected"); resources->clear(); observed->clear();
        startButton->setEnabled(false); stopButton->setEnabled(false); githubButton->setEnabled(false); return;
    }
    const auto& r=monitor.runners()[i];
    title->setText(r.remote.jobName.isEmpty() ? r.registration.name : r.remote.jobName);
    identity->setText(r.registration.githubUrl + (r.remote.workflowName.isEmpty() ? "" : " · " + r.remote.workflowName));
    states->setText("Service: " + text(isFresh(r.service.observedAt) ? r.service.state : ServiceState::Unknown)
       + "     GitHub: " + text(isFresh(r.remote.observedAt) ? r.remote.connection : ConnectionState::Unknown)
       + "     Job: " + text(isFresh(r.remote.observedAt) ? r.remote.job : JobState::Unknown));
    activity->setText(!r.remote.stepName.isEmpty() ? r.remote.stepName : !r.remote.jobName.isEmpty() ? r.remote.jobName : r.remote.detail.isEmpty() ? "Waiting for an observation" : r.remote.detail);
    QString cpu=r.service.cpuCores ? QString::number(*r.service.cpuCores,'f',2) + " cores" : "—";
    QString memory=r.service.rssBytes ? QString::number(*r.service.rssBytes/1073741824.0,'f',2) + " GB" : "—";
    QString disk=r.service.freeDiskBytes ? QString::number(*r.service.freeDiskBytes/1073741824.0,'f',0) + " GB" : "—";
    resources->setText("CPU: " + cpu + "      RSS: " + memory + "      Free disk: " + disk + "      Elapsed: " + elapsed(r.remote.startedAt));
    resources->setToolTip("CPU is the process tree's aggregate core usage; RSS sums resident memory and can count shared pages more than once. Free disk is the runner installation volume.");
    explanation->setText(r.service.detail + "\n" + r.remote.detail);
    auto stamp=[](QDateTime t) { return t.isValid() ? t.toLocalTime().toString("HH:mm:ss") + (isFresh(t) ? "" : " (stale)") : "not observed"; };
    observed->setText((r.refreshing ? "Refreshing…  " : "") + QString("Service: %1 · GitHub: %2").arg(stamp(r.service.observedAt),stamp(r.remote.observedAt)));
    auto start=canControl(r,ServiceAction::Start,QDateTime::currentDateTimeUtc());
    auto stop=canControl(r,ServiceAction::Stop,QDateTime::currentDateTimeUtc());
    startButton->setEnabled(start.allowed); startButton->setToolTip(start.reason);
    stopButton->setEnabled(stop.allowed); stopButton->setToolTip(stop.reason);
    githubButton->setEnabled(true);
}
void Window::trayMenu() {
    if(menu->isVisible()) {
        for(auto* action : menu->actions()) if(!action->data().toString().isEmpty()) {
            bool found=false;
            for(const auto& r : monitor.runners()) if(r.registration.root==action->data().toString()) {
                action->setText(r.registration.name + "   ·   " + status(r)); found=true; break;
            }
            if(!found) { action->setText("Runner removed"); action->setEnabled(false); }
        }
        return;
    }
    menu->clear(); auto* heading=menu->addAction("Runperch · This computer"); heading->setEnabled(false); menu->addSeparator();
    for(const auto& r : monitor.runners()) {
        auto* action=menu->addAction(r.registration.name + "   ·   " + status(r));
        action->setData(r.registration.root);
        connect(action,&QAction::triggered,this,[this,root=r.registration.root] { selectedRoot=root; rebuild(); show(); raise(); activateWindow(); });
    }
    if(monitor.runners().isEmpty()) { auto* a=menu->addAction("No runners configured"); a->setEnabled(false); }
    menu->addSeparator();
    menu->addAction("Open Runperch",this,[this] { show(); raise(); activateWindow(); });
    menu->addAction("Add existing runner…",this,&Window::addRunner);
    menu->addAction("Refresh",this,[this] { monitor.refresh(); });
    menu->addAction("Settings…",this,&Window::settings);
    menu->addSeparator(); menu->addAction("Quit Runperch",qApp,&QApplication::quit);
    tray->setToolTip(QString("Runperch · %1 runner(s)").arg(monitor.runners().size()));
}
void Window::control(ServiceAction action) {
    const auto i=table->currentRow(); if(i<0 || i>=monitor.runners().size()) return;
    const auto root=monitor.runners()[i].registration.root;
    const auto name=monitor.runners()[i].registration.name;
    const QString warning=action==ServiceAction::Stop
        ? "Stop this runner service? GitHub currently reports it idle, but a new assignment can race this check. Stopping the service can interrupt a job. This is not drain-after-job."
        : "Start the existing runner service using its current configuration?";
    if(QMessageBox::question(this,action==ServiceAction::Stop ? "Stop " + name : "Start " + name,
        warning,QMessageBox::Yes|QMessageBox::Cancel,QMessageBox::Cancel)!=QMessageBox::Yes) return;
    // Modal interaction may have refreshed/reordered the table: resolve identity again.
    for(int n=0;n<monitor.runners().size();++n) if(monitor.runners()[n].registration.root==root) { monitor.control(n,action); break; }
}
void Window::settings() {
    QDialog dialog(this); dialog.setWindowTitle("Runperch settings"); auto* layout=new QVBoxLayout(&dialog);
    auto* form=new QFormLayout; auto* seconds=new QSpinBox; seconds->setRange(5,3600); seconds->setSuffix(" seconds"); seconds->setValue(savedRefreshInterval());
    form->addRow("Refresh interval",seconds); layout->addLayout(form);
    layout->addWidget(label("GitHub access uses your existing gh authentication. Run gh auth login in Terminal if needed. No credentials are copied into Runperch."));
    layout->addWidget(label("Runperch is licensed under Apache 2.0. Qt is dynamically linked under its open-source licenses. See THIRD_PARTY_NOTICES.md for distribution requirements."));
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel); layout->addWidget(buttons);
    connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept); connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()==QDialog::Accepted) { saveRefreshInterval(seconds->value()); monitor.setInterval(seconds->value()); }
}
void Window::history() {
    QDialog dialog(this); dialog.setWindowTitle("Observed jobs"); dialog.resize(850,380); auto* layout=new QVBoxLayout(&dialog);
    layout->addWidget(label("Jobs seen while Runperch was open. This is not a complete GitHub execution history; completion is not inferred from disappearance.","muted"));
    const auto records=QJsonDocument::fromJson(QSettings().value("history").toByteArray()).array();
    auto* jobs=new QTableWidget(records.size(),3); jobs->setHorizontalHeaderLabels({"Runner","Job","Last seen"});
    jobs->setEditTriggers(QAbstractItemView::NoEditTriggers); jobs->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    for(int i=0;i<records.size();++i) { auto o=records[i].toObject(); QStringList v{o["runner"].toString(),o["job"].toString(),o["last_seen"].toString()};
        for(int c=0;c<v.size();++c) jobs->setItem(i,c,new QTableWidgetItem(v[c])); }
    layout->addWidget(jobs); dialog.exec();
}
void Window::closeEvent(QCloseEvent* event) {
    if(tray->isVisible()) { hide(); event->ignore(); } else QMainWindow::closeEvent(event);
}
}
