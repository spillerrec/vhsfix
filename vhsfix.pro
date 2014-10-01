TEMPLATE = app
CONFIG += console
TARGET = vhsfix
INCLUDEPATH += .
QMAKE_CXXFLAGS += -std=c++11
LIBS += -lz -llzma -lavcodec -lavformat -lavutil

# Input
HEADERS += src/ffmpeg.hpp
SOURCES += src/main.cpp src/dump/DumpPlane.cpp
