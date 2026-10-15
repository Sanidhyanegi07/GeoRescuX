#ifndef GEORESCUX_CRYPTO_PACKET_AUTHENTICATOR_H
#define GEORESCUX_CRYPTO_PACKET_AUTHENTICATOR_H

#include <vector>
#include <string>
#include <cstdint>
#include <array>

namespace georescux::crypto {

/**
 * Embedded cryptographic engine for disaster distress packet authentication.
 * Implements FIPS-180-4 SHA-256 and RFC 2104 HMAC-SHA256 with constant-time verification
 * to protect disaster networks against spoofed SOS beacons and tampering.
 */
class PacketAuthenticator {
public:
    static constexpr size_t SHA256_BLOCK_SIZE = 64;
    static constexpr size_t SHA256_DIGEST_SIZE = 32;

    using DigestArray = std::array<uint8_t, SHA256_DIGEST_SIZE>;

    /**
     * Computes raw SHA-256 hash of a buffer.
     */
    static DigestArray sha256(const uint8_t* data, size_t length) noexcept;

    /**
     * Computes raw SHA-256 hash of a string.
     */
    static DigestArray sha256(const std::string& input) noexcept;

    /**
     * Computes HMAC-SHA256 with a pre-shared secret key.
     */
    static DigestArray hmacSha256(
        const uint8_t* key, size_t keyLength,
        const uint8_t* message, size_t messageLength
    ) noexcept;

    /**
     * Constant-time comparison between two 32-byte digests to prevent timing attacks.
     */
    static bool constantTimeEqual(const DigestArray& a, const DigestArray& b) noexcept;

    /**
     * Converts a 32-byte digest into a lowercase hexadecimal string.
     */
    static std::string toHexString(const DigestArray& digest);

private:
    struct Sha256Context {
        uint32_t state[8];
        uint64_t bitCount;
        uint8_t buffer[64];
    };

    static void sha256Init(Sha256Context& ctx) noexcept;
    static void sha256Update(Sha256Context& ctx, const uint8_t* data, size_t len) noexcept;
    static void sha256Final(Sha256Context& ctx, uint8_t digest[32]) noexcept;
    static void sha256Transform(uint32_t state[8], const uint8_t data[64]) noexcept;
};

} // namespace georescux::crypto

#endif // GEORESCUX_CRYPTO_PACKET_AUTHENTICATOR_H
