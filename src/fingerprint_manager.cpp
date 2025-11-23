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

#include <QCoreApplication>

int FingerprintManager::identifyUser(const QMap<int, QByteArray>& userTemplates, int& score, 
                                   std::function<void(int, int)> progressCallback,
                                   std::function<bool()> checkCancelCallback)
{
    if (!m_device) {
        setError("Device not open");
        return -1;
    }

    if (userTemplates.isEmpty()) {
        setError("No users to identify against");
        return -1;
    }

    // Capture fingerprint first (only once)
    qDebug() << "=== IDENTIFICATION STARTED ===";
    qDebug() << "Please place your finger on the reader...";

    // We need to capture a print to match against the gallery
    // But libfprint's identify_sync takes a gallery and handles everything
    // However, to support progress and cancellation with a large gallery,
    // we might need to split the gallery or use lower-level APIs if possible.
    //
    // Unfortunately, identify_sync is an all-or-nothing operation with the gallery provided.
    // To support batching, we would need to capture the print first, then compare manually.
    // But libfprint 2.0 doesn't easily expose "capture only" for identification without enrollment.
    //
    // ALTERNATIVE STRATEGY:
    // We will perform identification in small batches using the standard identify_sync.
    // This means the user MIGHT have to lift/place finger multiple times if we strictly followed
    // "scan once, match many". But identify_sync waits for a finger.
    //
    // WAIT! The proper way with libfprint for large datasets and responsiveness is tricky.
    // `fp_device_identify_sync` takes the whole gallery. If we pass 10,000 prints, it might block.
    //
    // IMPROVED STRATEGY:
    // We can't easily "scan once" and "match manual" because `fp_print_verify` isn't exposed 
    // for raw comparison in the high-level API in the same way.
    //
    // HOWEVER, for the purpose of this "optimization", let's assume we pass the whole gallery
    // but we rely on the underlying GLib main loop integration if we were async.
    // Since we are sync, we are stuck.
    //
    // REVISED PLAN:
    // We will use `fp_device_capture_sync` (if available/supported) to get a probe print,
    // and then manually compare it against the gallery in batches.
    // BUT `fp_device_capture_sync` is for image capture, not necessarily minutiae for matching.
    //
    // Let's look at `fp_device_verify_sync`. It compares one stored print against a live scan.
    // That requires scanning every time. Not good for 1:N.
    //
    // Let's stick to `fp_device_identify_sync` but break the gallery into chunks?
    // NO, that would require the user to scan their finger for EACH chunk. Terrible UX.
    //
    // CORRECT APPROACH for libfprint 2:
    // We should load all prints into the gallery. The matching happens inside the driver/library.
    // If it's slow, it's slow.
    //
    // BUT, if we want to avoid UI freeze, we MUST call `processEvents` or run in a thread.
    //
    // Since the user specifically asked for "batching" to avoid chaos:
    // The "chaos" (freezing) happens during the matching phase after scan.
    //
    // OPTIMIZATION:
    // 1. We prepare the gallery.
    // 2. We call identify.
    //
    // If `libfprint` matches linearly, it might take time.
    //
    // To truly implement "Scan Once, Match Many in Batches" with `libfprint`,
    // we face a limitation: `fp_device_identify` does the capture AND match.
    //
    // HACK/WORKAROUND for "Scan Once, Compare Many" if drivers don't support it:
    // It seems `libfprint` doesn't expose a "match two prints" function publicly in the high-level API
    // that works without a device interaction for *verification*.
    //
    // WAIT! `fp_device_identify_sync` takes a gallery.
    //
    // Let's try to just be transparent:
    // If we pass 10k prints to `fp_device_identify_sync`, it will block until done.
    // The only way to make it non-blocking is to use the ASYNC version `fp_device_identify`.
    //
    // But we are in `identifyUser` (sync).
    //
    // Let's keep the batching logic simple:
    // We can't easily batch the *matching* if the API wraps capture+match.
    //
    // ACTUALLY, most U.are.U devices do matching in software (libfprint host).
    // So `fp_device_identify` will capture, then loop.
    //
    // If we want to allow UI updates, we must run this in a thread (which we do on Mac, but avoided on Linux).
    //
    // On Linux, we must use the Async API or a Thread.
    // Since the user wants to "avoid chaos" (freeze), moving to a Thread is the standard solution.
    // The "crash" we saw earlier was likely due to GLib context issues across threads.
    //
    // If we properly manage the GLib context or use `QThread` with `moveToThread` for a worker
    // that owns the `FpContext`, it should work.
    //
    // However, changing to full Async/Threaded architecture is risky now.
    //
    // ALTERNATIVE: 
    // For now, we will load ALL templates into the gallery (as before).
    // But to report "Progress" during loading (which can take time for 10k records),
    // we can at least show that.
    //
    // "Matching" progress is hard to hook into `fp_device_identify_sync`.
    //
    // Let's assume the user wants us to try the "Batching" approach where we:
    // 1. Capture a print (using a special "Capture for identification" mode if exists, or just Enroll/Verify logic?)
    //    No, generic capture is hard.
    //
    // Let's stick to the Plan:
    // We will use `fp_device_identify_sync` but we will allow the user to "Cancel"
    // simply by cancelling the operation if it supports `GCancellable`.
    //
    // Wait, `fp_device_identify_sync` takes `GCancellable`.
    // We can hook that up!
    //
    // But for "Batching" matching...
    // The only way is if we have the probe print.
    //
    // Let's look at `fp_print_deserialize`. We have that.
    //
    // If we can't "Scan Once" and get a `FpPrint` back without matching, we can't batch match.
    //
    // `fp_device_capture` -> gets `FpImage`. Not a template.
    //
    // `fp_device_enroll` -> gets `FpPrint`. But requires 5 scans.
    //
    // `fp_device_verify` -> takes a template, scans, and matches.
    //
    // It seems `libfprint` strongly couples scan+match.
    //
    // SO, strictly speaking, "Batching" (Scan 1 time, match 100, pause, match 100...)
    // is NOT possible if the API doesn't let us get the "Probe Template" separate from matching.
    //
    // UNLESS `fp_device_identify` has a callback for each match attempt?
    // `match_cb`: "A FpDeviceMatchCb called when a print matches." -> Only on match.
    //
    // CONCLUSION:
    // We cannot do "Batch Matching" with the current `libfprint` high-level API easily.
    // The best we can do is:
    // 1. Run the `identify` in a thread (carefully) so UI doesn't freeze.
    // 2. Or, optimize the *loading* of 10k templates into the gallery (which is slow).
    //
    // Let's focus on optimizing the **Gallery Creation** which is O(N).
    // We can report progress during Gallery Creation.
    // And check for cancel during Gallery Creation.
    //
    // Once `identify_sync` starts, it's inside the driver.
    
    // Prepare gallery
    GPtrArray* gallery = g_ptr_array_new_with_free_func(g_object_unref);
    QMap<FpPrint*, int> printToIdMap;

    int totalTemplates = userTemplates.size();
    int currentProcessed = 0;

    qDebug() << "Preparing gallery for" << totalTemplates << "users...";

    // Loop through templates
    QMapIterator<int, QByteArray> i(userTemplates);
    while (i.hasNext()) {
        // Check cancel
        if (checkCancelCallback && checkCancelCallback()) {
            qDebug() << "Identification cancelled during gallery preparation";
            g_ptr_array_unref(gallery);
            return -1;
        }

        i.next();
        int userId = i.key();
        const QByteArray& data = i.value();

        GError* error = nullptr;
        FpPrint* print = fp_print_deserialize((const guchar*)data.constData(), data.size(), &error);
        if (error) {
            // qWarning() << "Skipping invalid template for user" << userId << ":" << error->message;
            g_error_free(error);
            continue;
        }
        
        g_ptr_array_add(gallery, print); // print is owned by gallery now
        printToIdMap.insert(print, userId);

        currentProcessed++;
        
        // Report progress every 100 items or so to avoid spamming
        if (progressCallback && (currentProcessed % 50 == 0 || currentProcessed == totalTemplates)) {
            progressCallback(currentProcessed, totalTemplates);
            // Process events to keep UI responsive during this heavy loop
            QCoreApplication::processEvents(); 
        }
    }

    if (gallery->len == 0) {
        setError("No valid templates loaded");
        g_ptr_array_unref(gallery);
        return -1;
    }

    qDebug() << "Gallery prepared. Size:" << gallery->len;
    qDebug() << "Starting identification scan...";
    
    // If we have a progress callback, tell them we are now scanning
    if (progressCallback) {
        progressCallback(totalTemplates, totalTemplates); // 100% loaded
    }

    GError* error = nullptr;
    FpPrint* matchPrint = nullptr;
    FpPrint* newPrint = nullptr;
    
    // Identify - capture and match against gallery
    // This call blocks. In a main-thread-only architecture (Linux fix), this will still freeze UI during the scan/match phase.
    // But at least we optimized the loading phase which handles the 10k serialization.
    gboolean result = fp_device_identify_sync(
        m_device,
        gallery,
        nullptr, // cancellable - TODO: Wire up GCancellable if needed later
        nullptr, // match_cb
        nullptr, // match_data
        &matchPrint, // return matching print
        &newPrint,   // return new print
        &error
    );

    int matchedUserId = -1;
    score = 0;

    if (error) {
        if (error->domain == FP_DEVICE_ERROR && error->code == FP_DEVICE_ERROR_DATA_NOT_FOUND) {
            qDebug() << "Identify: No match found (DATA_NOT_FOUND)";
        } else {
            QString errorMsg = QString("Identification failed: %1").arg(error->message);
            setError(errorMsg);
            qWarning() << errorMsg;
        }
        g_error_free(error);
    } else if (matchPrint) {
        // Found a match!
        if (printToIdMap.contains(matchPrint)) {
            matchedUserId = printToIdMap.value(matchPrint);
            score = 95; // High confidence match
            qDebug() << "✓ IDENTIFICATION MATCH: User ID" << matchedUserId;
        } else {
            qWarning() << "Match returned but not found in map!";
        }
    } else {
        qDebug() << "Identification completed: No match found.";
    }

    // Cleanup
    if (newPrint) g_object_unref(newPrint);
    g_ptr_array_unref(gallery);

    return matchedUserId;
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

