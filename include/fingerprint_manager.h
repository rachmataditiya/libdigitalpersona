#ifndef FINGERPRINT_MANAGER_H
#define FINGERPRINT_MANAGER_H

#include "digitalpersona_global.h"
#include <QString>
#include <QByteArray>
#include <QVector>
#include <QImage>
#include <QDateTime>
#include <functional>

// Forward declarations to avoid including GLib in header
typedef struct _FpContext FpContext;
typedef struct _FpDevice FpDevice;
typedef struct _FpPrint FpPrint;
typedef struct _FpImage FpImage;

// Progress callback signature: (currentStage, totalStages, message)
typedef std::function<void(int, int, QString)> ProgressCallback;

/**
 * @brief Device information structure
 */
struct DIGITALPERSONA_EXPORT DeviceInfo {
    QString name;           ///< Device name
    QString driver;         ///< Driver name
    QString deviceId;       ///< Device ID
    bool isOpen;            ///< Is device currently open
    bool supportsCapture;   ///< Supports image capture
    bool supportsIdentify;  ///< Supports identification
};

/**
 * @brief Fingerprint template with metadata
 */
struct DIGITALPERSONA_EXPORT FingerprintTemplate {
    QByteArray data;        ///< Raw template data (serialized FpPrint)
    int qualityScore;       ///< Quality score (0-100)
    QDateTime timestamp;    ///< When template was created
    int scanCount;          ///< Number of scans used in enrollment
};

/**
 * @brief Main fingerprint manager class
 * 
 * This class provides a high-level API for fingerprint operations.
 * It is completely independent of any database implementation.
 * 
 * Typical usage:
 * 1. Create FingerprintManager instance
 * 2. Call initialize()
 * 3. Call openReader() or openReader(deviceIndex)
 * 4. Perform enrollment or verification
 * 5. Call closeReader() and cleanup() when done
 */
class DIGITALPERSONA_EXPORT FingerprintManager {
public:
    FingerprintManager();
    ~FingerprintManager();

    // ========== Initialization & Cleanup ==========
    
    /**
     * @brief Initialize the fingerprint library context
     * @return true on success, false on failure
     */
    bool initialize();
    
    /**
     * @brief Cleanup all resources
     */
    void cleanup();
    
    // ========== Device Management ==========
    
    /**
     * @brief Get number of available fingerprint readers
     * @return Number of devices, or 0 if none found
     */
    int getDeviceCount();
    
    /**
     * @brief Get list of available devices
     * @return Vector of DeviceInfo structures
     */
    QVector<DeviceInfo> getAvailableDevices();
    
    /**
     * @brief Open the first available fingerprint reader
     * @return true on success, false on failure
     */
    bool openReader();
    
    /**
     * @brief Open a specific fingerprint reader by index
     * @param deviceIndex Index of the device (0-based)
     * @return true on success, false on failure
     */
    bool openReader(int deviceIndex);
    
    /**
     * @brief Close the currently open reader
     */
    void closeReader();
    
    /**
     * @brief Check if a reader is currently open
     * @return true if open, false otherwise
     */
    bool isReaderOpen() const { return m_device != nullptr; }
    
    /**
     * @brief Get information about currently open device
     * @return DeviceInfo structure, or empty if no device is open
     */
    DeviceInfo getCurrentDeviceInfo() const;
    
    /**
     * @brief Get name of currently open device
     * @return Device name, or empty string if no device is open
     */
    QString getDeviceName() const;
    
    // ========== Enrollment Operations ==========
    
    /**
     * @brief Start a new enrollment session
     * @return true on success, false on failure
     */
    bool startEnrollment();
    
    /**
     * @brief Add enrollment samples (captures all required scans)
     * 
     * This method will block until all scans are complete or an error occurs.
     * Use setProgressCallback() to receive progress updates.
     * 
     * @param[out] message Status message describing the result
     * @param[out] quality Quality score of the enrollment (0-100)
     * @param[out] image Optional pointer to receive fingerprint image (if supported)
     * @return 1 if enrollment complete, 0 if more samples needed, -1 on error
     */
    int addEnrollmentSample(QString& message, int& quality, QImage* image = nullptr);
    
    /**
     * @brief Create fingerprint template from completed enrollment
     * @param[out] templateData Output buffer for template data
     * @return true on success, false on failure
     */
    bool createEnrollmentTemplate(QByteArray& templateData);
    
    /**
     * @brief Create fingerprint template with metadata
     * @param[out] fpTemplate Output FingerprintTemplate structure
     * @return true on success, false on failure
     */
    bool createEnrollmentTemplate(FingerprintTemplate& fpTemplate);
    
    /**
     * @brief Cancel current enrollment session
     */
    void cancelEnrollment();
    
    /**
     * @brief Check if enrollment is in progress
     * @return true if in progress, false otherwise
     */
    bool isEnrollmentInProgress() const { return m_enrollmentInProgress; }
    
    // ========== Progress Callback ==========
    
    /**
     * @brief Set callback for enrollment progress updates
     * @param callback Function to call with (currentStage, totalStages, message)
     */
    void setProgressCallback(ProgressCallback callback) { m_progressCallback = callback; }
    
    /**
     * @brief Get current enrollment stage
     * @return Current stage number (0-based)
     */
    int getCurrentEnrollmentStage() const { return m_enrollmentCount; }
    
    /**
     * @brief Get total number of enrollment stages required
     * @return Total stages (typically 5 for U.are.U devices)
     */
    int getTotalEnrollmentStages() const { return 5; }
    
    // ========== Verification Operations ==========
    
    /**
     * @brief Verify a fingerprint against a stored template
     * 
     * This method will block until verification is complete or an error occurs.
     * User should place finger on reader before calling this method.
     * 
     * @param templateData Template data to verify against
     * @param[out] score Match score (0-100), higher is better match
     * @return true if fingerprint matches, false otherwise
     */
    bool verifyFingerprint(const QByteArray& templateData, int& score);
    
    /**
     * @brief Verify a fingerprint against a template with metadata
     * @param fpTemplate FingerprintTemplate structure with template data
     * @param[out] score Match score (0-100)
     * @return true if fingerprint matches, false otherwise
     */
    bool verifyFingerprint(const FingerprintTemplate& fpTemplate, int& score);
    
    // ========== Error Handling ==========
    
    /**
     * @brief Get last error message
     * @return Error message string, or empty if no error
     */
    QString getLastError() const { return m_lastError; }
    
    /**
     * @brief Check if last operation had an error
     * @return true if there was an error, false otherwise
     */
    bool hasError() const { return !m_lastError.isEmpty(); }
    
    /**
     * @brief Clear last error message
     */
    void clearError() { m_lastError.clear(); }

private:
    FpContext* m_context;
    FpDevice* m_device;
    FpPrint* m_enrollPrint;
    FpImage* m_lastImage; // Store last captured image
    QString m_lastError;
    int m_enrollmentCount;
    bool m_enrollmentInProgress;
    ProgressCallback m_progressCallback;
    
    void setError(const QString& error);
    bool captureAndMatch(FpPrint* storedPrint, bool& matched, int& score);
    QImage convertFpImageToQImage(FpImage* fpImage);
};

#endif // FINGERPRINT_MANAGER_H

