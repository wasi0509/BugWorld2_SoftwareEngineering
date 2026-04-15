#include <QApplication>
#include <iostream>
#include "mainwindow.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    if (argc < 4) {
        std::cerr << "Usage: ./bugworld <world> <bug1> <bug2> [ticks_per_frame] [fps]\n";
        return 1;
    }

    std::string world = argv[1];
    std::string bug1  = argv[2];
    std::string bug2  = argv[3];
    int ticks = (argc >= 5) ? std::atoi(argv[4]) : 50;
    int fps   = (argc >= 6) ? std::atoi(argv[5]) : 10;

    if (ticks < 1 || fps < 1) {
        std::cerr << "ticks_per_frame and fps must be >= 1\n";
        return 1;
    }

    MainWindow window(world, bug1, bug2, ticks, fps);
    window.show();

    return app.exec();
}