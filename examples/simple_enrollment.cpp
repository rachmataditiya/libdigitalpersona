/**
 * @file simple_enrollment.cpp
 * @brief Simple example demonstrating fingerprint enrollment
 * 
 * This example shows how to:
 * 1. Initialize the fingerprint manager
 * 2. Open the fingerprint device
 * 3. Enroll a user's fingerprint
 * 4. Save the fingerprint to database
 * 
 * Compile:
 *   qmake6 && make
 *   
 * Run:
 *   ./simple_enrollment
 */

#include <digitalpersona.h>
#include <QCoreApplication>
#include <QDebug>
#include <QTimer>

void enrollUser() {
    qInfo() << "=== DigitalPersona Fingerprint Enrollment Example ===";
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
    qInfo() << "";
    
    // Start enrollment
    if (!fpManager.startEnrollment()) {
        qCritical() << "❌ Failed to start enrollment:" << fpManager.getLastError();
        fpManager.closeReader();
        fpManager.cleanup();
        QCoreApplication::exit(1);
        return;
    }
    
    qInfo() << "📌 ENROLLMENT STARTED";
    qInfo() << "Please scan your finger 5 times when prompted...";
    qInfo() << "";
    
    // Capture enrollment samples
    QString message;
    int quality;
    int result = fpManager.addEnrollmentSample(message, quality);
    
    if (result < 0) {
        qCritical() << "❌ Enrollment failed:" << fpManager.getLastError();
        fpManager.closeReader();
        fpManager.cleanup();
        QCoreApplication::exit(1);
        return;
    }
    
    if (result == 1) {
        qInfo() << "";
        qInfo() << "✓ Enrollment completed successfully!";
        
        // Create template
        QByteArray templateData;
        if (!fpManager.createEnrollmentTemplate(templateData)) {
            qCritical() << "❌ Failed to create template:" << fpManager.getLastError();
            fpManager.cancelEnrollment();
            fpManager.closeReader();
            fpManager.cleanup();
            QCoreApplication::exit(1);
            return;
        }
        
        qInfo() << "✓ Template created, size:" << templateData.size() << "bytes";
        
        // Save to database
        int userId;
        QString userName = "Test User";
        QString userEmail = "test@example.com";
        
        if (dbManager.addUser(userName, userEmail, templateData, userId)) {
            qInfo() << "✓ User saved to database!";
            qInfo() << "  - User ID:" << userId;
            qInfo() << "  - Name:" << userName;
            qInfo() << "  - Email:" << userEmail;
            qInfo() << "  - Template size:" << templateData.size() << "bytes";
        } else {
            qCritical() << "❌ Failed to save user:" << dbManager.getLastError();
        }
        
        fpManager.cancelEnrollment();
    }
    
    // Cleanup
    fpManager.closeReader();
    fpManager.cleanup();
    
    qInfo() << "";
    qInfo() << "=== Enrollment Complete ===";
    
    QCoreApplication::exit(0);
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    
    // Start enrollment after event loop starts
    QTimer::singleShot(0, enrollUser);
    
    return app.exec();
}

