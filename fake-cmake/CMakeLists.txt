# This is just a FAKE CMake config; e.g. usable for CLion but not to actually build the project

cmake_minimum_required(VERSION 3.0)

add_library(fake-azure-sphere-sensor-demo STATIC
    ../azure-sphere-sensor-demo/main.c
)

include_directories(../external/MT3620_Grove_Shield_Library/)
include_directories(../external/flatcc/Inc/Public)
include_directories(../objectbox-client-azure-sphere/Inc/Public)
