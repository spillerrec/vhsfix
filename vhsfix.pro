TEMPLATE = app
CONFIG += console
TARGET = vhsfix
INCLUDEPATH += .
QMAKE_CXXFLAGS += -std=c++11
LIBS += -lz -llzma -lavcodec -lavformat -lavutil

# Input
HEADERS += src/VideoFile.hpp src/VideoFrame.hpp src/ffmpeg.hpp
SOURCES += src/VideoFile.cpp src/VideoFrame.cpp src/main.cpp src/dump/DumpPlane.cpp
