#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <string_view>
#include <random>
#include <chrono>

namespace darkfire::net {

    // CRC32 - IEEE 802.3
    namespace detail {
        inline constexpr std::array<uint32_t, 256> GenerateCRC32Table() noexcept {
            std::array<uint32_t, 256> table{};
            for (uint32_t i = 0; i < 256; ++i) {
                uint32_t crc = i;
                for (uint32_t j = 0; j < 8; ++j) {
                    crc = (crc & 1) ? ((crc >> 1) ^ 0xEDB88320u) : (crc >> 1);
                }
                table[i] = crc;
            }
            return table;
        }
        inline constexpr std::array<uint32_t, 256> CRC32Table = GenerateCRC32Table();
    }

    class CRC32 {
    private:
        static constexpr const auto& Table = detail::CRC32Table;

    public:
        static constexpr uint32_t Calculate(std::span<const uint8_t> data) noexcept {
            uint32_t crc = 0xFFFFFFFFu;
            for (uint8_t byte : data) {
                crc = (crc >> 8) ^ Table[(crc ^ byte) & 0xFF];
            }
            return crc ^ 0xFFFFFFFFu;
        }

        static constexpr uint32_t Calculate(const void* data, size_t length) noexcept {
            if (!data || length == 0) return 0;
            return Calculate(std::span<const uint8_t>(static_cast<const uint8_t*>(data), length));
        }
    };

    // SHA-256 (RFC 6234 / FIPS 180-4)
    class SHA256 {
    public:
        static constexpr size_t DigestSize = 32;
        static constexpr size_t BlockSize = 64;
        using Digest = std::array<uint8_t, DigestSize>;

        SHA256() noexcept {
            Reset();
        }

        void Reset() noexcept {
            m_state[0] = 0x6a09e667;
            m_state[1] = 0xbb67ae85;
            m_state[2] = 0x3c6ef372;
            m_state[3] = 0xa54ff53a;
            m_state[4] = 0x510e527f;
            m_state[5] = 0x9b05688c;
            m_state[6] = 0x1f83d9ab;
            m_state[7] = 0x5be0cd19;
            m_count = 0;
            m_buffer.fill(0);
        }

        void Update(std::span<const uint8_t> data) noexcept {
            size_t input_len = data.size();
            size_t buffer_idx = static_cast<size_t>(m_count & 0x3F);
            m_count += input_len;

            size_t part_len = BlockSize - buffer_idx;
            size_t i = 0;

            if (input_len >= part_len) {
                std::memcpy(m_buffer.data() + buffer_idx, data.data(), part_len);
                Transform(m_buffer.data());

                for (i = part_len; i + BlockSize <= input_len; i += BlockSize) {
                    Transform(data.data() + i);
                }
                buffer_idx = 0;
            }

            if (i < input_len) {
                std::memcpy(m_buffer.data() + buffer_idx, data.data() + i, input_len - i);
            }
        }

        Digest Final() noexcept {
            uint64_t total_bits = m_count * 8;
            size_t buffer_idx = static_cast<size_t>(m_count & 0x3F);
            size_t pad_len = (buffer_idx < 56) ? (56 - buffer_idx) : (120 - buffer_idx);

            static const uint8_t padding[64] = { 0x80 };
            Update(std::span<const uint8_t>(padding, pad_len));

            // Append length in bits as 64-bit big-endian integer
            uint8_t length_bytes[8];
            for (int i = 0; i < 8; ++i) {
                length_bytes[i] = static_cast<uint8_t>((total_bits >> (56 - i * 8)) & 0xFF);
            }
            Update(std::span<const uint8_t>(length_bytes, 8));

            Digest digest{};
            for (size_t i = 0; i < 8; ++i) {
                digest[i * 4 + 0] = static_cast<uint8_t>((m_state[i] >> 24) & 0xFF);
                digest[i * 4 + 1] = static_cast<uint8_t>((m_state[i] >> 16) & 0xFF);
                digest[i * 4 + 2] = static_cast<uint8_t>((m_state[i] >> 8) & 0xFF);
                digest[i * 4 + 3] = static_cast<uint8_t>(m_state[i] & 0xFF);
            }

            Reset();
            return digest;
        }

        static Digest Hash(std::span<const uint8_t> data) noexcept {
            SHA256 sha;
            sha.Update(data);
            return sha.Final();
        }

        static Digest Hash(std::string_view str) noexcept {
            return Hash(std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(str.data()), str.size()));
        }

    private:
        static constexpr uint32_t RotR(uint32_t x, uint32_t n) noexcept {
            return (x >> n) | (x << (32 - n));
        }

        static constexpr uint32_t Ch(uint32_t x, uint32_t y, uint32_t z) noexcept {
            return (x & y) ^ (~x & z);
        }

        static constexpr uint32_t Maj(uint32_t x, uint32_t y, uint32_t z) noexcept {
            return (x & y) ^ (x & z) ^ (y & z);
        }

