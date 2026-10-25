#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

// Original implementation of SHA-256 from NIST FIPS 180-4, sections 4-6.
// Identity hashes are independent of host endianness and require no crypto library.
namespace rms_suite_identity_detail {
class Sha256 {
    uint32_t state_[8] = {
        0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
        0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u
    };
    uint8_t block_[64] = {};
    size_t used_ = 0;
    uint64_t bytes_ = 0;

    static uint32_t rotate(uint32_t value, unsigned bits) {
        return (value >> bits) | (value << (32 - bits));
    }
    void compress(const uint8_t* block) {
        static const uint32_t constants[64] = {
            0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u,
            0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
            0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u,
            0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
            0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu,
            0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
            0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u,
            0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
            0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u,
            0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
            0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u,
            0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
            0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u,
            0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
            0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
            0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u
        };
        uint32_t words[64];
        for (size_t i = 0; i < 16; ++i) {
            const uint8_t* p = block + i * 4;
            words[i] = (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16)
                     | (uint32_t(p[2]) << 8) | uint32_t(p[3]);
        }
        for (size_t i = 16; i < 64; ++i) {
            const uint32_t x = words[i - 15], y = words[i - 2];
            const uint32_t small0 = rotate(x, 7) ^ rotate(x, 18) ^ (x >> 3);
            const uint32_t small1 = rotate(y, 17) ^ rotate(y, 19) ^ (y >> 10);
            words[i] = words[i - 16] + small0 + words[i - 7] + small1;
        }
        uint32_t a = state_[0], b = state_[1], c = state_[2], d = state_[3];
        uint32_t e = state_[4], f = state_[5], g = state_[6], h = state_[7];
        for (size_t i = 0; i < 64; ++i) {
            const uint32_t big1 = rotate(e, 6) ^ rotate(e, 11) ^ rotate(e, 25);
            const uint32_t choose = (e & f) ^ (~e & g);
            const uint32_t t1 = h + big1 + choose + constants[i] + words[i];
            const uint32_t big0 = rotate(a, 2) ^ rotate(a, 13) ^ rotate(a, 22);
            const uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
            const uint32_t t2 = big0 + majority;
            h = g; g = f; f = e; e = d + t1;
            d = c; c = b; b = a; a = t1 + t2;
        }
        state_[0] += a; state_[1] += b; state_[2] += c; state_[3] += d;
        state_[4] += e; state_[5] += f; state_[6] += g; state_[7] += h;
    }

public:
    void update(const uint8_t* data, size_t size) {
        // A null pointer is valid for an empty input.
        if (!size) return;
        bytes_ += static_cast<uint64_t>(size);
        if (used_) {
            const size_t take = size < 64 - used_ ? size : 64 - used_;
            std::memcpy(block_ + used_, data, take);
            used_ += take; data += take; size -= take;
            if (used_ == 64) { compress(block_); used_ = 0; }
        }
        while (size >= 64) {
            compress(data);
            data += 64; size -= 64;
        }
        if (size) { std::memcpy(block_ + used_, data, size); used_ += size; }
    }
    void append_u64(uint64_t value) {
        uint8_t encoded[8];
        for (size_t i = 0; i < 8; ++i)
            encoded[7 - i] = static_cast<uint8_t>(value >> (8 * i));
        update(encoded, sizeof(encoded));
    }
    void append_string(const std::string& value) {
        append_u64(static_cast<uint64_t>(value.size()));
        update(reinterpret_cast<const uint8_t*>(value.data()), value.size());
    }
    std::string finish() {
        const uint64_t bits = bytes_ * 8;
        block_[used_++] = 0x80;
        if (used_ > 56) {
            std::memset(block_ + used_, 0, 64 - used_);
            compress(block_); used_ = 0;
        }
        std::memset(block_ + used_, 0, 56 - used_);
        for (size_t i = 0; i < 8; ++i)
            block_[63 - i] = static_cast<uint8_t>(bits >> (8 * i));
        compress(block_);
        static const char hex[] = "0123456789abcdef";
        std::string digest(64, '0');
        for (size_t i = 0; i < 8; ++i)
            for (size_t nibble = 0; nibble < 8; ++nibble)
                digest[i * 8 + nibble] = hex[(state_[i] >> (28 - nibble * 4)) & 15];
        return digest;
    }
};
} // namespace rms_suite_identity_detail

// Hash the complete JAR bytes, including ZIP metadata. Paths, filenames and
// parsed manifest values never participate in this identity.
inline std::string rms_jar_identity(const uint8_t* data, size_t size) {
    rms_suite_identity_detail::Sha256 hash;
    hash.update(data, size);
    return hash.finish();
}

// Desktop class launches use a separate, versioned domain and an unambiguous
// byte encoding: domain, u64be main length + main, u64be classpath count,
// then u64be length + bytes for each classpath entry in its original order.
inline std::string rms_class_identity(const std::string& main,
                                      const std::vector<std::string>& classpath) {
    rms_suite_identity_detail::Sha256 hash;
    static const char domain[] = "j2me-vm/rms/class-identity/v1\0";
    hash.update(reinterpret_cast<const uint8_t*>(domain), sizeof(domain) - 1);
    hash.append_string(main);
    hash.append_u64(static_cast<uint64_t>(classpath.size()));
    for (const std::string& entry : classpath) hash.append_string(entry);
    return hash.finish();
}
