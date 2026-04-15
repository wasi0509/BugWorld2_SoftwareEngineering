#pragma once

#include <QWidget>
#include <deque>
#include <map>
#include <utility>
#include "simulator.h"

// GridWidget is a custom QWidget that renders the bug world grid.
// It maintains its own trace history and repaints on every new frame.
class GridWidget : public QWidget
{
    Q_OBJECT

public:
    explicit GridWidget(QWidget* parent = nullptr);
    void setTraceN(int n);

public slots:
    void onFrameReady(const WorldState& state);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    WorldState m_state;
    std::map<char, std::deque<std::pair<int,int>>> m_traceHistory;
    int m_traceN = 5;

    bool isBugChar(char c) const;
    std::map<char, std::pair<int,int>> extractBugPositions() const;
    void updateHistory(const std::map<char, std::pair<int,int>>& positions);
    QColor colorForBug(char bug) const;
    QPointF cellTopLeft(int row, int col, float cellW, float cellH) const;
};