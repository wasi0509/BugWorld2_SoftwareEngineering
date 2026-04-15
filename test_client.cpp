#include <atomic>
#include <deque>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>
#include "parser.h"

using namespace std;

// SharedState is defined in client.cpp.
// We redefine it here locally for the test binary so that tests can
// create isolated instances without linking against the full client
// or pulling in any global state. Each test gets its own fresh copy.
struct SharedState
{
    atomic<bool> running{true};
    atomic<int>  traceN{5};
    mutex        historyMutex;
    map<char, deque<pair<int, int>>> traceHistory;
};

// Functions under test, implemented in client.cpp.
map<char, pair<int, int>> extractBugPositions(const vector<string>& grid);

// updateHistory now takes a SharedState reference instead of using globals.
// This matches the refactored client.cpp signature after the global state
// was wrapped into SharedState to address the TA's design feedback.
void updateHistory(const map<char, pair<int, int>>& positions, SharedState& state);

bool isBugChar(char c);

// parseResponse is tested via parser.cpp — no SimulatorWorker needed.
// It is a free function so it can be called here without Qt dependencies.
WorldState parseResponse(const std::string& response);

// Returns a heap-allocated SharedState so we avoid copying atomic and mutex
// members, which are non-copyable by design in C++.
// Each test gets its own isolated instance with no shared global state.
unique_ptr<SharedState> resetState(int n)
{
    auto state = make_unique<SharedState>();
    state->traceN = n;
    return state;
}

// Prints PASS or FAIL and returns the result so it can be added to the counter.
bool expect(bool condition, const string& testName)
{
    if (condition)
    {
        cout << "PASS: " << testName << "\n";
        return true;
    }
    cout << "FAIL: " << testName << "\n";
    return false;
}

