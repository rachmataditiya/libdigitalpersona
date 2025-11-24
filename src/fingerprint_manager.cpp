#include "fingerprint_manager.h"
#include <QDebug>
#include <QDateTime>
#include <cstring>

// Undefine GLib macros that conflict with Qt
#ifdef signals
#undef signals
#endif
#ifdef slots  
#undef slots
#endif
#ifdef emit
#undef emit
#endif

// Include GLib/libfprint after Qt
extern "C" {
#include <glib.h>
#include <glib-object.h>
#ifdef Q_OS_MACOS
#include <fprint.h>
#else
#include <libfprint-2/fprint.h>
#endif
}

FingerprintManager::FingerprintManager()
    : m_context(nullptr)
    , m_device(nullptr)
    , m_enrollPrint(nullptr)
    , m_lastImage(nullptr)
    , m_enrollmentCount(0)
    , m_enrollmentInProgress(false)
{
}

FingerprintManager::~FingerprintManager()
{
    cleanup();
}

bool FingerprintManager::initialize()
{
    // g_type_init() deprecated in GLib 2.36+, not needed in modern GLib
    // GLib type system initializes automatically

    m_context = fp_context_new();
    if (!m_context) {
        setError("Failed to create libfprint context");
        return false;
    }
    
    return true;
}

void FingerprintManager::cleanup()
{
    closeReader();
    
    if (m_lastImage) {
        g_object_unref(m_lastImage);
        m_lastImage = nullptr;
    }
    
    if (m_enrollPrint) {
        g_object_unref(m_enrollPrint);
        m_enrollPrint = nullptr;
    }
    
    // On macOS, destroying the context at exit causes a crash in libgusb/libplatform
    // due to mutex corruption (os_unfair_lock is corrupt). 
    // Leaking the context at process exit is a safer workaround.
#ifndef Q_OS_MACOS
    if (m_context) {
        g_object_unref(m_context);
        m_context = nullptr;
    }
#endif
}

int FingerprintManager::getDeviceCount()
{
    if (!m_context) {
        return 0;
    }
    
    GPtrArray* devices = fp_context_get_devices(m_context);
    int count = devices->len;
    g_ptr_array_unref(devices);
    
    return count;
}

QVector<DeviceInfo> FingerprintManager::getAvailableDevices()
{
    QVector<DeviceInfo> deviceList;
    
    if (!m_context) {
        return deviceList;
    }
    
    GPtrArray* devices = fp_context_get_devices(m_context);
    
    for (guint i = 0; i < devices->len; ++i) {
        FpDevice* device = FP_DEVICE(g_ptr_array_index(devices, i));
        
        DeviceInfo info;
        info.name = QString(fp_device_get_name(device));
        info.driver = QString(fp_device_get_driver(device));
        info.deviceId = QString(fp_device_get_device_id(device));
        info.isOpen = fp_device_is_open(device);
        info.supportsCapture = fp_device_has_feature(device, FP_DEVICE_FEATURE_CAPTURE);
        info.supportsIdentify = fp_device_has_feature(device, FP_DEVICE_FEATURE_IDENTIFY);
        
        deviceList.append(info);
    }
    
    g_ptr_array_unref(devices);
    
    return deviceList;
}

QString FingerprintManager::getDeviceName() const
{
    if (!m_device) {
        return QString();
    }
    
    return QString(fp_device_get_name(m_device));
}

DeviceInfo FingerprintManager::getCurrentDeviceInfo() const
{
    DeviceInfo info;
    
    if (!m_device) {
        return info;
    }
    
    info.name = QString(fp_device_get_name(m_device));
    info.driver = QString(fp_device_get_driver(m_device));
    info.deviceId = QString(fp_device_get_device_id(m_device));
    info.isOpen = fp_device_is_open(m_device);
    info.supportsCapture = fp_device_has_feature(m_device, FP_DEVICE_FEATURE_CAPTURE);
    info.supportsIdentify = fp_device_has_feature(m_device, FP_DEVICE_FEATURE_IDENTIFY);
    
    return info;
}

bool FingerprintManager::openReader()
{
    return openReader(0); // Open first device
}

