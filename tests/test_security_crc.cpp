#include <darkfire/darkfire.hpp>
#include <cassert>
#include <iostream>
#include <string_view>
#include <iomanip>

void TestCRC32KnownVectors() {
    std::cout << "  [TEST] CRC32 IEEE 802.3 Checksum... ";

    // Standard CRC32 check value for "123456789" is 0xCBF43926
    std::string_view testStr = "123456789";
    uint32_t crc = darkfire::net::CRC32::Calculate(testStr.data(), testStr.size());
    assert(crc == 0xCBF43926);

    std::cout << "PASSED (0x" << std::hex << crc << std::dec << ")\n";
}

void TestSHA256NISTVector() {
    std::cout << "  [TEST] SHA-256 NIST Test Vector ('abc')... ";

    // Standard hash for "abc": ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad
    auto digest = darkfire::net::SHA256::Hash("abc");

    static constexpr uint8_t expected[32] = {
        0xba, 0x78, 0x16, 0xbf, 0x8f, 0x01, 0xcf, 0xea,
        0x41, 0x41, 0x40, 0xde, 0x5d, 0xae, 0x22, 0x23,
        0xb0, 0x03, 0x61, 0xa3, 0x96, 0x17, 0x7a, 0x9c,
        0xb4, 0x10, 0xff, 0x61, 0xf2, 0x00, 0x15, 0xad
    };

    assert(std::memcmp(digest.data(), expected, 32) == 0);
    std::cout << "PASSED\n";
}

void TestHMACSHA256() {
    std::cout << "  [TEST] HMAC-SHA256 Challenge-Response... ";

    std::string_view key = "DarkFireSecretAuthKey";
    std::string_view message = "ChallengeNonce123456789";

    auto hmac1 = darkfire::net::HMAC_SHA256::Compute(key, message);
    auto hmac2 = darkfire::net::HMAC_SHA256::Compute(key, message);
    auto hmacBad = darkfire::net::HMAC_SHA256::Compute("WrongKey", message);

    assert(darkfire::net::SecurityUtils::ConstantTimeCompare(hmac1, hmac2));
    assert(!darkfire::net::SecurityUtils::ConstantTimeCompare(hmac1, hmacBad));

    std::cout << "PASSED\n";
}

void RunSecurityTests() {
    std::cout << "\n--- Running Security and CRC32 Tests ---\n";
    TestCRC32KnownVectors();
    TestSHA256NISTVector();
    TestHMACSHA256();
}
