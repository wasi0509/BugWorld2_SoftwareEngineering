#pragma once

#include <QObject>
#include <string>
#include <sys/types.h>
#include "parser.h"  // WorldState and parseResponse live here

// Required so WorldState can be passed between threads via Qt signals.
Q_DECLARE_METATYPE(WorldState)

class SimulatorWorker : public QObject
{
    Q_OBJECT

public:
    explicit SimulatorWorker(const std::string& world,
                              const std::string& bug1,
                              const std::string& bug2,
                              int ticks,
                              QObject* parent = nullptr);
    ~SimulatorWorker();

public slots:
    void run();
    void stop();

signals:
    void frameReady(WorldState state);
    void finished();
    void errorOccurred(QString message);

private:
    std::string m_world;
    std::string m_bug1;
    std::string m_bug2;
    int         m_ticks;
    bool        m_running;
    pid_t       m_pid;
    int         m_cmdFd;
    int         m_dataFd;
    std::string m_tmpdir;

    bool        startSimulator();
    std::string readToEnd();
    WorldState  parseResponse(const std::string& response);
    void        cleanup();
};