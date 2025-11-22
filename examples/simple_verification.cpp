/**
 * @file simple_verification.cpp
 * @brief Simple example demonstrating fingerprint verification
 * 
 * This example shows how to:
 * 1. Initialize the fingerprint manager
 * 2. Load a user from database
 * 3. Verify a fingerprint against stored template
 * 
 * Note: You must run simple_enrollment first to create a user!
 * 
 * Compile:
 *   qmake6 && make
 *   
 * Run:
 *   ./simple_verification [user_id]
 */

#include <digitalpersona.h>
#include <QCoreApplication>
#include <QDebug>
#include <QTimer>

void verifyUser(int userId) {
    qInfo() << "=== DigitalPersona Fingerprint Verification Example ===";
    qInfo() << "Library version:" << DigitalPersona::version();
    qInfo() << "";
    
    // Initialize fingerprint manager
    FingerprintManager fpManager;
    if (!fpManager.initialize()) {
        qCritical() << "❌ Failed to initialize:" << fpManager.getLastError();
        QCoreApplication::exit(1);
        return;
    }
    qInfo() << "✓ Fingerprint manager initialized";
    
    // Open device
    if (!fpManager.openReader()) {
        qCritical() << "❌ Failed to open reader:" << fpManager.getLastError();
        fpManager.cleanup();
        QCoreApplication::exit(1);
        return;
    }
    qInfo() << "✓ Fingerprint reader opened";
    qInfo() << "";
    
    // Initialize database
    DatabaseManager dbManager("enrollment_example.db");
    if (!dbManager.initialize()) {
        qCritical() << "❌ Database error:" << dbManager.getLastError();
        fpManager.closeReader();
        fpManager.cleanup();
        QCoreApplication::exit(1);
        return;
    }
    qInfo() << "✓ Database initialized";
    
    // Load user
    User user;
    if (!dbManager.getUserById(userId, user)) {
        qCritical() << "❌ User not found with ID:" << userId;
        qInfo() << "";
        qInfo() << "Available users:";
        QVector<User> allUsers = dbManager.getAllUsers();
        for (const User& u : allUsers) {
            qInfo() << "  - ID:" << u.id << "| Name:" << u.name;
        }
        fpManager.closeReader();
        fpManager.cleanup();
        QCoreApplication::exit(1);
        return;
    }
    
    qInfo() << "✓ User loaded:";
    qInfo() << "  - ID:" << user.id;
    qInfo() << "  - Name:" << user.name;
    qInfo() << "  - Email:" << user.email;
    qInfo() << "";
    
    // Verify fingerprint
    qInfo() << "📌 VERIFICATION STARTED";
    qInfo() << "Please place your finger on the reader...";
    qInfo() << "";
    
    int matchScore = 0;
    bool matched = fpManager.verifyFingerprint(user.fingerprintTemplate, matchScore);
    
    qInfo() << "";
    if (matched && matchScore >= 60) {
        qInfo() << "✓ FINGERPRINT VERIFIED!";
        qInfo() << "  - User:" << user.name;
        qInfo() << "  - Match score:" << matchScore << "%";
        qInfo() << "  - Result: MATCH ✓";
    } else {
        qWarning() << "✗ VERIFICATION FAILED";
        qWarning() << "  - Expected user:" << user.name;
        qWarning() << "  - Match score:" << matchScore << "%";
        qWarning() << "  - Result: NO MATCH ✗";
    }
    
    // Cleanup
    fpManager.closeReader();
    fpManager.cleanup();
    
    qInfo() << "";
    qInfo() << "=== Verification Complete ===";
    
    QCoreApplication::exit(matched ? 0 : 1);
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    
    // Get user ID from command line
    int userId = 1;
    if (argc > 1) {
        bool ok;
        userId = QString(argv[1]).toInt(&ok);
        if (!ok) {
            qCritical() << "Invalid user ID. Usage:" << argv[0] << "[user_id]";
            return 1;
        }
    }
    
    // Start verification after event loop starts
    QTimer::singleShot(0, [userId]() { verifyUser(userId); });
    
    return app.exec();
}

