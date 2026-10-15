#include "PacketAuthenticator.h"
#include <cstring>
#include <iomanip>
#include <sstream>

namespace georescux::crypto {

namespace {

inline uint32_t rightRotate(uint32_t value, unsigned int count) noexcept {
    return (value >> count) | (value << (32 - count));
}

const uint32_t K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

} // namespace

void PacketAuthenticator::sha256Init(Sha256Context& ctx) noexcept {
    ctx.state[0] = 0x6a09e667;
    ctx.state[1] = 0xbb67ae85;
    ctx.state[2] = 0x3c6ef372;
    ctx.state[3] = 0xa54ff53a;
    ctx.state[4] = 0x510e527f;
    ctx.state[5] = 0x9b05688c;
    ctx.state[6] = 0x1f83d9ab;
    ctx.state[7] = 0x5be0cd19;
    ctx.bitCount = 0;
}

void PacketAuthenticator::sha256Transform(uint32_t state[8], const uint8_t data[64]) noexcept {
    uint32_t m[64];
    for (int i = 0, j = 0; i < 16; ++i, j += 4) {
        m[i] = (static_cast<uint32_t>(data[j]) << 24) |
               (static_cast<uint32_t>(data[j + 1]) << 16) |
               (static_cast<uint32_t>(data[j + 2]) << 8) |
               (static_cast<uint32_t>(data[j + 3]));
    }
    for (int i = 16; i < 64; ++i) {
        const uint32_t s0 = rightRotate(m[i - 15], 7) ^ rightRotate(m[i - 15], 18) ^ (m[i - 15] >> 3);
        const uint32_t s1 = rightRotate(m[i - 2], 17) ^ rightRotate(m[i - 2], 19) ^ (m[i - 2] >> 10);
        m[i] = m[i - 16] + s0 + m[i - 7] + s1;
    }

    uint32_t a = state[0], b = state[1], c = state[2], d = state[3];
    uint32_t e = state[4], f = state[5], g = state[6], h = state[7];

    for (int i = 0; i < 64; ++i) {
        const uint32_t S1 = rightRotate(e, 6) ^ rightRotate(e, 11) ^ rightRotate(e, 25);
        const uint32_t ch = (e & f) ^ ((~e) & g);
        const uint32_t temp1 = h + S1 + ch + K[i] + m[i];
        const uint32_t S0 = rightRotate(a, 2) ^ rightRotate(a, 13) ^ rightRotate(a, 22);
        const uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        const uint32_t temp2 = S0 + maj;

        h = g;
        g = f;
        f = e;
        e = d + temp1;
        d = c;
        c = b;
        b = a;
        a = temp1 + temp2;
    }

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
    state[5] += f;
    state[6] += g;
    state[7] += h;
}

void PacketAuthenticator::sha256Update(Sha256Context& ctx, const uint8_t* data, size_t len) noexcept {
    size_t bufferIndex = static_cast<size_t>((ctx.bitCount / 8) % 64);
    ctx.bitCount += static_cast<uint64_t>(len) * 8;

    size_t i = 0;
    if (bufferIndex > 0) {
        const size_t needed = 64 - bufferIndex;
        const size_t toCopy = std::min(needed, len);
        std::memcpy(&ctx.buffer[bufferIndex], data, toCopy);
        bufferIndex += toCopy;
        i += toCopy;
        if (bufferIndex == 64) {
            sha256Transform(ctx.state, ctx.buffer);
            bufferIndex = 0;
        }
    }

    while (i + 64 <= len) {
        sha256Transform(ctx.state, &data[i]);
        i += 64;
    }

    if (i < len) {
        std::memcpy(&ctx.buffer[bufferIndex], &data[i], len - i);
    }
}

void PacketAuthenticator::sha256Final(Sha256Context& ctx, uint8_t digest[32]) noexcept {
    const uint64_t totalBits = ctx.bitCount;
    size_t bufferIndex = static_cast<size_t>((totalBits / 8) % 64);

    ctx.buffer[bufferIndex++] = 0x80;
    if (bufferIndex > 56) {
        std::memset(&ctx.buffer[bufferIndex], 0, 64 - bufferIndex);
        sha256Transform(ctx.state, ctx.buffer);
        bufferIndex = 0;
    }
    std::memset(&ctx.buffer[bufferIndex], 0, 56 - bufferIndex);

    for (int i = 0; i < 8; ++i) {
        ctx.buffer[63 - i] = static_cast<uint8_t>(totalBits >> (i * 8));
    }
    sha256Transform(ctx.state, ctx.buffer);

    for (int i = 0; i < 8; ++i) {
        digest[i * 4]     = static_cast<uint8_t>(ctx.state[i] >> 24);
        digest[i * 4 + 1] = static_cast<uint8_t>(ctx.state[i] >> 16);
        digest[i * 4 + 2] = static_cast<uint8_t>(ctx.state[i] >> 8);
        digest[i * 4 + 3] = static_cast<uint8_t>(ctx.state[i]);
    }
}

PacketAuthenticator::DigestArray PacketAuthenticator::sha256(const uint8_t* data, size_t length) noexcept {
    Sha256Context ctx;
    sha256Init(ctx);
    sha256Update(ctx, data, length);
    DigestArray result{};
    sha256Final(ctx, result.data());
    return result;
}

PacketAuthenticator::DigestArray PacketAuthenticator::sha256(const std::string& input) noexcept {
    return sha256(reinterpret_cast<const uint8_t*>(input.data()), input.size());
}

PacketAuthenticator::DigestArray PacketAuthenticator::hmacSha256(
    const uint8_t* key, size_t keyLength,
    const uint8_t* message, size_t messageLength
) noexcept {
    uint8_t k0[SHA256_BLOCK_SIZE];
    std::memset(k0, 0, SHA256_BLOCK_SIZE);

    if (keyLength > SHA256_BLOCK_SIZE) {
        DigestArray keyHash = sha256(key, keyLength);
        std::memcpy(k0, keyHash.data(), SHA256_DIGEST_SIZE);
    } else {
        std::memcpy(k0, key, keyLength);
    }

    uint8_t ipad[SHA256_BLOCK_SIZE];
    uint8_t opad[SHA256_BLOCK_SIZE];
    for (size_t i = 0; i < SHA256_BLOCK_SIZE; ++i) {
        ipad[i] = k0[i] ^ 0x36;
        opad[i] = k0[i] ^ 0x5C;
    }

    // Inner hash: H(ipad || message)
    Sha256Context innerCtx;
    sha256Init(innerCtx);
    sha256Update(innerCtx, ipad, SHA256_BLOCK_SIZE);
    sha256Update(innerCtx, message, messageLength);
    DigestArray innerDigest{};
    sha256Final(innerCtx, innerDigest.data());

    // Outer hash: H(opad || innerDigest)
    Sha256Context outerCtx;
    sha256Init(outerCtx);
    sha256Update(outerCtx, opad, SHA256_BLOCK_SIZE);
    sha256Update(outerCtx, innerDigest.data(), SHA256_DIGEST_SIZE);
    DigestArray hmacResult{};
    sha256Final(outerCtx, hmacResult.data());

    return hmacResult;
}

bool PacketAuthenticator::constantTimeEqual(const DigestArray& a, const DigestArray& b) noexcept {
    uint8_t diff = 0;
    for (size_t i = 0; i < SHA256_DIGEST_SIZE; ++i) {
        diff |= (a[i] ^ b[i]);
    }
    return diff == 0;
}

std::string PacketAuthenticator::toHexString(const DigestArray& digest) {
    std::ostringstream oss;
    for (uint8_t b : digest) {
        oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(b);
    }
    return oss.str();
}

} // namespace georescux::crypto
