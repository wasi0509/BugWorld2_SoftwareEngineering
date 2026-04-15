#include "simulator.h"
#include "parser.h"   // parseResponse free function
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sstream>
#include <algorithm>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <iostream>

using std::string;

SimulatorWorker::SimulatorWorker(const string& world,
                                  const string& bug1,
                                  const string& bug2,
                                  int ticks,
                                  QObject* parent)
    : QObject(parent)
    , m_world(world), m_bug1(bug1), m_bug2(bug2)
    , m_ticks(ticks)
    , m_running(false)
    , m_pid(-1), m_cmdFd(-1), m_dataFd(-1)
    // We include the process ID in the tmpdir name to avoid collisions
    // if multiple clients are run at the same time on the same machine.
    , m_tmpdir("/tmp/bugworld_qt_" + std::to_string(getpid()))
{}

SimulatorWorker::~SimulatorWorker()
{
    cleanup();
}

bool SimulatorWorker::startSimulator()
{
    // Printed at the very start of startSimulator() to confirm the function
    // was actually reached and to show the tmpdir path being used.
    // During debugging, the window was blank with no output at all —
    // this line confirmed whether the function was being called or silently skipped.
    std::cerr << "startSimulator() called, tmpdir=" << m_tmpdir << "\n";
    std::cerr.flush();

    if (mkdir(m_tmpdir.c_str(), 0777) != 0) {
        emit errorOccurred("Failed to create temp directory");
        return false;
    }

    string cmd_pipe  = m_tmpdir + "/cmd.pipe";
    string data_pipe = m_tmpdir + "/data.pipe";

    if (mkfifo(cmd_pipe.c_str(), 0666) != 0 ||
        mkfifo(data_pipe.c_str(), 0666) != 0) {
        emit errorOccurred("Failed to create named pipes");
        rmdir(m_tmpdir.c_str());
        return false;
    }

    // Printed before fork() to confirm the working directory and sim path.
    // This was critical because the Qt app is launched from a different context
    // than the terminal client — if the working directory was wrong, execl()
    // would silently fail to find ./bin/sim and the child would exit immediately.
    // Printing cwd confirmed we were in /projects/project-08 as expected.
    char cwd[512];
    getcwd(cwd, sizeof(cwd));
    std::cerr << "Working directory: " << cwd << "\n";
    std::cerr << "Launching: ./bin/sim with world=" << m_world << "\n";
    std::cerr.flush();

    m_pid = fork();
    if (m_pid < 0) {
        emit errorOccurred("Failed to fork simulator process");
        return false;
    }

    if (m_pid == 0) {
        // Child process: replace this process image with the simulator binary.
        // execl() only returns if launching failed, in which case we print
        // the error and exit so the parent can detect the child died.
        execl("./bin/sim", "./bin/sim",
              "--cmd-pipe",  cmd_pipe.c_str(),
              "--data-pipe", data_pipe.c_str(),
              m_world.c_str(),
              m_bug1.c_str(),
              m_bug2.c_str(),
              (char*)nullptr);
        perror("execl failed");
        exit(1);
    }

    // Printed in the parent process after fork() succeeds.
    // The child always sees pid=0 from fork(), so the pid printed here
    // is the real OS process ID of the simulator — useful for confirming
    // a new process was actually created and for manual inspection with ps.
    std::cerr << "forked simulator pid=" << m_pid << "\n";
    std::cerr.flush();

    // Wait 50ms then check if sim already exited.
    // This catches the case where sim exits immediately due to bad arguments
    // or missing files — without this check, the retry loop below would spin
    // for 2 full seconds before reporting failure, with no explanation why.
    // WNOHANG means waitpid returns immediately instead of blocking.
    usleep(50000);
    int childStatus;
    pid_t result = waitpid(m_pid, &childStatus, WNOHANG);
    if (result == m_pid) {
        std::cerr << "sim exited immediately with status="
                  << WEXITSTATUS(childStatus) << "\n";
        std::cerr.flush();
        emit errorOccurred("Simulator exited immediately");
        return false;
    }
    std::cerr << "sim still running, opening pipes...\n";
    std::cerr.flush();

    // Retry opening the command pipe using O_NONBLOCK so the open() call
    // returns immediately with ENXIO instead of blocking forever.
    // A blocking open() on a FIFO with O_WRONLY waits until a reader opens
    // the other end — if sim crashes before doing so, we would hang permanently.
    // We retry up to 200 times (2 seconds total) to give sim time to start.
    int retries = 0;
    while (retries < 200) {
        m_cmdFd = open(cmd_pipe.c_str(), O_WRONLY | O_NONBLOCK);
        if (m_cmdFd >= 0) break;
        // ENXIO means no reader has opened the pipe yet — keep retrying.
        // Any other errno is an unexpected error and we stop immediately.
        if (errno != ENXIO) {
            std::cerr << "open cmd_pipe failed with unexpected errno="
                      << errno << " (" << strerror(errno) << ")\n";
            std::cerr.flush();
            break;
        }
        usleep(10000);
        retries++;
    }

    // Printed after the retry loop to show whether the pipe was opened
    // successfully and how many retries were needed. A fd of -1 after
    // 200 retries means sim never opened its end of the pipe.
    std::cerr << "cmd_pipe open result: fd=" << m_cmdFd
              << " after " << retries << " retries\n";
    std::cerr.flush();

    if (m_cmdFd < 0) {
        emit errorOccurred("Simulator did not start in time");
        kill(m_pid, SIGTERM);
        waitpid(m_pid, nullptr, 0);
        return false;
    }

    m_dataFd = open(data_pipe.c_str(), O_RDONLY | O_NONBLOCK);
    if (m_dataFd < 0) {
        emit errorOccurred("Failed to open data pipe");
        close(m_cmdFd);
        kill(m_pid, SIGTERM);
        waitpid(m_pid, nullptr, 0);
        return false;
    }

    // Final confirmation that both pipes are open and the simulator
    // is ready to receive commands. If this line appears, the setup
    // succeeded and the main loop can begin sending STEP commands.
    std::cerr << "Pipes opened successfully\n";
    std::cerr.flush();
    return true;
}