bool FingerprintManager::openReader(int deviceIndex)
{
    if (m_device) {
        qDebug() << "Device already open, closing first...";
        closeReader();
    }
    
    if (!m_context) {
        setError("Context not initialized. Call initialize() first.");
        return false;
    }
    
    GPtrArray* devices = fp_context_get_devices(m_context);
    if (devices->len == 0) {
        g_ptr_array_unref(devices);
        setError("No fingerprint readers found");
        return false;
    }
    
    if (deviceIndex < 0 || deviceIndex >= (int)devices->len) {
        g_ptr_array_unref(devices);
        setError(QString("Invalid device index: %1 (available: 0-%2)")
                 .arg(deviceIndex).arg(devices->len - 1));
        return false;
    }
    
    // Get the specified device
    m_device = FP_DEVICE(g_object_ref(g_ptr_array_index(devices, deviceIndex)));
    g_ptr_array_unref(devices);
    
    // Open device
    GError* error = nullptr;
    if (!fp_device_open_sync(m_device, nullptr, &error)) {
        setError(QString("Failed to open device: %1").arg(error->message));
        g_error_free(error);
        g_object_unref(m_device);
        m_device = nullptr;
        return false;
    }
    
    qDebug() << "Device opened:" << fp_device_get_name(m_device);
    return true;
}

void FingerprintManager::closeReader()
{
    if (m_device) {
        // Cancel any ongoing enrollment
        if (m_enrollmentInProgress) {
            m_enrollmentInProgress = false;
        }
        
        GError* error = nullptr;
        if (!fp_device_close_sync(m_device, nullptr, &error)) {
            qWarning() << "Failed to close device:" << error->message;
            g_error_free(error);
        }
        
        g_object_unref(m_device);
        m_device = nullptr;
        qDebug() << "Device closed";
    }
}

// Enrollment callback signature: FpEnrollProgress
// typedef void (*FpEnrollProgress) (FpDevice *device, int completed_stages, FpPrint *print, gpointer user_data, GError *error);
static void
enrollment_progress_cb(FpDevice* device, int completed_stages, FpPrint* print, gpointer user_data, GError* error)
{
    Q_UNUSED(device);
    Q_UNUSED(print);
    
    if (error) {
        qWarning() << "Enrollment progress error:" << error->message;
        return;
    }
    
    // Progress callback can be called here if needed
    // For now, we rely on fp_device_enroll_sync completing all stages in one call
    qDebug() << "Enrollment progress: stage" << completed_stages;
}

bool FingerprintManager::startEnrollment()
{
    if (!m_device) {
        setError("Device not open");
        return false;
    }
    
    if (m_enrollmentInProgress) {
        setError("Enrollment already in progress");
        return false;
    }
    
    // Create new print for enrollment
    m_enrollPrint = fp_print_new(m_device);
    if (!m_enrollPrint) {
        setError("Failed to create print for enrollment");
        return false;
    }
    
    // Set metadata (required for serialization)
    fp_print_set_username(m_enrollPrint, "user");
    fp_print_set_finger(m_enrollPrint, FP_FINGER_UNKNOWN);
    fp_print_set_description(m_enrollPrint, "enrolled");
    
    m_enrollmentCount = 0;
    m_enrollmentInProgress = true;
    
    qDebug() << "Enrollment started";
    return true;
}

int FingerprintManager::addEnrollmentSample(QString& message, int& quality, QImage* image)
{
    if (!m_enrollmentInProgress) {
        setError("Enrollment not started. Call startEnrollment() first.");
        return -1;
    }
    
    if (!m_device) {
        setError("Device not open");
        m_enrollmentInProgress = false;
        return -1;
    }
    
    // Use fp_device_enroll_sync to perform full enrollment (all required scans internally)
    // This matches Android implementation - enrollment completes in one call
    GError* error = nullptr;
    FpPrint* enrolledPrint = fp_device_enroll_sync(
        m_device,
        m_enrollPrint,  // existing print to add to
        nullptr, // cancellable
        enrollment_progress_cb, // progress callback
        nullptr, // user_data
        &error
    );
    
    if (error) {
        QString errorMsg = QString("Enrollment failed: %1").arg(error->message);
        setError(errorMsg);
        qWarning() << errorMsg;
        g_error_free(error);
        m_enrollmentInProgress = false;
        return -1;
    }
    
    if (!enrolledPrint) {
        setError("Enrollment failed - no print returned");
        m_enrollmentInProgress = false;
        return -1;
    }
    
    // Enrollment is complete - fp_device_enroll_sync does all scans in one call
    m_enrollmentCount = 5; // Mark as complete
    message = QString("Enrollment complete!");
    quality = 95;
    
    // Update m_enrollPrint if needed
    if (enrolledPrint != m_enrollPrint) {
        if (m_enrollPrint) {
            g_object_unref(m_enrollPrint);
        }
        m_enrollPrint = enrolledPrint;
    }
    
    return 1; // Success, enrollment complete
}

