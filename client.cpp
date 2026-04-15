#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <cctype>
#include <deque>
#include <errno.h>
#include <fcntl.h>
#include <iostream>
#include <map>
#include <mutex>
#include <signal.h>
#include <sstream>
#include <string>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#include <utility>
#include <vector>

using namespace std;

// ANSI color codes for rendering bug positions and their trace trails.
// Each bug team gets its own color with three intensity levels:
// vivid for the current position, medium for recent history, soft for older steps.
const string ANSI_RESET = "\033[0m";
const string ANSI_DIM = "\033[2m";
const string ANSI_BUG_RED = "\033[1;92m";
const string ANSI_BUG_BLACK = "\033[1;96m";
const string ANSI_BUG_RED_MID = "\033[0;92m";
const string ANSI_BUG_BLACK_MID = "\033[0;96m";
const string ANSI_BUG_RED_SOFT = "\033[2;92m";
const string ANSI_BUG_BLACK_SOFT = "\033[2;96m";

// Character used to mark previous bug positions in the trace.
const char TRACE_SYMBOL = '*';
/*
// Global flag that controls the main simulation loop.
// It stays true while the program is running and becomes false
// when the user presses Ctrl+C.
bool running = true;

// Number of previous positions to keep per bug.
atomic<int> traceN{5};

mutex historyMutex;

// Stores the position history for each bug character.
map<char, deque<pair<int, int>>> traceHistory;

*/

// wrap into struct to avoid global state
struct SharedState
{
    atomic<bool> running{true};
    atomic<int> traceN{5};
    mutex historyMutex;
    map<char, deque<pair<int, int>>> traceHistory;
};

// Sets running to false on Ctrl+C so the main loop can clean up before exiting.
SharedState *globalState = nullptr;
void signal_handler(int)
{
    if (globalState)
        globalState->running.store(false);
}

// Checks if a character is a bug
// why do we need this function: it determines whther a character represents a bug in the grid or not
// Since the simulator output is plain text, we need this to identify bugs manually when scanning the grid
bool isBugChar(char c)
{
    return c == 'R' || c == 'r' || c == 'B' || c == 'b'; // only consider these bug characters as valid bugs for tracking and display purposes.
}

// Returns the color used for the bug's current position meaning the brightest color
// is used here so that the live position stands out clearly from its trail.
string vividColorForBug(char bug)
{
    if (bug == 'R' || bug == 'r')
        return ANSI_BUG_RED;
    if (bug == 'B' || bug == 'b')
        return ANSI_BUG_BLACK;
    return "\033[1;93m";
}

// Returns a medium-intensity color for recent positions in the trace as this will help
// differnetiate newer movements from older ones without overpowering the current position.
string mediumColorForBug(char bug)
{
    if (bug == 'R' || bug == 'r')
        return ANSI_BUG_RED_MID;
    if (bug == 'B' || bug == 'b')
        return ANSI_BUG_BLACK_MID;
    return "\033[0;93m";
}

// Returns a faded color for older positions in the trace which creates a visual fading effect
// this makes the bug's path easier to follow over time.
string softColorForBug(char bug)
{
    if (bug == 'R' || bug == 'r')
        return ANSI_BUG_RED_SOFT;
    if (bug == 'B' || bug == 'b')
        return ANSI_BUG_BLACK_SOFT;
    return "\033[2;93m";
}

// extractBugPositions : basically finds where each is in  the grid
// why this function is needed : to extract the current position of each bug from the grid
// Since sim doesnt preovide structured data and only text, we must scan grid in
// every frame to locate the bugs
map<char, pair<int, int>> extractBugPositions(const vector<string> &grid)
{
    map<char, pair<int, int>> positions;

    for (int row = 0; row < static_cast<int>(grid.size()); ++row)
    {
        for (int col = 0; col < static_cast<int>(grid[row].size()); ++col)
        {
            char c = grid[row][col];
            // positions.find() ensures we only store the first occurrence of each
            // bug character per frame in case of any duplicate in the output.
            if (isBugChar(c) && positions.find(c) == positions.end())
            {
                positions[c] = {row, col};
            }
        }
    }

    return positions;
}

// Purpose of updateHistory: pushes the new position to the front of each bug's history
// and removes the oldest entry from the back if the size goes over trace  N.
// A mutex is used as this data is shared with inputThread and we need to make sure
// the thread safety
void updateHistory(const map<char, pair<int, int>> &positions, SharedState &state)
{
    lock_guard<mutex> lock(state.historyMutex);
    int limit = max(0, state.traceN.load());

    for (const auto &[bug, pos] : positions)
    {
        auto &history = state.traceHistory[bug];
        history.push_front(pos);
        while (static_cast<int>(history.size()) > limit)
        {
            history.pop_back();
        }
    }

    for (auto &[bug, history] : state.traceHistory)
    {
        while (static_cast<int>(history.size()) > limit)
        {
            history.pop_back();
        }
        if (limit == 0)
        {
            history.clear();
        }
    }
}

