# DigitalPersona Fingerprint Library - API Reference

**Version:** 1.0.0  
**License:** LGPL-3  
**Platform:** Linux (ARM64, x86_64)  
**Qt Version:** Qt6+

---

## Table of Contents

1. [Overview](#overview)
2. [Installation](#installation)
3. [Quick Start](#quick-start)
4. [Data Structures](#data-structures)
5. [FingerprintManager Class](#fingerprintmanager-class)
6. [Error Handling](#error-handling)
7. [Examples](#examples)
8. [Best Practices](#best-practices)

---

## Overview

The **DigitalPersona Fingerprint Library** provides a high-level, Qt-friendly API for interacting with fingerprint readers. It is built on top of `libfprint-2` and specifically tested with U.are.U 4500 devices.

### Key Features

- ✅ **Database Independent** - No database coupling, bring your own storage
- ✅ **Thread-Safe Callbacks** - Progress updates via Qt signals/slots compatible callbacks
- ✅ **Multi-Device Support** - List and select from multiple fingerprint readers
- ✅ **Modern C++17** - Clean, documented API with RAII principles
- ✅ **Comprehensive Error Handling** - Clear error messages for debugging

### What's NOT Included

- ❌ Database/Storage Management (application responsibility)
- ❌ User Management (application responsibility)
- ❌ UI Components (application responsibility)

---

## Installation

### System Requirements

```bash
# Required libraries
sudo apt install libfprint-2-dev libglib2.0-dev qt6-base-dev
```

### Building the Library

```bash
cd digitalpersonalib/
qmake6
make -j$(nproc)
sudo make install  # Optional: install system-wide
```

### Using in Your Project

#### QMake (`.pro` file)

```qmake
QT += core gui

# Link against digitalpersona library
LIBS += -L/path/to/digitalpersonalib/lib -ldigitalpersona
INCLUDEPATH += /path/to/digitalpersonalib/include
QMAKE_RPATHDIR += /path/to/digitalpersonalib/lib
```

#### CMake

```cmake
find_library(DIGITALPERSONA_LIB digitalpersona PATHS /path/to/digitalpersonalib/lib)
target_include_directories(your_target PRIVATE /path/to/digitalpersonalib/include)
target_link_libraries(your_target PRIVATE ${DIGITALPERSONA_LIB})
```

---

## Quick Start

### Basic Enrollment Example

```cpp
#include <digitalpersona.h>

// 1. Create and initialize
FingerprintManager fpManager;
if (!fpManager.initialize()) {
    qWarning() << "Init failed:" << fpManager.getLastError();
    return;
}

// 2. Open device
if (!fpManager.openReader()) {
    qWarning() << "Open failed:" << fpManager.getLastError();
    return;
}

// 3. Start enrollment
if (!fpManager.startEnrollment()) {
    qWarning() << "Enrollment start failed:" << fpManager.getLastError();
    return;
}

// 4. Capture samples (blocks until complete)
QString message;
int quality;
int result = fpManager.addEnrollmentSample(message, quality);

if (result == 1) {
    // 5. Get template data
    QByteArray templateData;
    if (fpManager.createEnrollmentTemplate(templateData)) {
        // Save templateData to your database
        saveToDatabase(userName, templateData);
    }
}

// 6. Cleanup
fpManager.closeReader();
fpManager.cleanup();
```

### Basic Verification Example

```cpp
// 1. Setup (same as enrollment)
FingerprintManager fpManager;
fpManager.initialize();
fpManager.openReader();

// 2. Load template from your database
QByteArray templateData = loadFromDatabase(userId);

// 3. Verify fingerprint
int score = 0;
bool matched = fpManager.verifyFingerprint(templateData, score);

if (matched && score >= 60) {
    qDebug() << "✓ Match! Score:" << score;
} else {
    qDebug() << "✗ No match. Score:" << score;
}

// 4. Cleanup
fpManager.closeReader();
fpManager.cleanup();
```

---

## Data Structures

### DeviceInfo

Information about a fingerprint reader device.

```cpp
struct DeviceInfo {
    QString name;           // Device name (e.g. "Digital Persona U.are.U 4500")
    QString driver;         // Driver name (e.g. "uru4000")
    QString deviceId;       // Device ID string
    bool isOpen;            // Is device currently open
    bool supportsCapture;   // Supports image capture
    bool supportsIdentify;  // Supports identification
};
```

### FingerprintTemplate

Fingerprint template with metadata.

```cpp
struct FingerprintTemplate {
    QByteArray data;        // Raw template data (serialized FpPrint)
    int qualityScore;       // Quality score (0-100)
    QDateTime timestamp;    // When template was created
    int scanCount;          // Number of scans used in enrollment
};
```

### ProgressCallback

Callback function type for enrollment progress updates.

```cpp
typedef std::function<void(int currentStage, int totalStages, QString message)> ProgressCallback;

// Usage example:
fpManager.setProgressCallback([](int current, int total, QString msg) {
    qDebug() << "Progress:" << current << "/" << total << "-" << msg;
});
```

---

## FingerprintManager Class

### Initialization & Cleanup

#### `bool initialize()`

Initialize the fingerprint library context. Must be called before any other operations.

**Returns:** `true` on success, `false` on failure

**Example:**
```cpp
FingerprintManager fpManager;
if (!fpManager.initialize()) {
    qWarning() << fpManager.getLastError();
}
```

---

#### `void cleanup()`

Cleanup all resources. Closes any open readers and releases the library context.

**Example:**
```cpp
fpManager.cleanup();
```

---

### Device Management

#### `int getDeviceCount()`

Get the number of available fingerprint readers.

**Returns:** Number of devices (0 if none found)

**Example:**
```cpp
int count = fpManager.getDeviceCount();
qDebug() << "Found" << count << "devices";
```

---

#### `QVector<DeviceInfo> getAvailableDevices()`

Get detailed information about all available devices.

**Returns:** Vector of `DeviceInfo` structures

**Example:**
```cpp
QVector<DeviceInfo> devices = fpManager.getAvailableDevices();
for (const auto& device : devices) {
    qDebug() << "Device:" << device.name;
    qDebug() << "  Driver:" << device.driver;
    qDebug() << "  Supports capture:" << device.supportsCapture;
}
```

---

#### `bool openReader()`

Open the first available fingerprint reader.

**Returns:** `true` on success, `false` on failure

**Example:**
```cpp
if (!fpManager.openReader()) {
    qWarning() << "Failed to open reader:" << fpManager.getLastError();
}
```

---

#### `bool openReader(int deviceIndex)`

Open a specific fingerprint reader by index.

**Parameters:**
- `deviceIndex`: Index of the device (0-based)

**Returns:** `true` on success, `false` on failure

**Example:**
```cpp
// Open second device
if (!fpManager.openReader(1)) {
    qWarning() << "Failed to open device 1:" << fpManager.getLastError();
}
```

---

#### `void closeReader()`

Close the currently open reader.

**Example:**
```cpp
fpManager.closeReader();
```

---

#### `bool isReaderOpen() const`

Check if a reader is currently open.

**Returns:** `true` if open, `false` otherwise

---

#### `QString getDeviceName() const`

Get the name of the currently open device.

**Returns:** Device name, or empty string if no device is open

**Example:**
```cpp
if (fpManager.isReaderOpen()) {
    qDebug() << "Current device:" << fpManager.getDeviceName();
}
```

---

#### `DeviceInfo getCurrentDeviceInfo() const`

Get detailed information about the currently open device.

**Returns:** `DeviceInfo` structure

**Example:**
```cpp
DeviceInfo info = fpManager.getCurrentDeviceInfo();
qDebug() << "Device:" << info.name;
qDebug() << "Driver:" << info.driver;
```

---

### Enrollment Operations

#### `bool startEnrollment()`

Start a new enrollment session. Must be called before capturing samples.

**Returns:** `true` on success, `false` on failure

**Example:**
```cpp
if (!fpManager.startEnrollment()) {
    qWarning() << "Failed to start enrollment:" << fpManager.getLastError();
}
```

---

#### `int addEnrollmentSample(QString& message, int& quality, QImage* image = nullptr)`

Capture enrollment samples. **This method blocks** until all required scans are complete or an error occurs.

**Parameters:**
- `message` [out]: Status message describing the result
- `quality` [out]: Quality score (0-100)
- `image` [out, optional]: Pointer to receive fingerprint image (if device supports it)

**Returns:**
- `1`: Enrollment complete
- `0`: More samples needed (shouldn't happen with current implementation)
- `-1`: Error occurred

**Example:**
```cpp
QString message;
int quality;
int result = fpManager.addEnrollmentSample(message, quality);

if (result == 1) {
    qDebug() << "Enrollment complete!";
    qDebug() << "Quality:" << quality;
} else if (result == -1) {
    qWarning() << "Error:" << fpManager.getLastError();
}
```

---

#### `bool createEnrollmentTemplate(QByteArray& templateData)`

Create a fingerprint template from the completed enrollment.

**Parameters:**
- `templateData` [out]: Output buffer for template data

**Returns:** `true` on success, `false` on failure

**Example:**
```cpp
QByteArray templateData;
if (fpManager.createEnrollmentTemplate(templateData)) {
    qDebug() << "Template size:" << templateData.size() << "bytes";
    // Save to database
    db.saveTemplate(userId, templateData);
}
```

---

#### `bool createEnrollmentTemplate(FingerprintTemplate& fpTemplate)`

Create a fingerprint template with metadata.

**Parameters:**
- `fpTemplate` [out]: Output `FingerprintTemplate` structure

**Returns:** `true` on success, `false` on failure

**Example:**
```cpp
FingerprintTemplate fpTemplate;
if (fpManager.createEnrollmentTemplate(fpTemplate)) {
    qDebug() << "Template size:" << fpTemplate.data.size();
    qDebug() << "Quality:" << fpTemplate.qualityScore;
    qDebug() << "Timestamp:" << fpTemplate.timestamp.toString();
    qDebug() << "Scans:" << fpTemplate.scanCount;
}
```

---

#### `void cancelEnrollment()`

Cancel the current enrollment session and release resources.

**Example:**
```cpp
fpManager.cancelEnrollment();
```

---

#### `bool isEnrollmentInProgress() const`

Check if an enrollment session is currently in progress.

**Returns:** `true` if in progress, `false` otherwise

---

### Progress Callbacks

#### `void setProgressCallback(ProgressCallback callback)`

Set a callback function to receive enrollment progress updates.

**Parameters:**
- `callback`: Function with signature `void(int currentStage, int totalStages, QString message)`

**Example:**
```cpp
fpManager.setProgressCallback([this](int current, int total, QString msg) {
    // Update UI with progress
    m_progressBar->setValue(current);
    m_statusLabel->setText(msg);
    qDebug() << "Enrollment:" << current << "/" << total;
});
```

**Thread Safety:** The callback is invoked from a GLib thread. Use `QMetaObject::invokeMethod` with `Qt::QueuedConnection` to update UI safely:

```cpp
fpManager.setProgressCallback([this](int current, int total, QString msg) {
    QMetaObject::invokeMethod(this, [this, current, total, msg]() {
        // Now safe to update UI
        updateProgressUI(current, total, msg);
    }, Qt::QueuedConnection);
});
```

---

#### `int getCurrentEnrollmentStage() const`

Get the current enrollment stage number.

**Returns:** Current stage (0-based)

---

#### `int getTotalEnrollmentStages() const`

Get the total number of enrollment stages required.

**Returns:** Total stages (typically 5 for U.are.U devices)

---

### Verification Operations

#### `bool verifyFingerprint(const QByteArray& templateData, int& score)`

Verify a fingerprint against a stored template. **This method blocks** until verification is complete.

**Parameters:**
- `templateData`: Template data to verify against
- `score` [out]: Match score (0-100), higher is better

**Returns:** `true` if fingerprint matches, `false` otherwise

**Example:**
```cpp
QByteArray templateData = db.loadTemplate(userId);
int score = 0;

bool matched = fpManager.verifyFingerprint(templateData, score);

if (matched && score >= 60) {
    qDebug() << "✓ Verified! Score:" << score;
} else {
    qDebug() << "✗ Not verified. Score:" << score;
}
```

---

#### `bool verifyFingerprint(const FingerprintTemplate& fpTemplate, int& score)`

Verify using a `FingerprintTemplate` structure.

**Parameters:**
- `fpTemplate`: Template structure with data and metadata
- `score` [out]: Match score (0-100)

**Returns:** `true` if fingerprint matches, `false` otherwise

**Example:**
```cpp
FingerprintTemplate fpTemplate = db.loadTemplateWithMetadata(userId);
int score = 0;

bool matched = fpManager.verifyFingerprint(fpTemplate, score);
```

---

### Error Handling

#### `QString getLastError() const`

Get the last error message.

**Returns:** Error message string, or empty if no error

**Example:**
```cpp
if (!fpManager.initialize()) {
    qWarning() << "Error:" << fpManager.getLastError();
}
```

---

#### `bool hasError() const`

Check if the last operation had an error.

**Returns:** `true` if there was an error, `false` otherwise

---

#### `void clearError()`

Clear the last error message.

**Example:**
```cpp
fpManager.clearError();
```

---

## Error Handling

### Common Error Scenarios

| Error | Cause | Solution |
|-------|-------|----------|
| "Context not initialized" | `initialize()` not called | Call `initialize()` first |
| "No fingerprint readers found" | No devices connected | Check USB connection and permissions |
| "Device not open" | Forgot to call `openReader()` | Call `openReader()` before operations |
| "Failed to open device: Access denied" | Permission issue | Add user to `plugdev` group, check udev rules |
| "Enrollment not started" | Forgot to call `startEnrollment()` | Call `startEnrollment()` before capturing |
| "No enrollment data" | Template requested before completion | Wait for `addEnrollmentSample()` to return `1` |

### Error Handling Pattern

```cpp
FingerprintManager fpManager;

// Always check return values
if (!fpManager.initialize()) {
    qCritical() << "Initialization failed:" << fpManager.getLastError();
    return false;
}

if (!fpManager.openReader()) {
    qCritical() << "Failed to open reader:" << fpManager.getLastError();
    fpManager.cleanup();
    return false;
}

// ... perform operations ...

// Always cleanup
fpManager.closeReader();
fpManager.cleanup();
```

---

## Examples

### Example 1: List All Available Devices

```cpp
#include <digitalpersona.h>
#include <QDebug>

void listDevices()
{
    FingerprintManager fpManager;
    
    if (!fpManager.initialize()) {
        qWarning() << "Failed to initialize:" << fpManager.getLastError();
        return;
    }
    
    QVector<DeviceInfo> devices = fpManager.getAvailableDevices();
    
    qDebug() << "Found" << devices.size() << "fingerprint reader(s):";
    
    for (int i = 0; i < devices.size(); ++i) {
        const DeviceInfo& device = devices[i];
        qDebug() << "\nDevice" << i << ":";
        qDebug() << "  Name:" << device.name;
        qDebug() << "  Driver:" << device.driver;
        qDebug() << "  Device ID:" << device.deviceId;
        qDebug() << "  Open:" << (device.isOpen ? "Yes" : "No");
        qDebug() << "  Supports Capture:" << (device.supportsCapture ? "Yes" : "No");
        qDebug() << "  Supports Identify:" << (device.supportsIdentify ? "Yes" : "No");
    }
    
    fpManager.cleanup();
}
```

### Example 2: Enrollment with Progress Callback

```cpp
#include <digitalpersona.h>
#include <QDebug>

void enrollWithProgress()
{
    FingerprintManager fpManager;
    
    // Initialize
    if (!fpManager.initialize() || !fpManager.openReader()) {
        qWarning() << "Setup failed:" << fpManager.getLastError();
        return;
    }
    
    // Set progress callback
    fpManager.setProgressCallback([](int current, int total, QString msg) {
        qDebug() << QString("[%1/%2] %3").arg(current).arg(total).arg(msg);
    });
    
    // Start enrollment
    if (!fpManager.startEnrollment()) {
        qWarning() << "Failed to start:" << fpManager.getLastError();
        return;
    }
    
    qDebug() << "Place your finger on the reader...";
    
    // Capture samples
    QString message;
    int quality;
    int result = fpManager.addEnrollmentSample(message, quality);
    
    if (result == 1) {
        qDebug() << "✓ Enrollment successful!";
        
        // Get template
        FingerprintTemplate fpTemplate;
        if (fpManager.createEnrollmentTemplate(fpTemplate)) {
            qDebug() << "Template size:" << fpTemplate.data.size() << "bytes";
            qDebug() << "Quality:" << fpTemplate.qualityScore;
            qDebug() << "Scans:" << fpTemplate.scanCount;
            
            // Save to your database here
            // myDatabase.save(userName, fpTemplate.data);
        }
    } else {
        qWarning() << "Enrollment failed:" << fpManager.getLastError();
    }
    
    // Cleanup
    fpManager.closeReader();
    fpManager.cleanup();
}
```

### Example 3: Verification Loop

```cpp
#include <digitalpersona.h>
#include <QDebug>
#include <QVector>

void verifyUsers(const QVector<QPair<QString, QByteArray>>& users)
{
    FingerprintManager fpManager;
    
    if (!fpManager.initialize() || !fpManager.openReader()) {
        qWarning() << "Setup failed:" << fpManager.getLastError();
        return;
    }
    
    qDebug() << "Place finger on reader for verification...";
    
    // Try to match against all stored users
    for (const auto& user : users) {
        int score = 0;
        bool matched = fpManager.verifyFingerprint(user.second, score);
        
        if (matched && score >= 60) {
            qDebug() << "✓ MATCH! User:" << user.first << "- Score:" << score;
            break;
        }
    }
    
    qDebug() << "Verification complete.";
    
    fpManager.closeReader();
    fpManager.cleanup();
}
```

---

## Best Practices

### 1. Resource Management

Always use RAII pattern or explicit cleanup:

```cpp
// Good: Cleanup in destructor
class MyFingerprintApp {
    FingerprintManager m_fpManager;
public:
    ~MyFingerprintApp() {
        m_fpManager.closeReader();
        m_fpManager.cleanup();
    }
};

// Or: Explicit cleanup
FingerprintManager fpManager;
// ... use fpManager ...
fpManager.closeReader();
fpManager.cleanup();
```

### 2. Error Checking

Always check return values:

```cpp
// ✅ Good
if (!fpManager.initialize()) {
    handleError(fpManager.getLastError());
    return;
}

// ❌ Bad
fpManager.initialize(); // Ignoring return value
```

### 3. Thread Safety

Use `QMetaObject::invokeMethod` for UI updates from callbacks:

```cpp
fpManager.setProgressCallback([this](int current, int total, QString msg) {
    QMetaObject::invokeMethod(this, [=]() {
        updateUI(current, total, msg);
    }, Qt::QueuedConnection);
});
```

### 4. Template Storage

Store templates as BLOB in database:

```sql
CREATE TABLE fingerprints (
    id INTEGER PRIMARY KEY,
    user_id INTEGER,
    template BLOB,
    quality_score INTEGER,
    created_at DATETIME,
    FOREIGN KEY (user_id) REFERENCES users(id)
);
```

### 5. Verification Threshold

Use appropriate match score threshold (recommended: 60-70):

```cpp
const int MATCH_THRESHOLD = 60;

if (matched && score >= MATCH_THRESHOLD) {
    // Accept as valid match
}
```

### 6. Multiple Devices

List devices first, let user choose:

```cpp
QVector<DeviceInfo> devices = fpManager.getAvailableDevices();
int selectedIndex = userSelectDevice(devices);
fpManager.openReader(selectedIndex);
```

---

## License

This library is licensed under LGPL-3.

---

## Support

For issues and questions:
- Check the examples in `/digitalpersonalib/examples/`
- Read the API documentation
- Check device permissions (`/etc/udev/rules.d/`)

---

**Generated:** 2025-11-22  
**Library Version:** 1.0.0