        static constexpr uint32_t Sigma0(uint32_t x) noexcept {
            return RotR(x, 2) ^ RotR(x, 13) ^ RotR(x, 22);
        }

        static constexpr uint32_t Sigma1(uint32_t x) noexcept {
            return RotR(x, 6) ^ RotR(x, 11) ^ RotR(x, 25);
        }

        static constexpr uint32_t Gamma0(uint32_t x) noexcept {
            return RotR(x, 7) ^ RotR(x, 18) ^ (x >> 3);
        }

        static constexpr uint32_t Gamma1(uint32_t x) noexcept {
            return RotR(x, 17) ^ RotR(x, 19) ^ (x >> 10);
        }

        void Transform(const uint8_t* block) noexcept {
            uint32_t w[64];
            for (size_t t = 0; t < 16; ++t) {
                w[t] = (static_cast<uint32_t>(block[t * 4]) << 24) |
                       (static_cast<uint32_t>(block[t * 4 + 1]) << 16) |
                       (static_cast<uint32_t>(block[t * 4 + 2]) << 8) |
                       (static_cast<uint32_t>(block[t * 4 + 3]));
            }
            for (size_t t = 16; t < 64; ++t) {
                w[t] = Gamma1(w[t - 2]) + w[t - 7] + Gamma0(w[t - 15]) + w[t - 16];
            }

            uint32_t a = m_state[0];
            uint32_t b = m_state[1];
            uint32_t c = m_state[2];
            uint32_t d = m_state[3];
            uint32_t e = m_state[4];
            uint32_t f = m_state[5];
            uint32_t g = m_state[6];
            uint32_t h = m_state[7];

            static constexpr uint32_t K[64] = {
                0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
                0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
                0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
                0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
                0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
                0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
                0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
                0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
            };

            for (size_t t = 0; t < 64; ++t) {
                uint32_t t1 = h + Sigma1(e) + Ch(e, f, g) + K[t] + w[t];
                uint32_t t2 = Sigma0(a) + Maj(a, b, c);
                h = g;
                g = f;
                f = e;
                e = d + t1;
                d = c;
                c = b;
                b = a;
                a = t1 + t2;
            }

            m_state[0] += a;
            m_state[1] += b;
            m_state[2] += c;
            m_state[3] += d;
            m_state[4] += e;
            m_state[5] += f;
            m_state[6] += g;
            m_state[7] += h;
        }

        std::array<uint32_t, 8> m_state{};
        uint64_t m_count = 0;
        std::array<uint8_t, 64> m_buffer{};
    };

    // HMAC-SHA256 (RFC 2104)
    class HMAC_SHA256 {
    public:
        using Digest = SHA256::Digest;

        static Digest Compute(std::span<const uint8_t> key, std::span<const uint8_t> data) noexcept {
            std::array<uint8_t, SHA256::BlockSize> k_pad{};

            if (key.size() > SHA256::BlockSize) {
                auto key_hash = SHA256::Hash(key);
                std::memcpy(k_pad.data(), key_hash.data(), SHA256::DigestSize);
            } else {
                std::memcpy(k_pad.data(), key.data(), key.size());
            }

            std::array<uint8_t, SHA256::BlockSize> i_key_pad = k_pad;
            std::array<uint8_t, SHA256::BlockSize> o_key_pad = k_pad;

            for (size_t i = 0; i < SHA256::BlockSize; ++i) {
                i_key_pad[i] ^= 0x36;
                o_key_pad[i] ^= 0x5C;
            }

            // Inner hash: H(i_key_pad || data)
            SHA256 inner_sha;
            inner_sha.Update(i_key_pad);
            inner_sha.Update(data);
            auto inner_hash = inner_sha.Final();

            // Outer hash: H(o_key_pad || inner_hash)
            SHA256 outer_sha;
            outer_sha.Update(o_key_pad);
            outer_sha.Update(inner_hash);
            return outer_sha.Final();
        }

        static Digest Compute(std::string_view key, std::string_view data) noexcept {
            return Compute(
                std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(key.data()), key.size()),
                std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(data.data()), data.size())
            );
        }
    };

    // Cryptographic Token & Nonce Generator
    class SecurityUtils {
    public:
        static uint64_t GenerateSecureNonce() noexcept {
            // Mix high-resolution clock with std::random_device
            std::random_device rd;
            uint64_t r1 = (static_cast<uint64_t>(rd()) << 32) | rd();
            uint64_t t = static_cast<uint64_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count());
            return r1 ^ t;
        }

        static bool ConstantTimeCompare(std::span<const uint8_t> a, std::span<const uint8_t> b) noexcept {
            if (a.size() != b.size()) return false;
            uint8_t diff = 0;
            for (size_t i = 0; i < a.size(); ++i) {
                diff |= (a[i] ^ b[i]);
            }
            return diff == 0;
        }
    };

} // namespace darkfire::net
