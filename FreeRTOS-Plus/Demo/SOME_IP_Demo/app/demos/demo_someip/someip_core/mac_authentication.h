/**
 * @file mac_authentication.h
 * @brief MAC Authentication for SOME/IP Power Management
 * 
 * Implements HMAC-SHA256 authentication for PM heartbeat and profile messages.
 * 
 * @author Integration Team
 * @date 2026-01-28
 * @version 1.0
 */

#ifndef MAC_AUTHENTICATION_H
#define MAC_AUTHENTICATION_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================== */
/*                              CONFIGURATION                                  */
/* ========================================================================== */

/** MAC tag length (truncated from 32 bytes SHA256) */
#define MAC_TAG_LENGTH 16

/** Timestamp length (8 bytes, IEEE 754 double) */
#define MAC_TIMESTAMP_LENGTH 8

/** Freshness timeout in seconds */
#define MAC_FRESHNESS_TIMEOUT_SEC 5

/** Replay window (allow out-of-order within this range) */
#define MAC_REPLAY_WINDOW 100

/** Maximum number of tracked sources */
#define MAC_MAX_SOURCES 32

/** Development key (CHANGE IN PRODUCTION!) */
#define MAC_DEV_KEY "SecureKey_SOME_IP_Power_Management_2026"
#define MAC_DEV_KEY_LEN 40

/* ========================================================================== */
/*                             MESSAGE HEADERS                                 */
/* ========================================================================== */

/** Power Management Heartbeat Frame Header */
#define PM_HEADER_HEARTBEAT 0xFFFE8FFE

/** Power Management Profile Request Header */
#define PM_HEADER_PROFILE 0xFFFD8FFF

/** Profile Request Types */
#define PM_PROFILE_ACTIVATE   0x01
#define PM_PROFILE_DEACTIVATE 0x02

/* ========================================================================== */
/*                              DATA STRUCTURES                                */
/* ========================================================================== */

/**
 * @brief MAC Authentication Context
 */
typedef struct {
    uint8_t key[64];              /**< HMAC key (padded to block size) */
    uint32_t key_length;          /**< Actual key length */
    
    /* Replay protection */
    uint32_t last_counters[MAC_MAX_SOURCES];  /**< Last seen counter per source */
    uint32_t source_ips[MAC_MAX_SOURCES];     /**< Source IP addresses */
    uint8_t  num_sources;                      /**< Number of tracked sources */
    
    /* Statistics */
    uint32_t generated;           /**< Messages generated */
    uint32_t verified;            /**< Messages verified */
    uint32_t failed;              /**< Verification failures */
    uint32_t replays_detected;    /**< Replay attacks detected */
    uint32_t freshness_violations;/**< Freshness violations */
    
    /* Key management */
    uint32_t key_created_at;      /**< Timestamp when key was set */
    
} mac_context_t;

/**
 * @brief MAC authenticated heartbeat message structure
 */
typedef struct __attribute__((packed)) {
    uint32_t header;              /**< 0xFFFE8FFE */
    uint32_t length;              /**< Payload length (16 bytes) */
    uint32_t someip_reserved;     /**< 0x00000000 */
    uint8_t  protocol_version;    /**< 0x01 */
    uint8_t  interface_version;   /**< 0x01 */
    uint8_t  message_type;        /**< 0x02 */
    uint8_t  return_code;         /**< 0x00 */
    uint32_t source_ip;           /**< Source PM ID (IP address) */
    uint32_t counter;             /**< Heartbeat counter */
    uint8_t  timestamp[MAC_TIMESTAMP_LENGTH];  /**< Timestamp */
    uint8_t  mac_tag[MAC_TAG_LENGTH];          /**< HMAC-SHA256 tag */
} mac_heartbeat_t;

#define MAC_HEARTBEAT_SIZE sizeof(mac_heartbeat_t)

/**
 * @brief MAC authenticated profile request message structure
 */
typedef struct __attribute__((packed)) {
    uint32_t header;              /**< 0xFFFD8FFF */
    uint32_t length;              /**< Payload length (26 bytes) */
    uint32_t someip_reserved;     /**< 0x00000000 */
    uint8_t  protocol_version;    /**< 0x01 */
    uint8_t  interface_version;   /**< 0x01 */
    uint8_t  message_type;        /**< 0x02 */
    uint8_t  return_code;         /**< 0x00 */
    uint32_t source_ip;           /**< Source PM ID */
    uint32_t dest_ip;             /**< Destination PM ID */
    uint8_t  msg_type_0;          /**< 0x00 */
    uint8_t  entry_length;        /**< 0x06 */
    uint8_t  profile_id[5];       /**< 40-bit profile ID */
    uint8_t  request_type;        /**< ACTIVATE or DEACTIVATE */
    uint8_t  msg_type_1;          /**< 0x01 */
    uint8_t  no_entries;          /**< 0x00 */
    uint8_t  timestamp[MAC_TIMESTAMP_LENGTH];  /**< Timestamp */
    uint8_t  mac_tag[MAC_TAG_LENGTH];          /**< HMAC-SHA256 tag */
} mac_profile_request_t;

#define MAC_PROFILE_REQUEST_SIZE sizeof(mac_profile_request_t)

/**
 * @brief Error codes
 */
