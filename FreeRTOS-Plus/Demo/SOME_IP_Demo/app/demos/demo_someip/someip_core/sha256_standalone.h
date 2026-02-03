/**
 * @file sha256_standalone.h
 * @brief Standalone SHA256 implementation for embedded systems
 * 
 * This is a simple, portable SHA256 implementation for systems
 * without crypto libraries. Based on FIPS 180-4.
 */

#ifndef SHA256_STANDALONE_H
#define SHA256_STANDALONE_H

#include <stdint.h>
#include <stddef.h>

typedef struct {
    uint32_t state[8];
    uint32_t count[2];
    uint8_t buffer[64];
} SHA256_CTX;

void SHA256_Init(SHA256_CTX *ctx);
void SHA256_Update(SHA256_CTX *ctx, const uint8_t *data, size_t len);
void SHA256_Final(uint8_t *digest, SHA256_CTX *ctx);

#endif /* SHA256_STANDALONE_H */
