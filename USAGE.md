# DigitalPersona Library - Quick Usage Guide

## 🚀 Quick Start (5 Minutes)

### Step 1: Add to Your Qt Project

**In your `.pro` file:**

```qmake
QT += core gui

# Link digitalpersona library
LIBS += -L/path/to/digitalpersonalib/lib -ldigitalpersona
INCLUDEPATH += /path/to/digitalpersonalib/include
QMAKE_RPATHDIR += /path/to/digitalpersonalib/lib
```

### Step 2: Include Header

```cpp
#include <digitalpersona.h>
```

### Step 3: Basic Usage

```cpp
// Create manager
FingerprintManager fpManager;

// Initialize
fpManager.initialize();
fpManager.openReader();

// Enroll
fpManager.startEnrollment();
QString msg;
int quality;
if (fpManager.addEnrollmentSample(msg, quality) == 1) {
    QByteArray templateData;
    fpManager.createEnrollmentTemplate(templateData);
    // Save templateData to your database
}

// Verify
int score;
if (fpManager.verifyFingerprint(templateData, score) && score >= 60) {
    qDebug() << "✓ Match!";
}

// Cleanup
fpManager.closeReader();
fpManager.cleanup();
```

---

## 📦 What's Included

### Core Components

- **`FingerprintManager`** - Main class for fingerprint operations
- **`DeviceInfo`** - Device information structure
- **`FingerprintTemplate`** - Template with metadata
- **`ProgressCallback`** - Progress update callback type

### What's NOT Included

- ❌ Database handling (you handle this)
- ❌ User management (you handle this)  
- ❌ UI components (you create these)

---

## 🎯 Common Use Cases

### Use Case 1: List Available Devices

```cpp
FingerprintManager fpManager;
fpManager.initialize();

QVector<DeviceInfo> devices = fpManager.getAvailableDevices();
for (const auto& device : devices) {
    qDebug() << device.name;
}
```

### Use Case 2: Enrollment with Progress

```cpp
fpManager.setProgressCallback([](int current, int total, QString msg) {
    qDebug() << current << "/" << total << ":" << msg;
});

fpManager.startEnrollment();
QString msg;
int quality;
fpManager.addEnrollmentSample(msg, quality); // Blocks until complete
```

### Use Case 3: Verification

```cpp
// Load from your database
QByteArray templateData = myDatabase.loadTemplate(userId);

// Verify
int score;
bool matched = fpManager.verifyFingerprint(templateData, score);

if (matched && score >= 60) {
    qDebug() << "✓ User verified!";
}
```

---

## ⚠️ Important Notes

### 1. No SQL Dependency

This library does **NOT** include any database code. You must:

```cpp
// ✅ Your responsibility
void saveFingerprint(const QString& userName, const QByteArray& templateData) {
    QSqlQuery query;
    query.prepare("INSERT INTO fingerprints (user_name, template) VALUES (?, ?)");
    query.addBindValue(userName);
    query.addBindValue(templateData);
    query.exec();
}

QByteArray loadFingerprint(int userId) {
    QSqlQuery query;
    query.prepare("SELECT template FROM fingerprints WHERE user_id = ?");
    query.addBindValue(userId);
    query.exec();
    if (query.next()) {
        return query.value(0).toByteArray();
    }
    return QByteArray();
}
```

### 2. Thread Safety

Progress callbacks run in GLib thread. Use `QMetaObject::invokeMethod` for UI updates:

```cpp
fpManager.setProgressCallback([this](int current, int total, QString msg) {
    QMetaObject::invokeMethod(this, [=]() {
        // Now safe to update UI
        m_progressBar->setValue(current);
    }, Qt::QueuedConnection);
});
```

### 3. Error Handling

Always check return values:

```cpp
if (!fpManager.initialize()) {
    qWarning() << fpManager.getLastError();
    return;
}
```

### 4. Resource Cleanup

Always cleanup:

```cpp
// In your destructor
~MyApp() {
    m_fpManager.closeReader();
    m_fpManager.cleanup();
}
```

---

## 🔧 Minimal Working Example

```cpp
#include <QCoreApplication>
#include <digitalpersona.h>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    
    FingerprintManager fpManager;
    
    // 1. Initialize
    if (!fpManager.initialize()) {
        qCritical() << "Init failed:" << fpManager.getLastError();
        return 1;
    }
    
    // 2. List devices
    int deviceCount = fpManager.getDeviceCount();
    qDebug() << "Found" << deviceCount << "device(s)";
    
    if (deviceCount == 0) {
        qCritical() << "No devices found";
        return 1;
    }
    
    // 3. Open device
    if (!fpManager.openReader()) {
        qCritical() << "Open failed:" << fpManager.getLastError();
        return 1;
    }
    
    qDebug() << "Device opened:" << fpManager.getDeviceName();
    
    // 4. Enrollment
    qDebug() << "Starting enrollment...";
    if (!fpManager.startEnrollment()) {
        qCritical() << "Enrollment start failed:" << fpManager.getLastError();
        return 1;
    }
    
    QString message;
    int quality;
    int result = fpManager.addEnrollmentSample(message, quality);
    
    if (result == 1) {
        qDebug() << "✓ Enrollment complete!";
        
        QByteArray templateData;
        if (fpManager.createEnrollmentTemplate(templateData)) {
            qDebug() << "Template size:" << templateData.size() << "bytes";
            // TODO: Save to database
        }
    } else {
        qCritical() << "Enrollment failed:" << fpManager.getLastError();
    }
    
    // 5. Cleanup
    fpManager.closeReader();
    fpManager.cleanup();
    
    return 0;
}
```

Compile with:

```bash
qmake6
make
./YourApp
```

---

## 📚 More Information

- **Full API Reference**: See `API_REFERENCE.md`
- **Examples**: Check `examples/` folder
- **Installation Guide**: See `INSTALL.md`

---

## ✅ Checklist for Integration

- [ ] Add library to `.pro` file
- [ ] Include `<digitalpersona.h>`
- [ ] Create database tables for templates
- [ ] Implement save/load template functions
- [ ] Add error handling
- [ ] Test enrollment
- [ ] Test verification
- [ ] Add cleanup in destructor
- [ ] Handle progress callbacks (if using UI)
- [ ] Test with actual device

---

**Library Version:** 1.0.0  
**Last Updated:** 2025-11-22

