/**
 * @file mac_authentication.c
 * @brief MAC Authentication Implementation for FreeRTOS
 * 
 * Implements HMAC-SHA256 for Power Management messages.
 * This implementation uses mbedTLS library for SHA256 if available,
 * or includes a standalone implementation.
 * 
 * @author Integration Team
 * @date 2026-01-28
 */

#include "mac_authentication.h"
#include <string.h>
#include <stdio.h>

/* Try to use mbedTLS if available, otherwise use standalone SHA256 */
#ifdef MBEDTLS_SHA256_C
#include "mbedtls/sha256.h"
#define SHA256_CTX mbedtls_sha256_context
#define SHA256_Init(ctx) mbedtls_sha256_init(ctx); mbedtls_sha256_starts_ret(ctx, 0)
#define SHA256_Update mbedtls_sha256_update_ret
#define SHA256_Final(digest, ctx) mbedtls_sha256_finish_ret(ctx, digest); mbedtls_sha256_free(ctx)
#else
/* Standalone SHA256 implementation will be included below */
#include "sha256_standalone.h"
#endif

/* FreeRTOS includes for timestamp */
#ifdef USE_FREERTOS
#include "FreeRTOS.h"
#include "task.h"
#endif

/* ========================================================================== */
/*                         INTERNAL HELPER FUNCTIONS                           */
/* ========================================================================== */

/**
 * @brief Get current timestamp
 */
double mac_get_timestamp(void) {
#ifdef USE_FREERTOS
    /* FreeRTOS tick count (convert to seconds) */
    TickType_t ticks = xTaskGetTickCount();
    return (double)ticks / (double)configTICK_RATE_HZ;
#else
    /* Use system time if available */
    #include <time.h>
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
#endif
}

/**
 * @brief Encode timestamp as IEEE 754 double (8 bytes, big-endian)
 */
static void encode_timestamp(uint8_t *buffer, double timestamp) {
    /* Simple encoding: convert double to uint64_t and store big-endian */
    union {
        double d;
        uint64_t u;
    } conv;
    
    conv.d = timestamp;
    
    /* Store as big-endian */
    buffer[0] = (conv.u >> 56) & 0xFF;
    buffer[1] = (conv.u >> 48) & 0xFF;
    buffer[2] = (conv.u >> 40) & 0xFF;
    buffer[3] = (conv.u >> 32) & 0xFF;
    buffer[4] = (conv.u >> 24) & 0xFF;
    buffer[5] = (conv.u >> 16) & 0xFF;
    buffer[6] = (conv.u >> 8) & 0xFF;
    buffer[7] = conv.u & 0xFF;
}

/**
 * @brief Decode timestamp from IEEE 754 double (8 bytes, big-endian)
 */
static double decode_timestamp(const uint8_t *buffer) {
    union {
        double d;
        uint64_t u;
    } conv;
    
    /* Read as big-endian */
    conv.u = ((uint64_t)buffer[0] << 56) |
             ((uint64_t)buffer[1] << 48) |
             ((uint64_t)buffer[2] << 40) |
             ((uint64_t)buffer[3] << 32) |
             ((uint64_t)buffer[4] << 24) |
             ((uint64_t)buffer[5] << 16) |
             ((uint64_t)buffer[6] << 8) |
             ((uint64_t)buffer[7]);
    
    return conv.d;
}

/**
 * @brief Constant-time memory comparison (timing attack resistant)
 */
static bool constant_time_compare(const uint8_t *a, const uint8_t *b, size_t len) {
    uint8_t diff = 0;
    
    for (size_t i = 0; i < len; i++) {
        diff |= a[i] ^ b[i];
    }
    
    return diff == 0;
}

/* ========================================================================== */
/*                          HMAC-SHA256 IMPLEMENTATION                         */
/* ========================================================================== */

#define SHA256_BLOCK_SIZE 64
#define SHA256_DIGEST_SIZE 32

/**
 * @brief Compute HMAC-SHA256
 * 
 * @param key HMAC key
 * @param key_len Key length
 * @param message Message to authenticate
 * @param message_len Message length
 * @param mac Output MAC (must be 32 bytes)
 */