bool FingerprintManager::createEnrollmentTemplate(QByteArray& templateData)
{
    templateData.clear();
    
    if (!m_enrollPrint) {
        setError("No enrollment print available");
        return false;
    }
    
    // Serialize print
    guchar* data = nullptr;
    gsize size = 0;
    GError* error = nullptr;
    
    qDebug() << "Serializing fingerprint data...";
    
    gboolean result = fp_print_serialize(m_enrollPrint, &data, &size, &error);
    if (!result || error) {
        QString errorMsg = QString("Failed to serialize print: %1").arg(error ? error->message : "Unknown error");
        setError(errorMsg);
        qWarning() << errorMsg;
        if (error) g_error_free(error);
        return false;
    }
    
    if (!data || size == 0) {
        setError("Serialization returned empty data");
        qWarning() << "Serialization returned empty data";
        return false;
    }
    
    templateData = QByteArray((const char*)data, size);
    g_free(data);
    
    qDebug() << "Template created successfully, size:" << templateData.size() << "bytes";
    
    return true;
}

void FingerprintManager::cancelEnrollment()
{
    m_enrollmentCount = 0;
    m_enrollmentInProgress = false;
    
    if (m_enrollPrint) {
        qDebug() << "Cleaning up enrollment print";
        g_object_unref(m_enrollPrint);
        m_enrollPrint = nullptr;
    }
}

#include <QCoreApplication>

bool FingerprintManager::identifyUser(const QVector<QPair<int, QByteArray>>& templates, int& matchedIndex, int& score,
                                   std::function<void(int, int)> progressCallback,
                                   std::function<bool()> checkCancelCallback)
{
    matchedIndex = -1;
    score = 0;
    
    if (!m_device) {
        setError("Device not open");
        return false;
    }

    if (templates.isEmpty()) {
        setError("No templates provided");
        return false;
    }

    qDebug() << "=== IDENTIFICATION STARTED ===";
    qDebug() << "Preparing gallery for" << templates.size() << "templates (all fingers)...";
    
    // Create gallery (like Android implementation)
    GPtrArray* gallery = g_ptr_array_new_with_free_func(g_object_unref);
    QMap<FpPrint*, int> printToIndexMap; // Map FpPrint* to template index

    int validTemplates = 0;
    for (int i = 0; i < templates.size(); ++i) {
        // Check cancel
        if (checkCancelCallback && checkCancelCallback()) {
            qDebug() << "Identification cancelled during gallery preparation";
            g_ptr_array_unref(gallery);
            return false;
        }
        
        const QPair<int, QByteArray>& pair = templates[i];
        int userId = pair.first;
        const QByteArray& templateData = pair.second;
        
        if (templateData.isEmpty()) {
            qWarning() << "Skipping template" << i << "(User" << userId << "): empty template";
            continue;
        }
        
        GError* error = nullptr;
        FpPrint* print = fp_print_deserialize((const guchar*)templateData.constData(), templateData.size(), &error);
        if (error) {
            qWarning() << "Skipping invalid template" << i << "(User" << userId << "):" << error->message;
            g_error_free(error);
            continue;
        }
        
        validTemplates++;
        g_ptr_array_add(gallery, print); // print is owned by gallery now
        printToIndexMap.insert(print, i); // Map to template index (supports multiple fingers per user)
        
        // Report progress
        if (progressCallback && (validTemplates % 50 == 0 || validTemplates == templates.size())) {
            progressCallback(validTemplates, templates.size());
            QCoreApplication::processEvents();
        }
    }

    if (gallery->len == 0) {
        setError("No valid templates loaded");
        g_ptr_array_unref(gallery);
        return false;
    }

    qDebug() << "Gallery prepared. Valid templates:" << validTemplates << "/" << templates.size() << "(gallery size:" << gallery->len << ")";
    qDebug() << "Starting identification scan...";
    
    // Report 100% loaded
    if (progressCallback) {
        progressCallback(templates.size(), templates.size());
    }

    GError* error = nullptr;
    FpPrint* matchPrint = nullptr;
    FpPrint* newPrint = nullptr;
    
    // Identify - capture and match against gallery (like Android)
    gboolean result = fp_device_identify_sync(
        m_device,
        gallery,
        nullptr, // cancellable
        nullptr, // match_cb
        nullptr, // match_data
        &matchPrint, // return matching print
        &newPrint,   // return new print
        &error
    );

    if (error) {
        if (error->domain == FP_DEVICE_ERROR && error->code == FP_DEVICE_ERROR_DATA_NOT_FOUND) {
            // No match - this is normal for failed identification
            qDebug() << "Identify: No match found (fingerprint scanned but doesn't match any user)";
            matchedIndex = -1;
            score = 0;
            g_error_free(error);
            g_ptr_array_unref(gallery);
            if (newPrint) {
                g_object_unref(newPrint);
            }
            return true; // Successfully completed, just no match
        } else if (error->code == FP_DEVICE_ERROR_NOT_OPEN) {
            setError("Device not open");
            g_error_free(error);
            g_ptr_array_unref(gallery);
            if (newPrint) {
                g_object_unref(newPrint);
            }
            return false;
        } else if (error->code == FP_DEVICE_ERROR_BUSY) {
            setError("Device is busy");
            g_error_free(error);
            g_ptr_array_unref(gallery);
            if (newPrint) {
                g_object_unref(newPrint);
            }
            return false;
        }
        
        QString errorMsg = QString("Identification failed: %1").arg(error->message);
        setError(errorMsg);
        qWarning() << errorMsg;
        g_error_free(error);
        g_ptr_array_unref(gallery);
        if (newPrint) {
            g_object_unref(newPrint);
        }
        return false;
    } else if (matchPrint) {
        // Found a match!
        if (printToIndexMap.contains(matchPrint)) {
            matchedIndex = printToIndexMap.value(matchPrint);
            score = 95; // High confidence match
            int matchedUserId = templates[matchedIndex].first;
            qDebug() << "✓ IDENTIFICATION MATCH: Template index" << matchedIndex << "(User ID" << matchedUserId << ")";
        } else {
            qWarning() << "Match returned but not found in map!";
            matchedIndex = -1;
            score = 0;
        }
    } else {
        // No match found (but operation completed successfully)
        qDebug() << "Identification completed: No match found.";
        matchedIndex = -1;
        score = 0;
    }

    // Cleanup
    g_ptr_array_unref(gallery);
    if (newPrint) {
        g_object_unref(newPrint);
    }

    return true; // Always return true if we got here (error cases already returned false above)
}

