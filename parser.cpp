#include "parser.h"

#include <sstream>
#include <algorithm>

using std::string;

// parseResponse is a free function so it can be tested independently
// in test_client.cpp without needing a SimulatorWorker instance or
// any Qt dependencies. The SimulatorWorker delegates to this function.
WorldState parseResponse(const string& response)
{
    WorldState state;
    std::istringstream iss(response);
    string line;

    while (std::getline(iss, line)) {
        // CYCLE marks the start of a valid frame and contains the current
        // simulation tick number. We use it to set the valid flag because
        // any response missing CYCLE should not be rendered.
        if (line.rfind("CYCLE", 0) == 0) {
            state.cycle = std::stoi(line.substr(6));
            state.valid = true;
        }
        // MAP gives us the grid dimensions so the renderer knows how many
        // rows and columns to expect before iterating over ROW lines.
        else if (line.rfind("MAP", 0) == 0) {
            std::istringstream ms(line);
            string tmp;
            ms >> tmp >> state.rows >> state.cols;
        }
        // Each ROW line represents one row of the world grid.
        // We strip the "ROW " prefix and store only the cell content.
        // Red nest cells (+) are replaced with empty cells (.) because
        // the nest terrain is handled separately by the renderer.
        else if (line.rfind("ROW", 0) == 0) {
            string row = line.substr(4);
            std::replace(row.begin(), row.end(), '+', '.');
            state.grid.push_back(row);
        }
        // STATS contains the live score: bugs alive and food collected
        // for each team, displayed in the bottom status bar.
        else if (line.rfind("STATS", 0) == 0) {
            std::istringstream ss(line);
            string tmp;
            ss >> tmp >> state.redAlive >> state.blackAlive
               >> state.redFood >> state.blackFood;
        }
    }
    return state;
}