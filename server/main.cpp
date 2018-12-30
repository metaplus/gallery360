// server.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

int main(int argc, char* argv[]) {
    app::parse_options(argc, argv);
    app::run();
    return 0;
}