static void hmac_sha256(const uint8_t *key, uint32_t key_len,
                        const uint8_t *message, uint32_t message_len,
                        uint8_t *mac) {
    SHA256_CTX ctx;
    uint8_t key_pad[SHA256_BLOCK_SIZE];
    uint8_t i_pad[SHA256_BLOCK_SIZE];
    uint8_t o_pad[SHA256_BLOCK_SIZE];
    uint8_t inner_hash[SHA256_DIGEST_SIZE];
    
    /* Prepare key (hash if longer than block size) */
    if (key_len > SHA256_BLOCK_SIZE) {
        SHA256_Init(&ctx);
        SHA256_Update(&ctx, key, key_len);
        SHA256_Final(key_pad, &ctx);
        
        /* Pad with zeros */
        memset(key_pad + SHA256_DIGEST_SIZE, 0, SHA256_BLOCK_SIZE - SHA256_DIGEST_SIZE);
    } else {
        /* Copy key and pad with zeros */
        memcpy(key_pad, key, key_len);
        memset(key_pad + key_len, 0, SHA256_BLOCK_SIZE - key_len);
    }
    
    /* Prepare i_pad and o_pad */
    for (int i = 0; i < SHA256_BLOCK_SIZE; i++) {
        i_pad[i] = key_pad[i] ^ 0x36;
        o_pad[i] = key_pad[i] ^ 0x5C;
    }
    
    /* Compute inner hash: H((K XOR i_pad) || message) */
    SHA256_Init(&ctx);
    SHA256_Update(&ctx, i_pad, SHA256_BLOCK_SIZE);
    SHA256_Update(&ctx, message, message_len);
    SHA256_Final(inner_hash, &ctx);
    
    /* Compute outer hash: H((K XOR o_pad) || inner_hash) */
    SHA256_Init(&ctx);
    SHA256_Update(&ctx, o_pad, SHA256_BLOCK_SIZE);
    SHA256_Update(&ctx, inner_hash, SHA256_DIGEST_SIZE);
    SHA256_Final(mac, &ctx);
    
    /* Clear sensitive data */
    memset(key_pad, 0, sizeof(key_pad));
    memset(i_pad, 0, sizeof(i_pad));
    memset(o_pad, 0, sizeof(o_pad));
    memset(inner_hash, 0, sizeof(inner_hash));
}

/* ========================================================================== */
/*                          PUBLIC API IMPLEMENTATION                          */
/* ========================================================================== */

mac_error_t mac_init(mac_context_t *ctx, const uint8_t *key, uint32_t key_len) {
    if (ctx == NULL) {
        return MAC_ERROR_NULL_PARAM;
    }
    
    /* Clear context */
    memset(ctx, 0, sizeof(mac_context_t));
    
    /* Set key (use default if NULL) */
    if (key == NULL) {
        /* Use development key */
        memcpy(ctx->key, MAC_DEV_KEY, MAC_DEV_KEY_LEN);
        ctx->key_length = MAC_DEV_KEY_LEN;
    } else {
        /* Use provided key */
        if (key_len > sizeof(ctx->key)) {
            key_len = sizeof(ctx->key);
        }
        memcpy(ctx->key, key, key_len);
        ctx->key_length = key_len;
    }
    
    /* Initialize replay protection */
    for (int i = 0; i < MAC_MAX_SOURCES; i++) {
        ctx->last_counters[i] = 0;
        ctx->source_ips[i] = 0;
    }
    ctx->num_sources = 0;
    
    /* Set key creation timestamp */
    ctx->key_created_at = (uint32_t)mac_get_timestamp();
    
    return MAC_OK;
}

