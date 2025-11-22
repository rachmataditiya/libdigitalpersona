# API Reference - DigitalPersona Library v1.0.0

## Table of Contents

1. [FingerprintManager](#fingerprintmanager)
2. [DatabaseManager](#databasemanager)
3. [Data Structures](#data-structures)
4. [Error Handling](#error-handling)
5. [Constants](#constants)

---

## FingerprintManager

Manages fingerprint reader operations including enrollment and verification.

### Constructor

```cpp
FingerprintManager()
```

Creates a new fingerprint manager instance.

### Destructor

```cpp
~FingerprintManager()
```

Automatically cleans up resources and closes the device.

---

### Initialization Methods

#### `initialize()`

```cpp
bool initialize()
```

Initialize libfprint context and discover devices.

**Returns:** `true` if successful, `false` on error

**Example:**
```cpp
FingerprintManager fpManager;
if (!fpManager.initialize()) {
    qWarning() << "Init failed:" << fpManager.getLastError();
}
```

---

#### `cleanup()`

```cpp
void cleanup()
```

Clean up libfprint context and release resources.

**Example:**
```cpp
fpManager.cleanup();
```

---

### Device Methods

#### `openReader()`

```cpp
bool openReader()
```

Open the fingerprint device for operations.

**Returns:** `true` if device opened successfully

**Example:**
```cpp
if (fpManager.openReader()) {
    qInfo() << "Device ready";
}
```

---

#### `closeReader()`

```cpp
void closeReader()
```

Close the fingerprint device.

**Example:**
```cpp
fpManager.closeReader();
```

---

#### `isReaderOpen()`

```cpp
bool isReaderOpen() const
```

Check if device is currently open.

**Returns:** `true` if device is open

---

### Enrollment Methods

#### `startEnrollment()`

```cpp
bool startEnrollment()
```

Start the enrollment process. Must be called before `addEnrollmentSample()`.

**Returns:** `true` if enrollment started successfully

**Example:**
```cpp
if (fpManager.startEnrollment()) {
    // Ready to capture samples
}
```

---

#### `addEnrollmentSample()`

```cpp
int addEnrollmentSample(QString& message, int& quality)
```

Capture fingerprint samples for enrollment. This method will capture **5 scans** automatically.

**Parameters:**
- `message` - [out] Status message for UI display
- `quality` - [out] Quality score (0-100)

**Returns:**
- `-1` - Error occurred (check `getLastError()`)
- `0` - Need more samples (not used in current implementation)
- `1` - Enrollment complete

**Example:**
```cpp
QString message;
int quality;
int result = fpManager.addEnrollmentSample(message, quality);

if (result == 1) {
    qInfo() << "Enrollment complete!";
    // Create template...
}
```

**Console Output:**
```
=== ENROLLMENT STARTED ===
You will need to scan your finger 5 times
✓ SCAN 1/5 Complete - Lift finger and place again...
✓ SCAN 2/5 Complete - Lift finger and place again...
...
✓ SCAN 5/5 Complete - Processing fingerprint template...
=== ENROLLMENT COMPLETED SUCCESSFULLY ===
```

---

#### `createEnrollmentTemplate()`

```cpp
bool createEnrollmentTemplate(QByteArray& templateData)
```

Generate binary template from completed enrollment.

**Parameters:**
- `templateData` - [out] Serialized fingerprint template

**Returns:** `true` if template created successfully

**Example:**
```cpp
QByteArray templateData;
if (fpManager.createEnrollmentTemplate(templateData)) {
    qInfo() << "Template size:" << templateData.size();
    // Save to database...
}
```

---

#### `cancelEnrollment()`

```cpp
void cancelEnrollment()
```

Cancel ongoing enrollment and free resources.

**Example:**
```cpp
fpManager.cancelEnrollment();
```

---

### Verification Methods

#### `verifyFingerprint()`

```cpp
bool verifyFingerprint(const QByteArray& storedTemplate, int& matchScore)
```

Verify a fingerprint against a stored template.

**Parameters:**
- `storedTemplate` - Fingerprint template from database
- `matchScore` - [out] Match score (0-100)

**Returns:** `true` if fingerprint matches (score >= 60)

**Example:**
```cpp
int score = 0;
bool matched = fpManager.verifyFingerprint(user.template, score);

if (matched && score >= 60) {
    qInfo() << "Verified! Score:" << score;
} else {
    qWarning() << "Not verified. Score:" << score;
}
```

**Match Threshold:**
- Score >= 60: Consider as match
- Score < 60: No match

---

### Error Handling

#### `getLastError()`

```cpp
QString getLastError() const
```

Get description of the last error that occurred.

**Returns:** Error message string

**Example:**
```cpp
if (!fpManager.openReader()) {
    qCritical() << fpManager.getLastError();
}
```

---

## DatabaseManager

Manages SQLite database for user and fingerprint template storage.

### Constructor

```cpp
explicit DatabaseManager(const QString& dbPath = "fingerprint.db", 
                        QObject* parent = nullptr)
```

**Parameters:**
- `dbPath` - Path to SQLite database file
- `parent` - Qt parent object

**Example:**
```cpp
DatabaseManager dbManager("myapp.db");
```

---

### Initialization

#### `initialize()`

```cpp
bool initialize()
```

Initialize database connection and create tables if needed.

**Returns:** `true` if successful

**Example:**
```cpp
if (!dbManager.initialize()) {
    qCritical() << dbManager.getLastError();
}
```

---

#### `isOpen()`

```cpp
bool isOpen() const
```

Check if database connection is open.

**Returns:** `true` if database is open

---

### User Management

#### `addUser()`

```cpp
bool addUser(const QString& name, 
             const QString& email, 
             const QByteArray& fingerprintTemplate, 
             int& userId)
```

Add a new user with fingerprint template.

**Parameters:**
- `name` - User's full name (required)
- `email` - User's email (optional)
- `fingerprintTemplate` - Binary template data
- `userId` - [out] ID of created user

**Returns:** `true` if user added successfully

**Example:**
```cpp
int userId;
if (dbManager.addUser("John Doe", "john@example.com", template, userId)) {
    qInfo() << "User created with ID:" << userId;
}
```

---

#### `getUserById()`

```cpp
bool getUserById(int userId, User& user)
```

Load user data by ID.

**Parameters:**
- `userId` - User ID to load
- `user` - [out] User structure to populate

**Returns:** `true` if user found

**Example:**
```cpp
User user;
if (dbManager.getUserById(123, user)) {
    qInfo() << "User:" << user.name;
}
```

---

#### `getAllUsers()`

```cpp
QVector<User> getAllUsers()
```

Get all registered users.

**Returns:** Vector of User structures

**Example:**
```cpp
QVector<User> users = dbManager.getAllUsers();
for (const User& user : users) {
    qInfo() << user.id << user.name;
}
```

---

#### `userExists()`

```cpp
bool userExists(const QString& name)
```

Check if user with given name exists.

**Parameters:**
- `name` - Username to check

**Returns:** `true` if user exists

**Example:**
```cpp
if (dbManager.userExists("John Doe")) {
    qWarning() << "User already exists";
}
```

---

#### `deleteUser()`

```cpp
bool deleteUser(int userId)
```

Delete user by ID.

**Parameters:**
- `userId` - ID of user to delete

**Returns:** `true` if deleted successfully

**Example:**
```cpp
if (dbManager.deleteUser(123)) {
    qInfo() << "User deleted";
}
```

---

#### `getLastError()`

```cpp
QString getLastError() const
```

Get description of the last database error.

**Returns:** Error message string

---

## Data Structures

### User

```cpp
struct User {
    int id;                          // User ID (primary key)
    QString name;                    // Full name
    QString email;                   // Email address
    QByteArray fingerprintTemplate;  // Binary template data
    QString createdAt;               // ISO 8601 timestamp
    QString updatedAt;               // ISO 8601 timestamp
};
```

**Example:**
```cpp
User user;
dbManager.getUserById(1, user);
qInfo() << "Name:" << user.name;
qInfo() << "Template size:" << user.fingerprintTemplate.size();
qInfo() << "Created:" << user.createdAt;
```

---

## Error Handling

Both `FingerprintManager` and `DatabaseManager` provide `getLastError()` method:

```cpp
if (!fpManager.initialize()) {
    QString error = fpManager.getLastError();
    qCritical() << "Error:" << error;
    // Handle error...
}
```

**Common Errors:**

| Error | Cause | Solution |
|-------|-------|----------|
| "Failed to initialize libfprint" | Library not installed | Install libfprint-2-dev |
| "No devices found" | Device not connected | Check USB connection |
| "Failed to open device" | Permission denied | Add user to `plugdev` group |
| "Device not open" | Reader not opened | Call `openReader()` first |
| "Enrollment not started" | Not in enrollment mode | Call `startEnrollment()` |
| "Database error: ..." | SQL error | Check database file permissions |

---

## Constants

### Match Threshold

```cpp
const int MATCH_THRESHOLD = 60;  // Minimum score for match (0-100)
```

**Usage:**
```cpp
if (matchScore >= 60) {
    // Consider as verified
}
```

### Enrollment Stages

```cpp
const int ENROLLMENT_STAGES = 5;  // Number of finger scans required
```

---

## Complete Example

```cpp
#include <digitalpersona.h>
#include <QDebug>

int main() {
    // Initialize
    FingerprintManager fpManager;
    fpManager.initialize();
    fpManager.openReader();
    
    DatabaseManager dbManager("app.db");
    dbManager.initialize();
    
    // Enroll
    fpManager.startEnrollment();
    QString msg;
    int quality;
    if (fpManager.addEnrollmentSample(msg, quality) == 1) {
        QByteArray tpl;
        fpManager.createEnrollmentTemplate(tpl);
        
        int userId;
        dbManager.addUser("User", "user@email.com", tpl, userId);
        qInfo() << "Enrolled! ID:" << userId;
    }
    
    // Verify
    User user;
    dbManager.getUserById(1, user);
    
    int score;
    if (fpManager.verifyFingerprint(user.fingerprintTemplate, score)) {
        qInfo() << "Verified! Score:" << score;
    }
    
    // Cleanup
    fpManager.closeReader();
    fpManager.cleanup();
    
    return 0;
}
```

---

## Version Information

```cpp
const char* DigitalPersona::version();      // Returns "1.0.0"
int DigitalPersona::versionInt();            // Returns 10000
```

---

For more examples, see the `examples/` directory in the library distribution.

