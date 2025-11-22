# Installation Guide - DigitalPersona Library

## Quick Install

```bash
cd digitalpersonalib
qmake6
make
sudo make install
```

## System-wide Installation

Installing the library system-wide allows all users and projects to use it.

### 1. Build the Library

```bash
cd digitalpersonalib
qmake6
make
```

This will create:
- `lib/libdigitalpersona.so.1.0.0` - The shared library
- `lib/libdigitalpersona.so.1` - Version symlink
- `lib/libdigitalpersona.so` - Development symlink

### 2. Install System-wide

```bash
sudo make install
```

This installs:
- Library files to `/usr/local/lib/`
- Header files to `/usr/local/include/digitalpersona/`
- pkg-config file to `/usr/local/lib/pkgconfig/`

### 3. Update Library Cache

```bash
sudo ldconfig
```

### 4. Verify Installation

```bash
# Check library is found
pkg-config --modversion digitalpersona

# Check library path
pkg-config --libs digitalpersona

# Expected output: -L/usr/local/lib -ldigitalpersona
```

## Local Installation (Development)

For development without system-wide installation:

### 1. Build the Library

```bash
cd digitalpersonalib
qmake6
make
```

### 2. Use in Your Project

Add to your `.pro` file:

```qmake
# Point to local library
LIBS += -L$$PWD/../digitalpersonalib/lib -ldigitalpersona
INCLUDEPATH += $$PWD/../digitalpersonalib/include

# Set runtime library path
QMAKE_RPATHDIR += $$PWD/../digitalpersonalib/lib
```

Or set `LD_LIBRARY_PATH`:

```bash
export LD_LIBRARY_PATH=/path/to/digitalpersonalib/lib:$LD_LIBRARY_PATH
```

## Uninstall

```bash
cd digitalpersonalib
sudo make uninstall
```

Or manually:

```bash
sudo rm -rf /usr/local/include/digitalpersona
sudo rm /usr/local/lib/libdigitalpersona.so*
sudo rm /usr/local/lib/pkgconfig/digitalpersona.pc
sudo ldconfig
```

## Build Options

### Static Library

Edit `digitalpersonalib.pro`:

```qmake
# Change from:
CONFIG += shared

# To:
CONFIG += staticlib
```

Then rebuild:

```bash
make clean
qmake6
make
```

### Debug Build

```bash
qmake6 CONFIG+=debug
make
```

### Release Build (default)

```bash
qmake6 CONFIG+=release
make
```

## Troubleshooting

### Library Not Found at Runtime

```bash
# Add library path to LD_LIBRARY_PATH
export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH

# Or update ldconfig
echo "/usr/local/lib" | sudo tee /etc/ld.so.conf.d/digitalpersona.conf
sudo ldconfig
```

### Permission Denied

Ensure you have sudo privileges for system-wide installation.

### Qt6 Not Found

Install Qt6 development packages:

```bash
# Ubuntu/Debian
sudo apt install qt6-base-dev

# Fedora
sudo dnf install qt6-qtbase-devel
```

### libfprint Not Found

Install libfprint development packages:

```bash
# Ubuntu/Debian
sudo apt install libfprint-2-dev

# Fedora
sudo dnf install libfprint-devel
```

## Next Steps

After installation, see:
- `README.md` - Library documentation and API reference
- `examples/` - Example applications
- Your project's `.pro` file - How to link against the library