// Runs in a background thread so the user can type a new N value while
// the simulation is running without blocking the display loop.
void inputThread(SharedState &state)
{
    while (state.running)
    {
        // check if there is input available (non-blocking)
        fd_set set;
        FD_ZERO(&set);
        FD_SET(STDIN_FILENO, &set);

        struct timeval timeout;
        timeout.tv_sec = 1; // wait max 1 second
        timeout.tv_usec = 0;

        int ret = select(STDIN_FILENO + 1, &set, NULL, NULL, &timeout);

        if (ret <= 0)
        {
            // no input or interrupted → loop again
            continue;
        }

        string input;
        getline(cin, input);

        // quit support
        if (input == "q" || input == "quit" || input == "QUIT")
        {
            state.running = false;
            break;
        }
        try
        {
            int newN = stoi(input);
            /*if (!(cin >> newN))
            {
                if (!running)
                {
                    break;
                }
                cin.clear();
                string dummy;
                getline(cin, dummy);
                continue;
            }*/

            if (newN >= 0)
            {
                state.traceN = newN;
                lock_guard<mutex> lock(state.historyMutex);
                for (auto &[bug, history] : state.traceHistory)
                {
                    while (static_cast<int>(history.size()) > newN)
                    {
                        history.pop_back();
                    }
                    if (newN == 0)
                    {
                        history.clear();
                    }
                }
            }
        }
        catch (...)
        {
            // ignore invalid input
        }
    }
}

// Reads simulator output from the data pipe until the protocol marker END
// is reached.
//
// Why this function exists:
// The simulator may send its response in multiple chunks, so a single read()
// call is not enough. We therefore keep reading until we detect the END line,
// which marks the end of one complete simulator frame.
string readtoend(int data_fd, SharedState &state)
{
    string buffer;
    char byte[4096];

    while (state.running)
    {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(data_fd, &readfds);

        // We use select() with a timeout so the program does not block forever
        // if the simulator is temporarily not sending data.
        struct timeval timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        int ret = select(data_fd + 1, &readfds, NULL, NULL, &timeout);

        if (ret < 0)
        {
            perror("select");
            break;
        }

        // Timeout happened: no data available yet.
        // We simply continue waiting instead of treating it as an error.
        if (ret == 0)
        {
            continue;
        }

        ssize_t bytes = read(data_fd, byte, sizeof(byte));

        if (bytes > 0)
        {
            buffer.append(byte, bytes);

            // The simulator protocol ends one complete frame with END.
            // We stop reading once END is found, so the caller receives
            // exactly one logical simulator response.
            if (buffer.find("\nEND\n") != string::npos ||
                (buffer.size() >= 4 && buffer.rfind("END\n") == buffer.size() - 4))
            {
                break;
            }
        }
        else if (bytes == 0)
        {
            // EOF: the simulator closed the pipe.
            break;
        }
        else
        {
            // EAGAIN / EWOULDBLOCK is expected for non-blocking pipes,
            // so we retry instead of failing immediately.
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                continue;
            }

            perror("read");
            break;
        }
    }

    return buffer;
}

