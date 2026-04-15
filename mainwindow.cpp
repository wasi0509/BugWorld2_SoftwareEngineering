#include "mainwindow.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMessageBox>

MainWindow::MainWindow(const std::string& world,
                       const std::string& bug1,
                       const std::string& bug2,
                       int ticks, int fps,
                       QWidget* parent)
    : QMainWindow(parent)
    , m_worker(nullptr), m_thread(nullptr)
{
    Q_UNUSED(fps)
    setupUi();
    startSimulator(world, bug1, bug2, ticks);
}

MainWindow::~MainWindow()
{
    if (m_worker) m_worker->stop();
    if (m_thread) { m_thread->quit(); m_thread->wait(2000); }
}

void MainWindow::setupUi()
{
    setWindowTitle("Bug World Simulator");
    resize(900, 700);

    QWidget* central = new QWidget(this);
    setCentralWidget(central);
    central->setStyleSheet("background-color: #1a1a1a;");

    QVBoxLayout* mainLayout = new QVBoxLayout(central);

    // Cycle display at the top
    m_cycleLabel = new QLabel("Cycle: --", this);
    m_cycleLabel->setStyleSheet("color: white; font-size: 14px; font-weight: bold;");
    mainLayout->addWidget(m_cycleLabel);

    // Main grid display
    m_grid = new GridWidget(this);
    mainLayout->addWidget(m_grid, 1);

    // Bottom bar: stats and trace length control
    QHBoxLayout* bottom = new QHBoxLayout();

    m_statsLabel = new QLabel("Stats: --", this);
    m_statsLabel->setStyleSheet("color: #aaaaaa; font-size: 12px;");
    bottom->addWidget(m_statsLabel, 1);

    QLabel* traceLabel = new QLabel("Trace length (N):", this);
    traceLabel->setStyleSheet("color: white;");
    bottom->addWidget(traceLabel);

    m_traceSpinBox = new QSpinBox(this);
    m_traceSpinBox->setRange(0, 100);
    m_traceSpinBox->setValue(5);
    m_traceSpinBox->setFixedWidth(80);
    bottom->addWidget(m_traceSpinBox);

    mainLayout->addLayout(bottom);

    connect(m_traceSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &MainWindow::onTraceNChanged);
}

void MainWindow::startSimulator(const std::string& world,
                                 const std::string& bug1,
                                 const std::string& bug2,
                                 int ticks)
{
    qRegisterMetaType<WorldState>("WorldState");

    m_thread = new QThread(this);
    m_worker = new SimulatorWorker(world, bug1, bug2, ticks);
    m_worker->moveToThread(m_thread);

    connect(m_thread, &QThread::started,            m_worker, &SimulatorWorker::run);
    connect(m_worker, &SimulatorWorker::frameReady, this,     &MainWindow::onFrameReady);
    connect(m_worker, &SimulatorWorker::errorOccurred, this,  &MainWindow::onSimulatorError);
    connect(m_worker, &SimulatorWorker::finished,   m_thread, &QThread::quit);
    connect(m_worker, &SimulatorWorker::finished,   m_worker, &QObject::deleteLater);
    connect(m_thread, &QThread::finished,           m_thread, &QObject::deleteLater);

    m_thread->start();
}

void MainWindow::onFrameReady(WorldState state)
{
    m_cycleLabel->setText(QString("Cycle: %1  |  Map: %2 x %3")
        .arg(state.cycle).arg(state.rows).arg(state.cols));

    m_statsLabel->setText(
        QString("Red alive: %1  |  Black alive: %2  |  Red food: %3  |  Black food: %4")
        .arg(state.redAlive).arg(state.blackAlive)
        .arg(state.redFood).arg(state.blackFood));

    m_grid->onFrameReady(state);
}

void MainWindow::onSimulatorError(QString message)
{
    QMessageBox::critical(this, "Simulator Error", message);
    close();
}

void MainWindow::onTraceNChanged(int value)
{
    m_grid->setTraceN(value);
}