TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt

QMAKE_CXXFLAGS += -std=c++11
LIBS += -L"D:\boost\boost_1_55_0\stage\lib"
LIBS += -lboost_unit_test_framework-mgw48-mt-d-1_55
INCLUDEPATH += D:\boost\boost_1_55_0
INCLUDEPATH += ..\


SOURCES += \
    mapstack_test.cpp \
    dynarray_test.cpp \
    inkamath_test.cpp

OTHER_FILES += \
    data/input1.txt \
    data/output1.txt \
    data/input2.txt \
    data/output2.txt

HEADERS += \
    inkamath_test.hpp