// Pupose of the given function : Parses the simulator response and renders it to the terminal.
// As the simulator uses a text-based protocol,we extract meaningful parts (cycle, grid, stats)
// and convert them into a visual display.
// This also integrates bug tracking and trace visualization.
void parseddisplay(const string &response, SharedState &state)
{
    istringstream iss(response);
    string line;
    string cycle;
    vector<string> grid;
    string stats;
    int rows = 0, cols = 0; // add map parsing

    while (getline(iss, line))
    {
        // The line starting with CYCLE contains the current simulation step.
        if (line.rfind("CYCLE", 0) == 0)
        {
            cycle = line;
        }
        // The line starting with MAP contains the dimensions of the world grid.
        else if (line.rfind("MAP", 0) == 0)
        {
            istringstream mapStream(line);
            string tmp;
            mapStream >> tmp >> rows >> cols;
        }
        // Each ROW line contains one row of the world grid.
        // We remove the "ROW " prefix and store only the visible map content.
        else if (line.rfind("ROW", 0) == 0)
        {
            string row = line.substr(4);
            replace(row.begin(), row.end(), '+', '.');
            grid.push_back(row);
        }
        // The STATS line contains summary information about the simulation.
        else if (line.rfind("STATS", 0) == 0)
        {
            stats = line;
        }
    }

    // Extract current positions and update the shared history for this tick.
    auto currentPositions = extractBugPositions(grid);
    updateHistory(currentPositions, state);
    map<pair<int, int>, pair<char, int>> traceCells;
    {
        lock_guard<mutex> lock(state.historyMutex);
        for (const auto &[bug, history] : state.traceHistory)
        {
            if (history.size() <= 1)
            {
                continue;
            }

            int previousCount = static_cast<int>(history.size()) - 1;
            for (int idx = 1; idx <= previousCount; ++idx)
            {
                const auto &pos = history[idx];
                int ageRank = idx;
                traceCells[pos] = {bug, ageRank};
            }
        }
    }

    // Clear the terminal and move the cursor to the top-left corner.
    // This creates a simple real-time animation effect instead of printing
    // frames one below another.
    cout << "\033[2J\033[H";
    cout << cycle << "\n";
    cout << "MAP " << rows << " x " << cols << "\n\n"; // display map size

    for (int row = 0; row < static_cast<int>(grid.size()); ++row)
    {
        for (int col = 0; col < static_cast<int>(grid[row].size()); ++col)
        {
            pair<int, int> pos = {row, col};
            char original = grid[row][col];

            // Current bug position: always drawn with the vivid color.
            auto currentIt = find_if(currentPositions.begin(), currentPositions.end(),
                                     [&](const auto &entry)
                                     { return entry.second == pos; });

            if (currentIt != currentPositions.end())
            {
                cout << vividColorForBug(currentIt->first) << original << ANSI_RESET;
                continue;
            }

            // Trace cell: draw the trace symbol with a faded color.
            // The history is split into three equal bands. The oldest third
            // gets soft, the middle gets medium, and the newest gets vivid,
            // giving a smooth fade from the bug's tail to just behind it.
            // We skip space cells to avoid placing trace symbols on empty terrain.
            auto traceIt = traceCells.find(pos);
            if (traceIt != traceCells.end() && original != '#')
            {
                char bug = traceIt->second.first;
                int rank = traceIt->second.second;
                int totalRanks = 1;
                {
                    auto it = state.traceHistory.find(bug);
                    if (it != state.traceHistory.end())
                    {
                        totalRanks = max(1, static_cast<int>(it->second.size()) - 1);
                    }
                }

                string color;
                if (totalRanks <= 2)
                {
                    color = (rank == totalRanks) ? mediumColorForBug(bug) : softColorForBug(bug);
                }
                else if (rank <= totalRanks / 3)
                {
                    color = softColorForBug(bug);
                }
                else if (rank >= (2 * totalRanks) / 3 + 1)
                {
                    color = vividColorForBug(bug);
                }
                else
                {
                    color = mediumColorForBug(bug);
                }

                cout << color << TRACE_SYMBOL << ANSI_RESET;
                continue;
            }

            cout << original;
        }
        cout << "\n";
    }

    cout << "\n"
         << stats << "\n";
    cout << "Trace length: N=" << state.traceN.load() << " | type a number + Enter to change\n";
    cout << "type 'q' + Enter to quit\n";
    cout.flush();
}

