# DigitalPersona Fingerprint Library

A Qt-based C++ library for integrating DigitalPersona U.are.U fingerprint readers into your applications.

## Features

- ✅ **Fingerprint Enrollment**: Multi-stage (5 scans) enrollment process
- ✅ **Fingerprint Verification**: Match fingerprints against stored templates
- ✅ **Database Integration**: SQLite database for user and template management
- ✅ **Qt Integration**: Full Qt6 support with signals/slots
- ✅ **libfprint Backend**: Native Linux support via libfprint-2
- ✅ **ARM64 Support**: Optimized for ARM64 architecture
- ✅ **Thread-Safe**: Safe for multi-threaded applications

## Supported Devices

- Digital Persona U.are.U 4000
- Digital Persona U.are.U 4000B
- Digital Persona U.are.U 4500

## Requirements

### System Requirements
- Linux (tested on Ubuntu/Debian)
- Qt6 (Core, SQL modules)
- libfprint-2
- SQLite3
- GLib 2.0

### Build Requirements
- qmake (Qt6)
- g++ with C++17 support
- pkg-config

## Installation

### Install Dependencies

```bash
# Ubuntu/Debian
sudo apt install libfprint-2-dev libglib2.0-dev qt6-base-dev libqt6sql6-sqlite

# Fedora/RHEL
sudo dnf install libfprint-devel glib2-devel qt6-qtbase-devel
```

### Build Library

```bash
cd digitalpersonalib
qmake6
make
sudo make install
```

This will install:
- Library: `/usr/local/lib/libdigitalpersona.so`
- Headers: `/usr/local/include/digitalpersona/`
- pkg-config: `/usr/local/lib/pkgconfig/digitalpersona.pc`

## Usage

### Include in Your Qt Project

Add to your `.pro` file:

```qmake
# Using pkg-config
CONFIG += link_pkgconfig
PKGCONFIG += digitalpersona

# Or manually
LIBS += -L/usr/local/lib -ldigitalpersona
INCLUDEPATH += /usr/local/include/digitalpersona
```

### Basic Example

```cpp
#include <digitalpersona.h>
#include <QCoreApplication>
#include <QDebug>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    
    // Initialize fingerprint manager
    FingerprintManager fpManager;
    if (!fpManager.initialize()) {
        qCritical() << "Failed to initialize:" << fpManager.getLastError();
        return 1;
    }
    
    // Open fingerprint reader
    if (!fpManager.openReader()) {
        qCritical() << "Failed to open reader:" << fpManager.getLastError();
        return 1;
    }
    
    qInfo() << "Fingerprint reader ready!";
    qInfo() << "Library version:" << DigitalPersona::version();
    
    // Initialize database
    DatabaseManager dbManager("myapp.db");
    if (!dbManager.initialize()) {
        qCritical() << "Database error:" << dbManager.getLastError();
        return 1;
    }
    
    return 0;
}
```

### Enrollment Example

```cpp
// Start enrollment
if (!fpManager.startEnrollment()) {
    qWarning() << "Failed to start enrollment:" << fpManager.getLastError();
    return;
}

// Capture samples (will prompt for 5 scans)
QString message;
int quality;
int result = fpManager.addEnrollmentSample(message, quality);

if (result == 1) {
    // Enrollment complete - save to database
    QByteArray templateData;
    if (fpManager.createEnrollmentTemplate(templateData)) {
        int userId;
        if (dbManager.addUser("John Doe", "john@example.com", templateData, userId)) {
            qInfo() << "User enrolled successfully! ID:" << userId;
        }
    }
    fpManager.cancelEnrollment();
}
```

### Verification Example

```cpp
// Load user from database
User user;
if (!dbManager.getUserById(userId, user)) {
    qWarning() << "User not found";
    return;
}

// Verify fingerprint
int matchScore = 0;
bool matched = fpManager.verifyFingerprint(user.fingerprintTemplate, matchScore);

if (matched && matchScore >= 60) {
    qInfo() << "Fingerprint verified! Score:" << matchScore;
} else {
    qWarning() << "Verification failed. Score:" << matchScore;
}
```

## API Reference

### FingerprintManager

| Method | Description |
|--------|-------------|
| `bool initialize()` | Initialize libfprint context |
| `void cleanup()` | Clean up resources |
| `bool openReader()` | Open the fingerprint device |
| `void closeReader()` | Close the device |
| `bool isReaderOpen()` | Check if device is open |
| `bool startEnrollment()` | Start enrollment process |
| `int addEnrollmentSample(QString& msg, int& quality)` | Capture enrollment samples (returns 1 when complete) |
| `bool createEnrollmentTemplate(QByteArray& data)` | Generate template from enrollment |
| `void cancelEnrollment()` | Cancel ongoing enrollment |
| `bool verifyFingerprint(const QByteArray& template, int& score)` | Verify fingerprint against template |
| `QString getLastError()` | Get last error message |

### DatabaseManager

| Method | Description |
|--------|-------------|
| `bool initialize()` | Initialize database and create tables |
| `bool addUser(name, email, template, userId&)` | Add new user with fingerprint |
| `bool getUserById(id, User&)` | Load user by ID |
| `QVector<User> getAllUsers()` | Get all registered users |
| `bool userExists(name)` | Check if user exists |
| `bool deleteUser(id)` | Delete user by ID |
| `QString getLastError()` | Get last error message |

## Device Permissions

Users need to be in the `plugdev` group to access the fingerprint device:

```bash
sudo usermod -aG plugdev $USER
# Log out and log back in for changes to take effect
```

## Troubleshooting

### Device Not Found
- Check USB connection
- Verify user is in `plugdev` group
- Run `lsusb` to confirm device is detected

### Compilation Errors
- Ensure Qt6 is installed
- Check libfprint-2-dev is installed
- Verify pkg-config can find dependencies

### Runtime Errors
- Check library path: `export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH`
- Verify database file permissions
- Check device permissions

## Architecture

```
┌─────────────────────────────────┐
│   Your Qt Application           │
├─────────────────────────────────┤
│   DigitalPersona Library         │
│   ┌───────────────────────────┐ │
│   │ FingerprintManager        │ │
│   │ - Enrollment              │ │
│   │ - Verification            │ │
│   └───────────────────────────┘ │
│   ┌───────────────────────────┐ │
│   │ DatabaseManager           │ │
│   │ - User Management         │ │
│   │ - Template Storage        │ │
│   └───────────────────────────┘ │
├─────────────────────────────────┤
│   libfprint-2                    │
├─────────────────────────────────┤
│   Hardware (U.are.U Device)      │
└─────────────────────────────────┘
```

## License

This library is provided as-is for use with DigitalPersona fingerprint readers.

## Support

For issues and questions:
- Check the examples directory
- Review API documentation
- Check device compatibility

## Version History

### v1.0.0 (2025)
- Initial release
- Enrollment and verification support
- SQLite database integration
- libfprint-2 backend
- ARM64 optimization

## Contributing

Contributions are welcome! Please ensure:
- Code follows Qt coding style
- All tests pass
- Documentation is updated
- Commit messages are descriptive

## Authors

- Arkana Development Team

---

**Note**: This library requires a compatible DigitalPersona U.are.U fingerprint reader connected via USB.