mac_error_t mac_sign_heartbeat(mac_context_t *ctx,
                                mac_heartbeat_t *msg,
                                uint32_t source_ip,
                                uint32_t counter) {
    if (ctx == NULL || msg == NULL) {
        return MAC_ERROR_NULL_PARAM;
    }
    
    if (ctx->key_length == 0) {
        return MAC_ERROR_KEY_NOT_SET;
    }
    
    /* Fill in message fields */
    msg->header = HTONL(PM_HEADER_HEARTBEAT);
    msg->length = HTONL(16);  /* Payload length without timestamp and MAC */
    msg->someip_reserved = 0;
    msg->protocol_version = 0x01;
    msg->interface_version = 0x01;
    msg->message_type = 0x02;
    msg->return_code = 0x00;
    msg->source_ip = HTONL(source_ip);
    msg->counter = HTONL(counter);
    
    /* Add timestamp */
    double timestamp = mac_get_timestamp();
    encode_timestamp(msg->timestamp, timestamp);
    
    /* Compute MAC over everything except MAC tag itself */
    uint8_t mac_full[SHA256_DIGEST_SIZE];
    uint32_t message_len = MAC_HEARTBEAT_SIZE - MAC_TAG_LENGTH;
    
    hmac_sha256(ctx->key, ctx->key_length,
                (uint8_t*)msg, message_len,
                mac_full);
    
    /* Truncate MAC to 16 bytes */
    memcpy(msg->mac_tag, mac_full, MAC_TAG_LENGTH);
    
    /* Update statistics */
    ctx->generated++;
    
    /* Clear sensitive data */
    memset(mac_full, 0, sizeof(mac_full));
    
    return MAC_OK;
}

mac_error_t mac_verify_heartbeat(mac_context_t *ctx,
                                  const mac_heartbeat_t *msg,
                                  uint32_t *source_ip_out,
                                  uint32_t *counter_out) {
    if (ctx == NULL || msg == NULL) {
        return MAC_ERROR_NULL_PARAM;
    }
    
    if (ctx->key_length == 0) {
        return MAC_ERROR_KEY_NOT_SET;
    }
    
    /* Verify header */
    if (NTOHL(msg->header) != PM_HEADER_HEARTBEAT) {
        ctx->failed++;
        return MAC_ERROR_INVALID_HEADER;
    }
    
    /* Verify length */
    if (NTOHL(msg->length) != 16) {
        ctx->failed++;
        return MAC_ERROR_INVALID_LENGTH;
    }
    
    /* Extract fields */
    uint32_t source_ip = NTOHL(msg->source_ip);
    uint32_t counter = NTOHL(msg->counter);
    
    /* Check freshness */
    double msg_timestamp = decode_timestamp(msg->timestamp);
    double current_time = mac_get_timestamp();
    double age = current_time - msg_timestamp;
    
    if (age > MAC_FRESHNESS_TIMEOUT_SEC) {
        ctx->freshness_violations++;
        return MAC_ERROR_FRESHNESS_VIOLATION;
    }
    
    /* Compute expected MAC */
    uint8_t mac_full[SHA256_DIGEST_SIZE];
    uint32_t message_len = MAC_HEARTBEAT_SIZE - MAC_TAG_LENGTH;
    
    hmac_sha256(ctx->key, ctx->key_length,
                (uint8_t*)msg, message_len,
                mac_full);
    
    /* Verify MAC (constant-time comparison) */
    if (!constant_time_compare(msg->mac_tag, mac_full, MAC_TAG_LENGTH)) {
        ctx->failed++;
        memset(mac_full, 0, sizeof(mac_full));
        return MAC_ERROR_VERIFICATION_FAILED;
    }
    
    /* Check for replay */
    if (!mac_check_replay(ctx, source_ip, counter)) {
        ctx->replays_detected++;
        memset(mac_full, 0, sizeof(mac_full));
        return MAC_ERROR_REPLAY_DETECTED;
    }
    
    /* Success */
    ctx->verified++;
    
    if (source_ip_out != NULL) {
        *source_ip_out = source_ip;
    }
    if (counter_out != NULL) {
        *counter_out = counter;
    }
    
    /* Clear sensitive data */
    memset(mac_full, 0, sizeof(mac_full));
    
    return MAC_OK;
}

