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
    // Force GLib type system initialization
    g_type_init();

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

// Callback data structure
struct EnrollmentCallbackData {
    int* enrollmentCount;
    ProgressCallback* progressCallback;
};

// Callback for enrollment progress
static void enroll_progress_cb(FpDevice* device, gint completed_stages, FpPrint* print, 
                                gpointer user_data, GError* error)
{
    (void)device;
    (void)print;
    (void)error;
    
    EnrollmentCallbackData* data = (EnrollmentCallbackData*)user_data;
    if (data) {
        if (data->enrollmentCount) {
            *data->enrollmentCount = completed_stages;
        }
        
        // User-friendly progress messages
        QString message;
        switch(completed_stages) {
            case 1:
                message = "✓ SCAN 1/5 Complete - Lift finger and place again...";
                break;
            case 2:
                message = "✓ SCAN 2/5 Complete - Lift finger and place again...";
                break;
            case 3:
                message = "✓ SCAN 3/5 Complete - Lift finger and place again...";
                break;
            case 4:
                message = "✓ SCAN 4/5 Complete - Lift finger and place again...";
                break;
            case 5:
                message = "✓ SCAN 5/5 Complete - Processing fingerprint template...";
                break;
            default:
                message = QString("Enrollment progress: %1 stages completed").arg(completed_stages);
        }
        
        qDebug() << message;
        
        // Call progress callback if set
        if (data->progressCallback && *data->progressCallback) {
            (*data->progressCallback)(completed_stages, 5, message);
        }
    }
}

bool FingerprintManager::startEnrollment()
{
    cancelEnrollment();
    
    if (!m_device) {
        setError("Device not open");
        return false;
    }
    
    m_enrollmentCount = 0;
    m_enrollmentInProgress = true;
    
    qDebug() << "Enrollment started - device ready for capture";
    qDebug() << "Please scan your finger 5 times when prompted";
    
    return true;
}

int FingerprintManager::addEnrollmentSample(QString& message, int& quality, QImage* image)
{
    if (!m_device || !m_enrollmentInProgress) {
        setError("Enrollment not started");
        return -1;
    }
    
    GError* error = nullptr;
    
    // First time - create template
    if (m_enrollmentCount == 0) {
        qDebug() << "=== ENROLLMENT STARTED ===";
        qDebug() << "You will need to scan your finger 5 times";
        qDebug() << "Please place your finger on the reader now...";
        message = "SCAN 1/5: Place your finger on the reader and hold...";
    }
    
    quality = 50;
    
    // Create template print for first scan with metadata
    FpPrint* template_print = fp_print_new(m_device);
    
    // Set required metadata to avoid NULL assertions during serialization
    fp_print_set_username(template_print, "user");
    fp_print_set_finger(template_print, FP_FINGER_UNKNOWN);
    fp_print_set_description(template_print, "enrolled");
    
    qDebug() << "Capturing enrollment samples...";
    qDebug() << "Keep finger steady on the reader...";
    
    // Setup callback data
    EnrollmentCallbackData callbackData;
    callbackData.enrollmentCount = &m_enrollmentCount;
    callbackData.progressCallback = &m_progressCallback;
    
    // Enroll - this will capture all required samples
    FpPrint* enrolled_print = fp_device_enroll_sync(
        m_device, 
        template_print, 
        nullptr, 
        enroll_progress_cb,
        &callbackData,
        &error
    );
    
    // Don't unref template_print yet - check if enrolled_print is the same object
    
    if (error) {
        QString errorMsg = QString("Enrollment failed: %1").arg(error->message);
        setError(errorMsg);
        qWarning() << errorMsg;
        g_error_free(error);
        g_object_unref(template_print);
        m_enrollmentInProgress = false;
        return -1;
    }
    
    if (!enrolled_print) {
        setError("Enrollment failed - no print returned");
        qWarning() << "Enrollment failed - no print returned";
        g_object_unref(template_print);
        m_enrollmentInProgress = false;
        return -1;
    }
    
    qDebug() << "Enrolled print received, checking validity...";
    qDebug() << "template_print ptr:" << (void*)template_print;
    qDebug() << "enrolled_print ptr:" << (void*)enrolled_print;
    
    // Note: libfprint doesn't expose raw images from enrollment for most devices
    // The image preview will be generated by the UI callback instead
    (void)image; // Suppress unused parameter warning
    
    // Clean up old enrollment if any
    if (m_enrollPrint) {
        g_object_unref(m_enrollPrint);
        m_enrollPrint = nullptr;
    }
    
    // Store the enrolled print (take ownership)
    m_enrollPrint = enrolled_print;
    
    // Unref template only if it's different from enrolled_print
    if (template_print != enrolled_print) {
        qDebug() << "Template and enrolled prints are different, unreffing template";
        g_object_unref(template_print);
    } else {
        qDebug() << "Template and enrolled prints are same object";
    }
    
    // Enrollment complete
    message = QString("✓ ENROLLMENT COMPLETE! Successfully captured %1 scans.").arg(m_enrollmentCount);
    quality = 100;
    m_enrollmentInProgress = false;
    
    qDebug() << "=== ENROLLMENT COMPLETED SUCCESSFULLY ===";
    qDebug() << "Total scans completed:" << m_enrollmentCount;
    
    return 1;
}

