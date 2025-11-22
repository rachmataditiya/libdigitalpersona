#ifndef DIGITALPERSONA_H
#define DIGITALPERSONA_H

/**
 * @file digitalpersona.h
 * @brief Main include file for DigitalPersona Fingerprint Library
 * @version 1.0.0
 * 
 * This library provides fingerprint enrollment and verification capabilities
 * for DigitalPersona U.are.U fingerprint readers using libfprint.
 * 
 * Features:
 * - Fingerprint enrollment (registration) with multiple scans
 * - Fingerprint verification against stored templates
 * - Thread-safe operations
 * - Qt integration
 * 
 * Note: Database management is NOT included in this library.
 * Applications should handle their own database operations.
 * 
 * Example usage:
 * @code
 * #include <digitalpersona.h>
 * 
 * // Initialize fingerprint manager
 * FingerprintManager* fpManager = new FingerprintManager();
 * if (!fpManager->initialize()) {
 *     qWarning() << "Failed to initialize:" << fpManager->getLastError();
 *     return;
 * }
 * 
 * // Open device
 * if (!fpManager->openReader()) {
 *     qWarning() << "Failed to open reader:" << fpManager->getLastError();
 *     return;
 * }
 * 
 * // Start enrollment
 * if (fpManager->startEnrollment()) {
 *     QString message;
 *     int quality;
 *     int result = fpManager->addEnrollmentSample(message, quality);
 *     
 *     if (result == 1) {
 *         // Enrollment complete
 *         QByteArray templateData;
 *         fpManager->createEnrollmentTemplate(templateData);
 *         // Save templateData to your database
 *     }
 * }
 * @endcode
 */

#include "digitalpersona_global.h"
#include "fingerprint_manager.h"

// Library version
#define DIGITALPERSONA_VERSION_MAJOR 1
#define DIGITALPERSONA_VERSION_MINOR 0
#define DIGITALPERSONA_VERSION_PATCH 0
#define DIGITALPERSONA_VERSION_STRING "1.0.0"

namespace DigitalPersona {

/**
 * @brief Get library version string
 * @return Version string in format "major.minor.patch"
 */
inline const char* version() {
    return DIGITALPERSONA_VERSION_STRING;
}

/**
 * @brief Get library version as integer
 * @return Version as integer (major * 10000 + minor * 100 + patch)
 */
inline int versionInt() {
    return DIGITALPERSONA_VERSION_MAJOR * 10000 + 
           DIGITALPERSONA_VERSION_MINOR * 100 + 
           DIGITALPERSONA_VERSION_PATCH;
}

} // namespace DigitalPersona

#endif // DIGITALPERSONA_H