bool mac_check_replay(mac_context_t *ctx, uint32_t source_ip, uint32_t counter) {
    if (ctx == NULL) {
        return false;
    }
    
    /* Find source in table */
    int source_index = -1;
    for (int i = 0; i < ctx->num_sources; i++) {
        if (ctx->source_ips[i] == source_ip) {
            source_index = i;
            break;
        }
    }
    
    /* If source not found, add it */
    if (source_index < 0) {
        if (ctx->num_sources < MAC_MAX_SOURCES) {
            source_index = ctx->num_sources;
            ctx->source_ips[source_index] = source_ip;
            ctx->last_counters[source_index] = 0;
            ctx->num_sources++;
        } else {
            /* Table full, accept message (could implement LRU eviction) */
            return true;
        }
    }
    
    /* Check counter */
    uint32_t last_counter = ctx->last_counters[source_index];
    
    /* Counter must be greater than last, or within replay window */
    if (counter <= last_counter) {
        /* Allow out-of-order delivery within window */
        if (last_counter - counter > MAC_REPLAY_WINDOW) {
            /* Definitely a replay */
            return false;
        }
    }
    
    /* Update last counter */
    if (counter > last_counter) {
        ctx->last_counters[source_index] = counter;
    }
    
    return true;
}

mac_error_t mac_sign_profile_request(mac_context_t *ctx,
                                      mac_profile_request_t *msg,
                                      uint32_t source_ip,
                                      uint32_t dest_ip,
                                      uint64_t profile_id,
                                      uint8_t request_type) {
    if (ctx == NULL || msg == NULL) {
        return MAC_ERROR_NULL_PARAM;
    }
    
    if (ctx->key_length == 0) {
        return MAC_ERROR_KEY_NOT_SET;
    }
    
    /* Fill in message fields */
    msg->header = HTONL(PM_HEADER_PROFILE);
    msg->length = HTONL(26);  /* Payload length without timestamp and MAC */
    msg->someip_reserved = 0;
    msg->protocol_version = 0x01;
    msg->interface_version = 0x01;
    msg->message_type = 0x02;
    msg->return_code = 0x00;
    msg->source_ip = HTONL(source_ip);
    msg->dest_ip = HTONL(dest_ip);
    
    /* Message Type 0 */
    msg->msg_type_0 = 0x00;
    msg->entry_length = 0x06;
    
    /* Profile ID (40 bits = 5 bytes, big-endian) */
    msg->profile_id[0] = (profile_id >> 32) & 0xFF;
    msg->profile_id[1] = (profile_id >> 24) & 0xFF;
    msg->profile_id[2] = (profile_id >> 16) & 0xFF;
    msg->profile_id[3] = (profile_id >> 8) & 0xFF;
    msg->profile_id[4] = profile_id & 0xFF;
    
    /* Request type */
    msg->request_type = request_type;
    
    /* Message Type 1 */
    msg->msg_type_1 = 0x01;
    msg->no_entries = 0x00;
    
    /* Add timestamp */
    double timestamp = mac_get_timestamp();
    encode_timestamp(msg->timestamp, timestamp);
    
    /* Compute MAC */
    uint8_t mac_full[SHA256_DIGEST_SIZE];
    uint32_t message_len = MAC_PROFILE_REQUEST_SIZE - MAC_TAG_LENGTH;
    
    hmac_sha256(ctx->key, ctx->key_length,
                (uint8_t*)msg, message_len,
                mac_full);
    
    /* Truncate MAC */
    memcpy(msg->mac_tag, mac_full, MAC_TAG_LENGTH);
    
    /* Update statistics */
    ctx->generated++;
    
    /* Clear sensitive data */
    memset(mac_full, 0, sizeof(mac_full));
    
    return MAC_OK;
}