int main()
{
    int passed = 0;
    int total = 0;

    // Test 1: extractBugPositions correctly identifies row and column for each bug.
    // If this is wrong, every trace position drawn on screen will be in the wrong cell.
    {
        vector<string> grid = {
            "# . R . #",
            " . . . . ",
            "# B . . #"
        };
        auto pos = extractBugPositions(grid);
        ++total; passed += expect(pos['R'] == make_pair(0, 4) && pos['B'] == make_pair(2, 2),
                                   "extractBugPositions finds correct row and column");
    }

    // Test 2: we always put the newest position at index 0, that way the
    // fade effect knows which ones to dim first and which ones are recent
    {
        auto state = resetState(3);
        updateHistory({{'R', {1, 1}}}, *state);
        updateHistory({{'R', {1, 2}}}, *state);
        updateHistory({{'R', {1, 3}}}, *state);
        updateHistory({{'R', {1, 4}}}, *state);
        lock_guard<mutex> lock(state->historyMutex);
        ++total; passed += expect(state->traceHistory['R'].size() == 3 &&
                                   state->traceHistory['R'][0] == make_pair(1, 4) &&
                                   state->traceHistory['R'][2] == make_pair(1, 2),
                                   "updateHistory keeps only the last N entries");
    }

    // Test 3: lowering N at runtime to trim the history on the very next call.
    // if not done then the trace would appear longer than the user requested.
    {
        auto state = resetState(5);
        updateHistory({{'R', {0, 0}}}, *state);
        updateHistory({{'R', {0, 1}}}, *state);
        updateHistory({{'R', {0, 2}}}, *state);
        state->traceN = 2;
        updateHistory({{'R', {0, 3}}}, *state);
        lock_guard<mutex> lock(state->historyMutex);
        ++total; passed += expect(state->traceHistory['R'].size() == 2 &&
                                   state->traceHistory['R'][0] == make_pair(0, 3) &&
                                   state->traceHistory['R'][1] == make_pair(0, 2),
                                   "lowering N trims history immediately");
    }

    // Test 4: even if the bug didnt move at all, we still write down
    // its position because the trail is supposed to track time steps not just new places it went
    {
        auto state = resetState(4);
        updateHistory({{'B', {2, 2}}}, *state);
        updateHistory({{'B', {2, 2}}}, *state);
        updateHistory({{'B', {2, 2}}}, *state);
        lock_guard<mutex> lock(state->historyMutex);
        ++total; passed += expect(state->traceHistory['B'].size() == 3,
                                   "bug that does not move still records every tick");
    }

    // Test 5: since N=0 is totally valid and the player doesnt want a trail,
    // we need to make sure the code doesnt crash when that happens
    {
        auto state = resetState(0);
        updateHistory({{'R', {5, 5}}}, *state);
        lock_guard<mutex> lock(state->historyMutex);
        ++total; passed += expect(state->traceHistory['R'].empty(),
                                   "N=0 keeps empty history without crashing");
    }

    // Test 6: if fewer ticks have passed than N, only that number of entries is stored.
    // The renderer must work with a partial history at the start of a simulation.
    {
        auto state = resetState(10);
        updateHistory({{'R', {3, 1}}}, *state);
        updateHistory({{'R', {3, 2}}}, *state);
        lock_guard<mutex> lock(state->historyMutex);
        ++total; passed += expect(state->traceHistory['R'].size() == 2,
                                   "fewer ticks than N stores only available entries");
    }

    // Test 7: isBugChar returns true for all valid bug characters.
    // R and B are bugs without food, r and b are bugs carrying food.
    // If any of these are missed, that bug type will never be tracked.
    {
        ++total; passed += expect(
            isBugChar('R') && isBugChar('r') &&
            isBugChar('B') && isBugChar('b'),
            "isBugChar returns true for R, r, B, b");
    }

    // Test 8: isBugChar returns false for all non-bug characters.
    // These are terrain characters from the protocol and must never
    // be treated as bugs or they would pollute the trace history.
    {
        ++total; passed += expect(
            !isBugChar('#') && !isBugChar('.') &&
            !isBugChar('+') && !isBugChar('-') &&
            !isBugChar('1') && !isBugChar('9') &&
            !isBugChar(' '),
            "isBugChar returns false for terrain and food characters");
    }

    // Test 9: extractBugPositions on an empty grid returns an empty map.
    // The renderer must handle this gracefully at simulation start.
    {
        vector<string> grid = {};
        auto pos = extractBugPositions(grid);
        ++total; passed += expect(pos.empty(),
            "extractBugPositions returns empty map for empty grid");
    }

    // Test 10: extractBugPositions on a grid with no bugs returns empty map.
    // A world with only terrain should produce no history entries.
    {
        vector<string> grid = {
            "# # # # #",
            "# . . . #",
            "# # # # #"
        };
        auto pos = extractBugPositions(grid);
        ++total; passed += expect(pos.empty(),
            "extractBugPositions returns empty map when no bugs present");
    }

    // Test 11: extractBugPositions correctly detects r and b (food-carrying bugs).
    // These were missing from isBugChar in the previous version and caused
    // food-carrying bugs to be invisible to the trace system.
    {
        vector<string> grid = {
            "# . r . #",
            " . . . . ",
            "# b . . #"
        };
        auto pos = extractBugPositions(grid);
        ++total; passed += expect(
            pos.count('r') && pos['r'] == make_pair(0, 4) &&
            pos.count('b') && pos['b'] == make_pair(2, 2),
            "extractBugPositions detects food-carrying bugs r and b");
    }

    // Test 12: when the same bug character appears twice, only the first
    // occurrence (top-left) is stored. This matches the known limitation
    // of the character-based tracking approach.
    {
        vector<string> grid = {
            "# R . . #",
            " . . . . ",
            "# R . . #"
        };
        auto pos = extractBugPositions(grid);
        ++total; passed += expect(
            pos['R'] == make_pair(0, 2),
            "extractBugPositions stores only first occurrence of duplicate bug char");
    }

    // Test 13: two different bugs tracked simultaneously must not interfere
    // with each other's histories. R and B must each maintain their own
    // independent deques.
    {
        auto state = resetState(3);
        updateHistory({{'R', {0, 0}}, {'B', {5, 5}}}, *state);
        updateHistory({{'R', {0, 1}}, {'B', {5, 6}}}, *state);
        lock_guard<mutex> lock(state->historyMutex);
        ++total; passed += expect(
            state->traceHistory['R'].size() == 2 &&
            state->traceHistory['B'].size() == 2 &&
            state->traceHistory['R'][0] == make_pair(0, 1) &&
            state->traceHistory['B'][0] == make_pair(5, 6),
            "R and B histories are tracked independently");
    }

    // Test 14: calling updateHistory with an empty positions map must not
    // crash or corrupt existing history entries.
    {
        auto state = resetState(5);
        updateHistory({{'R', {1, 1}}}, *state);
        updateHistory({}, *state);
        lock_guard<mutex> lock(state->historyMutex);
        ++total; passed += expect(
            state->traceHistory['R'].size() == 1 &&
            state->traceHistory['R'][0] == make_pair(1, 1),
            "empty positions map does not corrupt existing history");
    }

    // Test 15: after lowering N and then raising it again, new entries
    // should accumulate up to the new higher limit.
    {
        auto state = resetState(5);
        updateHistory({{'R', {0, 0}}}, *state);
        updateHistory({{'R', {0, 1}}}, *state);
        state->traceN = 1;
        updateHistory({{'R', {0, 2}}}, *state);
        state->traceN = 4;
        updateHistory({{'R', {0, 3}}}, *state);
        updateHistory({{'R', {0, 4}}}, *state);
        lock_guard<mutex> lock(state->historyMutex);
        ++total; passed += expect(
            state->traceHistory['R'].size() == 3 &&
            state->traceHistory['R'][0] == make_pair(0, 4),
            "raising N after lowering allows history to grow again");
    }

    // Test 16: a very large N should not cause any crash and history
    // should simply grow to however many ticks have occurred.
    {
        auto state = resetState(10000);
        updateHistory({{'R', {0, 0}}}, *state);
        updateHistory({{'R', {0, 1}}}, *state);
        updateHistory({{'R', {0, 2}}}, *state);
        lock_guard<mutex> lock(state->historyMutex);
        ++total; passed += expect(
            state->traceHistory['R'].size() == 3,
            "very large N stores only as many entries as ticks occurred");
    }

    // Test 17: parseResponse correctly parses a full valid simulator response.
    // This verifies that all five protocol fields (CYCLE, MAP, ROW, STATS, END)
    // are extracted correctly and that the valid flag is set when CYCLE is present.
    {
        string response =
            "CYCLE 42\n"
            "MAP 3 3\n"
            "ROW # . #\n"
            "ROW . R .\n"
            "ROW # . #\n"
            "STATS 5 6 2 3\n"
            "END\n";
        WorldState ws = parseResponse(response);
        ++total; passed += expect(
            ws.valid &&
            ws.cycle == 42 &&
            ws.rows == 3 && ws.cols == 3 &&
            ws.grid.size() == 3 &&
            ws.redAlive == 5 && ws.blackAlive == 6 &&
            ws.redFood == 2 && ws.blackFood == 3,
            "parseResponse correctly parses a full valid response");
    }

    // Test 18: parseResponse returns valid=false when CYCLE line is missing.
    // The renderer checks the valid flag before drawing — a frame without
    // a cycle number is incomplete and must never be displayed.
    {
        string response = "MAP 3 3\nROW # . #\nSTATS 1 1 0 0\nEND\n";
        WorldState ws = parseResponse(response);
        ++total; passed += expect(!ws.valid,
            "parseResponse returns valid=false when CYCLE is missing");
    }

    // Test 19: parseResponse on an empty string returns valid=false and does not crash.
    // This guards against the case where readToEnd returns an empty buffer
    // due to a pipe error or simulator shutdown.
    {
        WorldState ws = parseResponse("");
        ++total; passed += expect(!ws.valid,
            "parseResponse on empty string returns valid=false without crashing");
    }

    cout << "\nSummary: " << passed << "/" << total << " tests passed.\n";

    // Return 1 if any test failed so the Makefile can detect it automatically.
    return (passed == total) ? 0 : 1;
}