// UNIT_TEST excludes main() when compiling the test binary so that
// test_client.cpp can define its own main() without a duplicate symbol error.
#ifndef UNIT_TEST
int main(int argc, char *argv[])
{
    // The program requires at least:
    // ./client <world> <bug1> <bug2>
    // Optional:
    // [ticks_per_frame] [fps]
    //
    // Why argument validation matters:
    // It prevents undefined behavior and gives the user a clear way
    // to run the program correctly.
    if (argc < 4)
    {
        cerr << "Usage: ./client <world> <bug1> <bug2> [ticks_per_frame] [fps]\n";
        return 1;
    }

    string world = argv[1];
    string bug1 = argv[2];
    string bug2 = argv[3];

    // ticks: how many simulation ticks are advanced after each STEP command
    // fps: how often frames are displayed per second
    int ticks = (argc >= 5) ? atoi(argv[4]) : 50;
    int fps = (argc >= 6) ? atoi(argv[5]) : 10;

    // We reject non-positive values because STEP 0 or fps 0
    // would make the simulation meaningless or invalid.
    if (ticks < 1 || fps < 1)
    {
        cerr << "ticks_per_frame and fps must be >= 1\n";
        return 1;
    }

    SharedState state;
    globalState = &state;

    // Register Ctrl+C handler for graceful shutdown.
    signal(SIGINT, signal_handler);

    // Start the input thread before the simulation loop so the user
    // can adjust N from the very first frame.
    thread t(inputThread, ref(state));
    /*  t.detach(); */

    // Create a unique temporary directory for this client process.
    // Using the PID avoids name collisions if multiple clients run at once.
    string tmpdir = "/tmp/bugworld_" + to_string(getpid());

    if (mkdir(tmpdir.c_str(), 0777) != 0)
    {
        perror("mkdir");
        return 1;
    }

    // Named pipes used for IPC with the simulator:
    // - cmd.pipe  : client -> simulator
    // - data.pipe : simulator -> client
    string cmd_pipe = tmpdir + "/cmd.pipe";
    string data_pipe = tmpdir + "/data.pipe";

    if (mkfifo(cmd_pipe.c_str(), 0666) != 0)
    {
        perror("mkfifo cmd");
        rmdir(tmpdir.c_str());
        return 1;
    }

    if (mkfifo(data_pipe.c_str(), 0666) != 0)
    {
        perror("mkfifo data");
        unlink(cmd_pipe.c_str());
        rmdir(tmpdir.c_str());
        return 1;
    }

    // fork() creates a child process that will run the simulator.
    // This allows the client to stay in control of sending commands
    // and receiving output through the two pipes.
    pid_t pid = fork();

    if (pid < 0)
    {
        perror("fork");
        unlink(cmd_pipe.c_str());
        unlink(data_pipe.c_str());
        rmdir(tmpdir.c_str());
        return 1;
    }

    if (pid == 0)
    {
        // Child process:
        // Replace this process image with the simulator executable.
        //
        // Why execl():
        // It directly launches the provided program with the required
        // command-line arguments, which is ideal for starting the simulator.
        execl("./bin/sim",
              "./bin/sim",
              "--cmd-pipe", cmd_pipe.c_str(),
              "--data-pipe", data_pipe.c_str(),
              world.c_str(),
              bug1.c_str(),
              bug2.c_str(),
              (char *)NULL);

        // If execl() returns, it means launching the simulator failed.
        perror("execl failed");
        exit(1);
    }

    // gives the simulator time to start and open the pipes and reduces race conditions
    // during pipe connection.
    // usleep(100000);
    int cmd_fd = -1;
    int retries = 0;
    while ((cmd_fd = open(cmd_pipe.c_str(), O_WRONLY)) < 0 && retries < 50)
    {
        usleep(10000); // wait 10ms per retry, 50 retries = 500ms max
        retries++;
    }

    if (cmd_fd < 0)
{
    cerr << "Failed to connect to simulator after timeout\n";
    kill(pid, SIGTERM);
    waitpid(pid, NULL, 0);
    unlink(cmd_pipe.c_str());
    unlink(data_pipe.c_str());
    rmdir(tmpdir.c_str());
    return 1;
}

    // Open command pipe for writing commands to simulator.
    // Open data pipe in non-blocking mode so reads can be managed with select().
    int data_fd = open(data_pipe.c_str(), O_RDONLY | O_NONBLOCK);

    if (data_fd < 0)
    {
        cerr << "Failed to open data pipe\n";
        close(cmd_fd);
        waitpid(pid, NULL, 0);
        unlink(cmd_pipe.c_str());
        unlink(data_pipe.c_str());
        rmdir(tmpdir.c_str());
        return 1;
    }

    while (state.running)
    {
        string command = "STEP " + to_string(ticks) + "\n";

        if (write(cmd_fd, command.c_str(), command.size()) < 0)
        {
            perror("write STEP");
            break;
        }

        string response = readtoend(data_fd, state);

        // we define the response empty if simulator wasterminated
        // or communication was interrupted.
        if (response.empty())
        {
            break;
        }

        parseddisplay(response, state);
        usleep(1000000 / fps);
    }

    // Tells the simulator to shut down cleanly before exiting.
    write(cmd_fd, "QUIT\n", 5);

    // exit cleanly
    usleep(200000); // 200ms

    int status;
    if (waitpid(pid, &status, WNOHANG) == 0)
    {
        kill(pid, SIGTERM);
    }

    close(cmd_fd);
    close(data_fd);

    // Wait for child process to finish to avoid zombie processes.
    waitpid(pid, NULL, 0);

    // Remove temporary IPC resources.
    unlink(cmd_pipe.c_str());
    unlink(data_pipe.c_str());
    rmdir(tmpdir.c_str());

    state.running = false;
    t.join();

    return 0;
}
#endif