typedef enum {
    MAC_OK = 0,                   /**< Success */
    MAC_ERROR_NULL_PARAM,         /**< Null parameter */
    MAC_ERROR_INVALID_LENGTH,     /**< Invalid message length */
    MAC_ERROR_VERIFICATION_FAILED,/**< MAC verification failed */
    MAC_ERROR_REPLAY_DETECTED,    /**< Replay attack detected */
    MAC_ERROR_FRESHNESS_VIOLATION,/**< Message too old */
    MAC_ERROR_INVALID_HEADER,     /**< Invalid message header */
    MAC_ERROR_KEY_NOT_SET,        /**< Key not initialized */
} mac_error_t;

/* ========================================================================== */
/*                           FUNCTION DECLARATIONS                             */
/* ========================================================================== */

/**
 * @brief Initialize MAC authentication context
 * 
 * @param ctx Pointer to MAC context
 * @param key HMAC key (NULL to use default development key)
 * @param key_len Key length in bytes
 * @return MAC_OK on success, error code otherwise
 */
mac_error_t mac_init(mac_context_t *ctx, const uint8_t *key, uint32_t key_len);

/**
 * @brief Sign a heartbeat message with MAC
 * 
 * @param ctx MAC context
 * @param msg Pointer to heartbeat message (must have space for timestamp + MAC)
 * @param source_ip Source IP address
 * @param counter Heartbeat counter
 * @return MAC_OK on success, error code otherwise
 */
mac_error_t mac_sign_heartbeat(mac_context_t *ctx,
                                mac_heartbeat_t *msg,
                                uint32_t source_ip,
                                uint32_t counter);

/**
 * @brief Verify a heartbeat message MAC
 * 
 * @param ctx MAC context
 * @param msg Pointer to heartbeat message
 * @param source_ip_out Output: source IP address
 * @param counter_out Output: heartbeat counter
 * @return MAC_OK on success, error code otherwise
 */
mac_error_t mac_verify_heartbeat(mac_context_t *ctx,
                                  const mac_heartbeat_t *msg,
                                  uint32_t *source_ip_out,
                                  uint32_t *counter_out);

/**
 * @brief Sign a profile request message with MAC
 * 
 * @param ctx MAC context
 * @param msg Pointer to profile request message
 * @param source_ip Source IP address
 * @param dest_ip Destination IP address
 * @param profile_id Profile ID (40 bits)
 * @param request_type ACTIVATE or DEACTIVATE
 * @return MAC_OK on success, error code otherwise
 */
mac_error_t mac_sign_profile_request(mac_context_t *ctx,
                                      mac_profile_request_t *msg,
                                      uint32_t source_ip,
                                      uint32_t dest_ip,
                                      uint64_t profile_id,
                                      uint8_t request_type);

/**
 * @brief Verify a profile request message MAC
 * 
 * @param ctx MAC context
 * @param msg Pointer to profile request message
 * @param source_ip_out Output: source IP
 * @param dest_ip_out Output: destination IP
 * @param profile_id_out Output: profile ID
 * @param request_type_out Output: request type
 * @return MAC_OK on success, error code otherwise
 */
mac_error_t mac_verify_profile_request(mac_context_t *ctx,
                                        const mac_profile_request_t *msg,
                                        uint32_t *source_ip_out,
                                        uint32_t *dest_ip_out,
                                        uint64_t *profile_id_out,
                                        uint8_t *request_type_out);

/**
 * @brief Check for replay attack
 * 
 * @param ctx MAC context
 * @param source_ip Source IP address
 * @param counter Message counter
 * @return true if message is fresh, false if replay detected
 */
bool mac_check_replay(mac_context_t *ctx, uint32_t source_ip, uint32_t counter);

/**
 * @brief Get current timestamp (seconds since epoch as double)
 * 
 * @return Current timestamp
 */
double mac_get_timestamp(void);

/**
 * @brief Rotate to a new key
 * 
 * @param ctx MAC context
 * @param new_key New HMAC key
 * @param key_len Key length
 * @return MAC_OK on success
 */
mac_error_t mac_rotate_key(mac_context_t *ctx, const uint8_t *new_key, uint32_t key_len);

/**
 * @brief Get MAC statistics
 * 
 * @param ctx MAC context
 * @param generated_out Output: messages generated
 * @param verified_out Output: messages verified
 * @param failed_out Output: verification failures
 * @param replays_out Output: replays detected
 * @param freshness_violations_out Output: freshness violations
 */
void mac_get_statistics(const mac_context_t *ctx,
                        uint32_t *generated_out,
                        uint32_t *verified_out,
                        uint32_t *failed_out,
                        uint32_t *replays_out,
                        uint32_t *freshness_violations_out);

/**
 * @brief Reset statistics
 * 
 * @param ctx MAC context
 */
void mac_reset_statistics(mac_context_t *ctx);

/**
 * @brief Get error string for error code
 * 
 * @param error Error code
 * @return Human-readable error string
 */
const char* mac_error_string(mac_error_t error);

/* ========================================================================== */
/*                            HELPER MACROS                                    */
/* ========================================================================== */

/** Convert network byte order to host */
#define NTOHL(x) __builtin_bswap32(x)
#define HTONL(x) __builtin_bswap32(x)

/** Extract IP bytes */
#define IP_BYTE(ip, n) (((ip) >> (24 - (n)*8)) & 0xFF)

/** Build IP from bytes */
#define BUILD_IP(a,b,c,d) (((uint32_t)(a) << 24) | ((uint32_t)(b) << 16) | \
                            ((uint32_t)(c) << 8) | ((uint32_t)(d)))

#ifdef __cplusplus
}
#endif

#endif /* MAC_AUTHENTICATION_H */
