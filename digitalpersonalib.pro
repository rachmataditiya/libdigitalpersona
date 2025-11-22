TEMPLATE = lib
TARGET = digitalpersona
VERSION = 1.0.0

CONFIG += qt shared c++17
QT += core gui

# Output directories
DESTDIR = $$PWD/lib
OBJECTS_DIR = $$PWD/build/obj
MOC_DIR = $$PWD/build/moc

# Library type
CONFIG += shared

# Define export macro
DEFINES += DIGITALPERSONA_LIBRARY

# Include paths
INCLUDEPATH += $$PWD/include

# Linux settings
unix:!macx {
    CONFIG += link_pkgconfig
    PKGCONFIG += libfprint-2 glib-2.0 gobject-2.0 gio-2.0
}

# macOS settings
macx {
    # Link to libfprint (compiled from source)
    INCLUDEPATH += $$PWD/../libfprint_repo/libfprint \
                   $$PWD/../libfprint_repo/libfprint/nbis/include \
                   $$PWD/../libfprint_repo/builddir/libfprint
                   
    LIBS += -L$$PWD/../libfprint_repo/builddir/libfprint -lfprint-2

    # Dependencies from Homebrew
    INCLUDEPATH += /opt/homebrew/include/glib-2.0 \
                   /opt/homebrew/lib/glib-2.0/include \
                   /opt/homebrew/include/gusb-1 \
                   /opt/homebrew/include/libusb-1.0 \
                   /opt/homebrew/include/pixman-1 \
                   /opt/homebrew/include/json-glib-1.0

    LIBS += -L/opt/homebrew/lib \
            -L/opt/homebrew/Cellar/libgusb/0.4.9/lib \
            -L/opt/homebrew/Cellar/libusb/1.0.29/lib \
            -L/opt/homebrew/Cellar/json-glib/1.10.8/lib \
            -L/opt/homebrew/Cellar/glib/2.86.1/lib \
            -L/opt/homebrew/opt/gettext/lib \
            -lglib-2.0 -lgobject-2.0 -lgio-2.0 \
            -lgusb -lusb-1.0 -lpixman-1 -lcairo -ljson-glib-1.0 -lintl
}


# Headers
HEADERS += \
    include/digitalpersona.h \
    include/digitalpersona_global.h \
    include/fingerprint_manager.h

# Sources
SOURCES += \
    src/fingerprint_manager.cpp

# Install rules
headers.files = $$HEADERS
headers.path = /usr/local/include/digitalpersona

target.path = /usr/local/lib

INSTALLS += target headers

# Generate pkg-config file
CONFIG += create_pc create_prl no_install_prl

QMAKE_PKGCONFIG_NAME = digitalpersona
QMAKE_PKGCONFIG_DESCRIPTION = DigitalPersona Fingerprint Library
QMAKE_PKGCONFIG_PREFIX = /usr/local
QMAKE_PKGCONFIG_LIBDIR = /usr/local/lib
QMAKE_PKGCONFIG_INCDIR = /usr/local/include/digitalpersona
QMAKE_PKGCONFIG_VERSION = $$VERSION
QMAKE_PKGCONFIG_DESTDIR = pkgconfig

