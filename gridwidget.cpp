#include "gridwidget.h"

#include <QPainter>
#include <algorithm>

using std::map;
using std::pair;
using std::string;

GridWidget::GridWidget(QWidget* parent)
    : QWidget(parent)
{
    setMinimumSize(400, 400);
    setStyleSheet("background-color: #1a1a1a;");
}

void GridWidget::setTraceN(int n)
{
    m_traceN = n;
    for (auto& [bug, history] : m_traceHistory) {
        while (static_cast<int>(history.size()) > n)
            history.pop_back();
        if (n == 0) history.clear();
    }
    update();
}

void GridWidget::onFrameReady(const WorldState& state)
{
    m_state = state;
    updateHistory(extractBugPositions());
    update(); // triggers paintEvent
}

bool GridWidget::isBugChar(char c) const
{
    return c == 'R' || c == 'r' || c == 'B' || c == 'b';
}

map<char, pair<int,int>> GridWidget::extractBugPositions() const
{
    map<char, pair<int,int>> positions;
    for (int row = 0; row < static_cast<int>(m_state.grid.size()); ++row)
        for (int col = 0; col < static_cast<int>(m_state.grid[row].size()); ++col) {
            char c = m_state.grid[row][col];
            if (isBugChar(c) && !positions.count(c))
                positions[c] = {row, col};
        }
    return positions;
}

void GridWidget::updateHistory(const map<char, pair<int,int>>& positions)
{
    int limit = std::max(0, m_traceN);
    for (const auto& [bug, pos] : positions) {
        auto& h = m_traceHistory[bug];
        h.push_front(pos);
        while (static_cast<int>(h.size()) > limit) h.pop_back();
    }
    for (auto& [bug, h] : m_traceHistory) {
        while (static_cast<int>(h.size()) > limit) h.pop_back();
        if (limit == 0) h.clear();
    }
}

QColor GridWidget::colorForBug(char bug) const
{
    if (bug == 'R' || bug == 'r') return QColor(80, 220, 80);   // green
    if (bug == 'B' || bug == 'b') return QColor(80, 200, 220);  // cyan
    return QColor(220, 220, 80);
}

// Odd rows are offset by half a cell to represent the hexagonal stagger
// described in the simulator protocol.
QPointF GridWidget::cellTopLeft(int row, int col, float cellW, float cellH) const
{
    float x = col * cellW + (row % 2 == 1 ? cellW * 0.5f : 0.0f);
    float y = row * cellH;
    return QPointF(x, y);
}

void GridWidget::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    if (m_state.grid.empty()) return;

    int numRows = static_cast<int>(m_state.grid.size());
    int numCols = 0;
    for (const auto& row : m_state.grid)
        numCols = std::max(numCols, static_cast<int>(row.size()));
    if (numRows == 0 || numCols == 0) return;

    // Fit cells to widget size, keeping them square.
    float cellW = width()  / (numCols + 0.5f);
    float cellH = height() / static_cast<float>(numRows);
    float cell  = std::min(cellW, cellH);

    // Build trace lookup: position -> (bug char, age rank)
    map<pair<int,int>, pair<char,int>> traceCells;
    for (const auto& [bug, history] : m_traceHistory) {
        if (history.size() <= 1) continue;
        int count = static_cast<int>(history.size()) - 1;
        for (int idx = 1; idx <= count; ++idx)
            traceCells[history[idx]] = {bug, idx};
    }

    auto currentPositions = extractBugPositions();

    for (int row = 0; row < numRows; ++row) {
        for (int col = 0; col < static_cast<int>(m_state.grid[row].size()); ++col) {
            char c = m_state.grid[row][col];
            QPointF tl = cellTopLeft(row, col, cell, cell);
            QRectF rect(tl.x(), tl.y(), cell - 1, cell - 1);

            // Base terrain color
            QColor bg;
            if      (c == '#')                  bg = QColor(60, 60, 60);
            else if (c == '-')                  bg = QColor(20, 40, 60);
            else if (c >= '1' && c <= '9')      bg = QColor(60 + (c-'0')*15, 60 + (c-'0')*15, 0);
            else                                bg = QColor(30, 30, 30);
            painter.fillRect(rect, bg);

            // Current bug position — draw colored cell with character label
            pair<int,int> pos = {row, col};
            auto curIt = std::find_if(currentPositions.begin(), currentPositions.end(),
                [&](const auto& e){ return e.second == pos; });
            if (curIt != currentPositions.end()) {
                painter.fillRect(rect, colorForBug(curIt->first));
                painter.setPen(Qt::white);
                painter.drawText(rect, Qt::AlignCenter, QString(curIt->first));
                continue;
            }

            // Trace cell — alpha fades from 180 (newest) to 40 (oldest)
            auto traceIt = traceCells.find(pos);
            if (traceIt != traceCells.end() && c != '#') {
                char bug = traceIt->second.first;
                int rank = traceIt->second.second;
                auto histIt = m_traceHistory.find(bug);
                int total = (histIt != m_traceHistory.end())
                            ? std::max(1, static_cast<int>(histIt->second.size()) - 1)
                            : 1;
                float t = (total > 1) ? (1.0f - static_cast<float>(rank - 1) / (total - 1)) : 1.0f;
                QColor tc = colorForBug(bug);
                tc.setAlpha(static_cast<int>(40 + t * 140));
                painter.fillRect(rect, tc);
            }
        }
    }
}