bool FingerprintManager::createEnrollmentTemplate(QByteArray& templateData)
{
    if (!m_enrollPrint) {
        setError("No enrollment data");
        qWarning() << "No enrollment print available";
        return false;
    }
    
    qDebug() << "Creating enrollment template...";
    qDebug() << "m_enrollPrint ptr:" << (void*)m_enrollPrint;
    
    // Check if print object is valid
    if (!FP_IS_PRINT(m_enrollPrint)) {
        setError("Invalid print object - corrupted or already freed");
        qWarning() << "ERROR: m_enrollPrint is not a valid FpPrint object!";
        return false;
    }
    
    qDebug() << "Print object is valid, checking metadata...";
    
    // Ensure metadata is set before serialization
    const gchar* existing_username = fp_print_get_username(m_enrollPrint);
    qDebug() << "Current username:" << (existing_username ? existing_username : "(null)");
    
    if (!existing_username || strlen(existing_username) == 0) {
        qDebug() << "Setting default metadata for serialization";
        fp_print_set_username(m_enrollPrint, "enrolled_user");
    }
    
    const gchar* existing_desc = fp_print_get_description(m_enrollPrint);
    qDebug() << "Current description:" << (existing_desc ? existing_desc : "(null)");
    
    if (!existing_desc || strlen(existing_desc) == 0) {
        fp_print_set_description(m_enrollPrint, "fingerprint");
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
    
    // Verify - capture and match in one step
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
    qDebug() << "Result: matched=" << (matched ? "YES" : "NO") << ", score=" << score << "%";
    
    return true;
}

void FingerprintManager::setError(const QString& error)
{
    m_lastError = error;
    qWarning() << "FingerprintManager Error:" << error;
}

QImage FingerprintManager::convertFpImageToQImage(FpImage* fpImage)
{
    if (!fpImage) {
        return QImage();
    }
    
    gint width = fp_image_get_width(fpImage);
    gint height = fp_image_get_height(fpImage);
    gsize data_len = 0;
    const guchar* data = fp_image_get_data(fpImage, &data_len);
    
    if (!data || width <= 0 || height <= 0) {
        qWarning() << "Invalid FpImage data";
        return QImage();
    }
    
    // FpImage is grayscale, convert to QImage
    QImage image(width, height, QImage::Format_Grayscale8);
    
    for (int y = 0; y < height; ++y) {
        memcpy(image.scanLine(y), data + (y * width), width);
    }
    
    return image;
}

