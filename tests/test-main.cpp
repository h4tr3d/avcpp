// In a Catch project with multiple files, dedicate one file to compile the
// source code of Catch itself and reuse the resulting object file for linking.

#define CATCH_CONFIG_RUNNER

#include <catch2/catch.hpp>

#include "av.h"

int main(int argc, char* argv[])
{
    av::init();
    return Catch::Session().run(argc, argv);
}

// That's it

// Compile implementation of Catch for use with files that do contain tests:
// - g++ -std=c++11 -Wall -I$(CATCH_SINGLE_INCLUDE) -c 000-CatchMain.cpp
// - cl -EHsc -I%CATCH_SINGLE_INCLUDE% -c 000-CatchMain.cpp
