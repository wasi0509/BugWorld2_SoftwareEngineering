#pragma once

#include <string>
#include <vector>

// WorldState holds one fully parsed frame of simulator output.
// It is kept in a separate header with no Qt dependency so that
// it can be included in unit tests without requiring the Qt toolchain.
struct WorldState {
    int cycle      = 0;
    int rows       = 0;
    int cols       = 0;
    std::vector<std::string> grid;
    int redAlive   = 0;
    int blackAlive = 0;
    int redFood    = 0;
    int blackFood  = 0;
    bool valid     = false;
};

// Free function declaration so test_client.cpp can call it directly
// without instantiating a SimulatorWorker.
WorldState parseResponse(const std::string& response);