void SimulatorWorker::run()
{
    // Confirms that the QThread actually started and called run().
    // During early debugging the window was blank — this line ruled out
    // the possibility that the thread was never started or the signal/slot
    // connection from QThread::started to run() was broken.
    std::cerr << "run() called\n";
    std::cerr.flush();
    m_running = true;

    if (!startSimulator()) {
        std::cerr << "startSimulator() FAILED\n";
        std::cerr.flush();
        emit finished();
        return;
    }

    while (m_running) {
        string command = "STEP " + std::to_string(m_ticks) + "\n";
        if (write(m_cmdFd, command.c_str(), command.size()) < 0) break;

        string response = readToEnd();
        if (response.empty()) {
            // An empty response means the simulator closed the pipe or
            // crashed. We log this before breaking so it is clear in the
            // terminal output why the loop stopped rather than silently exiting.
            std::cerr << "Empty response from simulator\n";
            std::cerr.flush();
            break;
        }

        // Delegate to the free function so the same parsing logic is
        // shared between the live simulation loop and the unit tests.
        WorldState state = parseResponse(response);

        // Printed every frame to confirm the parse succeeded and show
        // the cycle counter incrementing. This was how we first confirmed
        // frames were being received after fixing the pipe connection issue.
        std::cerr << "Frame parsed: cycle=" << state.cycle
                  << " valid=" << state.valid << "\n";
        std::cerr.flush();
        if (state.valid) emit frameReady(state);
        usleep(100000);
    }

    cleanup();
    emit finished();
}

void SimulatorWorker::stop()
{
    m_running = false;
}

string SimulatorWorker::readToEnd()
{
    string buffer;
    char chunk[4096];

    while (m_running) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(m_dataFd, &readfds);

        // select() with a 1 second timeout prevents blocking forever
        // if the simulator is slow or temporarily not sending data.
        struct timeval timeout;
        timeout.tv_sec  = 1;
        timeout.tv_usec = 0;

        int ret = select(m_dataFd + 1, &readfds, nullptr, nullptr, &timeout);
        if (ret < 0) break;
        if (ret == 0) continue;

        ssize_t bytes = read(m_dataFd, chunk, sizeof(chunk));
        if (bytes > 0) {
            buffer.append(chunk, bytes);
            // The simulator protocol marks the end of each frame with END.
            // We check for both mid-buffer and end-of-buffer positions
            // because the END marker may arrive in a separate read chunk.
            if (buffer.find("\nEND\n") != string::npos ||
                (buffer.size() >= 4 &&
                 buffer.rfind("END\n") == buffer.size() - 4))
                break;
        } else if (bytes == 0) {
            // EOF means the simulator closed the write end of the pipe.
            break;
        } else {
            // EAGAIN/EWOULDBLOCK is normal for non-blocking pipes —
            // it means no data is available yet, so we retry.
            if (errno == EAGAIN || errno == EWOULDBLOCK) continue;
            break;
        }
    }
    return buffer;
}

// The member function delegates to the free function above.
// This keeps the class interface intact while allowing the parsing
// logic to be tested without instantiating a SimulatorWorker.
WorldState SimulatorWorker::parseResponse(const string& response)
{
    return ::parseResponse(response);
}

void SimulatorWorker::cleanup()
{
    if (m_cmdFd >= 0) {
        // Cast to void to explicitly acknowledge we are not checking
        // the return value — at shutdown the pipe may already be broken
        // and a failed write here is acceptable.
        (void)write(m_cmdFd, "QUIT\n", 5);
        close(m_cmdFd);
        m_cmdFd = -1;
    }
    if (m_dataFd >= 0) {
        close(m_dataFd);
        m_dataFd = -1;
    }
    if (m_pid > 0) {
        usleep(200000);
        int status;
        // WNOHANG checks if sim has already exited without blocking.
        // If it hasn't exited after 200ms, we send SIGTERM to force it.
        if (waitpid(m_pid, &status, WNOHANG) == 0)
            kill(m_pid, SIGTERM);
        waitpid(m_pid, nullptr, 0);
        m_pid = -1;
    }
    unlink((m_tmpdir + "/cmd.pipe").c_str());
    unlink((m_tmpdir + "/data.pipe").c_str());
    rmdir(m_tmpdir.c_str());
}