mac_error_t mac_verify_profile_request(mac_context_t *ctx,
                                        const mac_profile_request_t *msg,
                                        uint32_t *source_ip_out,
                                        uint32_t *dest_ip_out,
                                        uint64_t *profile_id_out,
                                        uint8_t *request_type_out) {
    if (ctx == NULL || msg == NULL) {
        return MAC_ERROR_NULL_PARAM;
    }
    
    if (ctx->key_length == 0) {
        return MAC_ERROR_KEY_NOT_SET;
    }
    
    /* Verify header */
    if (NTOHL(msg->header) != PM_HEADER_PROFILE) {
        ctx->failed++;
        return MAC_ERROR_INVALID_HEADER;
    }
    
    /* Check freshness */
    double msg_timestamp = decode_timestamp(msg->timestamp);
    double current_time = mac_get_timestamp();
    double age = current_time - msg_timestamp;
    
    if (age > MAC_FRESHNESS_TIMEOUT_SEC) {
        ctx->freshness_violations++;
        return MAC_ERROR_FRESHNESS_VIOLATION;
    }
    
    /* Compute expected MAC */
    uint8_t mac_full[SHA256_DIGEST_SIZE];
    uint32_t message_len = MAC_PROFILE_REQUEST_SIZE - MAC_TAG_LENGTH;
    
    hmac_sha256(ctx->key, ctx->key_length,
                (uint8_t*)msg, message_len,
                mac_full);
    
    /* Verify MAC */
    if (!constant_time_compare(msg->mac_tag, mac_full, MAC_TAG_LENGTH)) {
        ctx->failed++;
        memset(mac_full, 0, sizeof(mac_full));
        return MAC_ERROR_VERIFICATION_FAILED;
    }
    
    /* Success - extract fields */
    ctx->verified++;
    
    if (source_ip_out != NULL) {
        *source_ip_out = NTOHL(msg->source_ip);
    }
    if (dest_ip_out != NULL) {
        *dest_ip_out = NTOHL(msg->dest_ip);
    }
    if (profile_id_out != NULL) {
        *profile_id_out = ((uint64_t)msg->profile_id[0] << 32) |
                          ((uint64_t)msg->profile_id[1] << 24) |
                          ((uint64_t)msg->profile_id[2] << 16) |
                          ((uint64_t)msg->profile_id[3] << 8) |
                          ((uint64_t)msg->profile_id[4]);
    }
    if (request_type_out != NULL) {
        *request_type_out = msg->request_type;
    }
    
    /* Clear sensitive data */
    memset(mac_full, 0, sizeof(mac_full));
    
    return MAC_OK;
}

mac_error_t mac_rotate_key(mac_context_t *ctx, const uint8_t *new_key, uint32_t key_len) {
    if (ctx == NULL || new_key == NULL) {
        return MAC_ERROR_NULL_PARAM;
    }
    
    /* Clear old key */
    memset(ctx->key, 0, sizeof(ctx->key));
    
    /* Set new key */
    if (key_len > sizeof(ctx->key)) {
        key_len = sizeof(ctx->key);
    }
    memcpy(ctx->key, new_key, key_len);
    ctx->key_length = key_len;
    
    /* Update timestamp */
    ctx->key_created_at = (uint32_t)mac_get_timestamp();
    
    /* Clear replay protection (old counters invalid with new key) */
    memset(ctx->last_counters, 0, sizeof(ctx->last_counters));
    memset(ctx->source_ips, 0, sizeof(ctx->source_ips));
    ctx->num_sources = 0;
    
    return MAC_OK;
}

void mac_get_statistics(const mac_context_t *ctx,
                        uint32_t *generated_out,
                        uint32_t *verified_out,
                        uint32_t *failed_out,
                        uint32_t *replays_out,
                        uint32_t *freshness_violations_out) {
    if (ctx == NULL) {
        return;
    }
    
    if (generated_out != NULL) *generated_out = ctx->generated;
    if (verified_out != NULL) *verified_out = ctx->verified;
    if (failed_out != NULL) *failed_out = ctx->failed;
    if (replays_out != NULL) *replays_out = ctx->replays_detected;
    if (freshness_violations_out != NULL) *freshness_violations_out = ctx->freshness_violations;
}

void mac_reset_statistics(mac_context_t *ctx) {
    if (ctx == NULL) {
        return;
    }
    
    ctx->generated = 0;
    ctx->verified = 0;
    ctx->failed = 0;
    ctx->replays_detected = 0;
    ctx->freshness_violations = 0;
}

const char* mac_error_string(mac_error_t error) {
    switch (error) {
        case MAC_OK:
            return "Success";
        case MAC_ERROR_NULL_PARAM:
            return "Null parameter";
        case MAC_ERROR_INVALID_LENGTH:
            return "Invalid message length";
        case MAC_ERROR_VERIFICATION_FAILED:
            return "MAC verification failed";
        case MAC_ERROR_REPLAY_DETECTED:
            return "Replay attack detected";
        case MAC_ERROR_FRESHNESS_VIOLATION:
            return "Message too old (freshness violation)";
        case MAC_ERROR_INVALID_HEADER:
            return "Invalid message header";
        case MAC_ERROR_KEY_NOT_SET:
            return "Key not initialized";
        default:
            return "Unknown error";
    }
}
