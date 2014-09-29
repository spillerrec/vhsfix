TEMPLATE = app
CONFIG += console
TARGET = vhsfix
INCLUDEPATH += .
QMAKE_CXXFLAGS += -std=c++11
LIBS += -lz -llzma -lavcodec -lavformat -lavutil

# Input
SOURCES += main.cpp dump/DumpPlane.cpp