bool FingerprintManager::verifyFingerprint(const QByteArray& templateData, int& score)
{
    if (!m_device) {
        setError("Device not open");
        return false;
    }
    
    // Deserialize stored print
    GError* error = nullptr;
    FpPrint* storedPrint = fp_print_deserialize((const guchar*)templateData.constData(), templateData.size(), &error);
    if (error) {
        setError(QString("Failed to deserialize print: %1").arg(error->message));
        g_error_free(error);
        return false;
    }
    
    // Capture and match
    bool matched = false;
    bool result = captureAndMatch(storedPrint, matched, score);
    
    g_object_unref(storedPrint);
    
    return result && matched;
}

bool FingerprintManager::verifyFingerprint(const FingerprintTemplate& fpTemplate, int& score)
{
    return verifyFingerprint(fpTemplate.data, score);
}

bool FingerprintManager::captureAndMatch(FpPrint* storedPrint, bool& matched, int& score)
{
    GError* error = nullptr;
    FpPrint* newPrint = nullptr;
    gboolean match = FALSE;
    
    qDebug() << "=== VERIFICATION STARTED ===";
    qDebug() << "Please place your finger on the reader...";
    qDebug() << "Waiting for finger scan...";
    
    // Verify - capture and match in one step (like Android)
    gboolean result = fp_device_verify_sync(
        m_device,
        storedPrint,
        nullptr,
        nullptr,
        nullptr,
        &match,
        &newPrint,
        &error
    );
    
    if (error) {
        if (error->domain == FP_DEVICE_ERROR && error->code == FP_DEVICE_ERROR_DATA_NOT_FOUND) {
            // No match - this is normal for failed verification
            qDebug() << "Fingerprint scanned but NO MATCH";
            matched = false;
            score = 0;
            g_error_free(error);
            qDebug() << "=== VERIFICATION COMPLETED: NO MATCH ===";
            return true;
        }
        
        QString errorMsg = QString("Verification failed: %1").arg(error->message);
        setError(errorMsg);
        qWarning() << errorMsg;
        g_error_free(error);
        if (newPrint) {
            g_object_unref(newPrint);
        }
        return false;
    }
    
    if (!result) {
        setError("Verification failed - no result returned");
        qWarning() << "Verification failed - no result";
        if (newPrint) {
            g_object_unref(newPrint);
        }
        return false;
    }
    
    matched = (match == TRUE);
    
    // Calculate score based on match
    if (matched) {
        score = 95; // libfprint doesn't provide detailed score, just match/no-match
        qDebug() << "✓ FINGERPRINT MATCHED!";
    } else {
        score = 30; // Low score for no match
        qDebug() << "✗ Fingerprint does not match";
    }
    
    if (newPrint) {
        g_object_unref(newPrint);
    }
    
    qDebug() << "=== VERIFICATION COMPLETED ===";
    return true;
}

