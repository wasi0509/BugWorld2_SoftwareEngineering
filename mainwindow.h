#pragma once

#include <QMainWindow>
#include <QThread>
#include <QLabel>
#include <QSpinBox>
#include "gridwidget.h"
#include "simulator.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(const std::string& world,
                        const std::string& bug1,
                        const std::string& bug2,
                        int ticks,
                        int fps,
                        QWidget* parent = nullptr);
    ~MainWindow();

private slots:
    void onFrameReady(WorldState state);
    void onSimulatorError(QString message);
    void onTraceNChanged(int value);

private:
    GridWidget*      m_grid;
    QLabel*          m_cycleLabel;
    QLabel*          m_statsLabel;
    QSpinBox*        m_traceSpinBox;
    SimulatorWorker* m_worker;
    QThread*         m_thread;

    void setupUi();
    void startSimulator(const std::string& world,
                        const std::string& bug1,
                        const std::string& bug2,
                        int ticks);
};