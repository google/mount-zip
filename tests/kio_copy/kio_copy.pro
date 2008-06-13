TEMPLATE = app
CONFIG -= moc
KDEPREF=$$system(kde-config --prefix)
INCLUDEPATH += .
INCLUDEPATH += $${KDEPREF}/include/kde

# Input
SOURCES += main.cpp

LIBS += -L$${KDEPREF}/lib -lkdecore -lkio
