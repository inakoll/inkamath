TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt

QMAKE_CXXFLAGS += -std=c++11
QMAKE_CXXFLAGS += -W -Wall
INCLUDEPATH += D:\boost\boost_1_55_0

SOURCES += main.cpp \
    pmath.cpp

HEADERS += \
    best_promotion.hpp \
    expression.hpp \
    expression_dict.hpp \
    expression_visitor.hpp \
    interpreter.hpp \
    mapstack.hpp \
    matrix.hpp \
    numeric_interface.hpp \
    pmath.hpp \
    reference.hpp \
    sequence.hpp \
    token.hpp \
    reference_stack.hpp \
    parameters.hpp \
    dynarraylike.hpp

OTHER_FILES += \
    .gitignore