// setProgressCallback() is defined inline in header

void FingerprintManager::setError(const QString& error)
{
    m_lastError = error;
    qWarning() << "FingerprintManager error:" << error;
}

// getLastError() is defined inline in header

bool FingerprintManager::captureRawImage(QByteArray& imageData)
{
    // First, try to use image from enrolled print if available
    if (m_lastImage) {
        guint width = fp_image_get_width(m_lastImage);
        guint height = fp_image_get_height(m_lastImage);
        gsize dataLen = 0;
        const guchar* data = fp_image_get_data(m_lastImage, &dataLen);
        
        if (data && dataLen > 0) {
            qDebug() << "Using image from enrolled print: " << width << "x" << height << ", " << dataLen << " bytes";
            imageData = QByteArray((const char*)data, dataLen);
            return true;
        }
    }
    
    // Also try to get image from enrolled print if available
    if (m_enrollPrint) {
        FpImage* printImage = fp_print_get_image(m_enrollPrint);
        if (printImage) {
            guint width = fp_image_get_width(printImage);
            guint height = fp_image_get_height(printImage);
            gsize dataLen = 0;
            const guchar* data = fp_image_get_data(printImage, &dataLen);
            
            if (data && dataLen > 0) {
                qDebug() << "Using image from enrolled print object: " << width << "x" << height << ", " << dataLen << " bytes";
                imageData = QByteArray((const char*)data, dataLen);
                return true;
            }
        }
    }
    
    // Fallback: Capture new image
    if (!m_device) {
        setError("Device not open");
        return false;
    }
    
    qDebug() << "Capturing new raw fingerprint image for storage...";
    
    GError* error = nullptr;
    
    // Capture image synchronously (wait for finger)
    // API: FpImage * fp_device_capture_sync (FpDevice *device, gboolean wait_for_finger, GCancellable *cancellable, GError **error)
    FpImage* fpImage = fp_device_capture_sync(
        m_device,
        true, // wait_for_finger
        nullptr, // cancellable
        &error
    );
    
    if (error) {
        QString errorMsg = QString("Failed to capture image: %1").arg(error->message);
        setError(errorMsg);
        qWarning() << errorMsg;
        g_error_free(error);
        return false;
    }
    
    if (!fpImage) {
        setError("Failed to capture image - no image returned");
        qWarning() << "Failed to capture image - no image returned";
        return false;
    }
    
    // Get image dimensions and data
    guint width = fp_image_get_width(fpImage);
    guint height = fp_image_get_height(fpImage);
    gsize dataLen = 0;
    const guchar* data = fp_image_get_data(fpImage, &dataLen);
    
    if (!data || dataLen == 0) {
        setError("Image data is empty");
        qWarning() << "Image data is empty";
        g_object_unref(fpImage);
        return false;
    }
    
    qDebug() << "Raw image captured: " << width << "x" << height << ", " << dataLen << " bytes";
    
    // Copy image data to QByteArray
    imageData = QByteArray((const char*)data, dataLen);
    
    // Store for future use
    if (m_lastImage) {
        g_object_unref(m_lastImage);
    }
    m_lastImage = (FpImage*)g_object_ref(fpImage);
    
    // Cleanup
    g_object_unref(fpImage);
    
    qDebug() << "Raw image captured successfully, size:" << imageData.size() << "bytes";
    
    return true;
}

bool FingerprintManager::createEnrollmentTemplate(FingerprintTemplate& fpTemplate)
{
    QByteArray templateData;
    if (!createEnrollmentTemplate(templateData)) {
        return false;
    }
    
    fpTemplate.data = templateData;
    fpTemplate.qualityScore = 95; // High quality since enrollment completed successfully
    fpTemplate.timestamp = QDateTime::currentDateTime();
    fpTemplate.scanCount = m_enrollmentCount;
    
    return true;
}
