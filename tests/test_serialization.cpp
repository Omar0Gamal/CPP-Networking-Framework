#include <darkfire/darkfire.hpp>
#include <cassert>
#include <iostream>
#include <vector>
#include <string>

enum class TestWeaponType : uint32_t {
    Pistol = 1,
    Rifle = 2,
    Plasma = 3
};

struct PlayerTransform {
    float x;
    float y;
    float z;
    uint32_t entityId;

    bool operator==(const PlayerTransform& o) const noexcept {
        return x == o.x && y == o.y && z == o.z && entityId == o.entityId;
    }
};
DARKFIRE_DECLARE_TRIVIALLY_SERIALIZABLE(PlayerTransform);

void TestSerializationPrimitives() {
    std::cout << "  [TEST] Serialization Primitives & Forward FIFO Order... ";

    darkfire::net::ByteWriter writer;
    uint8_t u8 = 42;
    uint16_t u16 = 12345;
    uint32_t u32 = 987654321;
    uint64_t u64 = 0xDEADBEEFCAFEBABEull;
    int16_t s16 = -456;
    float f = 3.14159f;
    double d = 2.718281828459;
    bool b = true;
    std::string str = "Hello DarkFire 2.0!";
    TestWeaponType weapon = TestWeaponType::Plasma;

    // Forward streaming insertion
    writer << u8 << u16 << u32 << u64 << s16 << f << d << b << str << weapon;

    // Forward reading
    darkfire::net::ByteReader reader(writer.Span());
    uint8_t r_u8{};
    uint16_t r_u16{};
    uint32_t r_u32{};
    uint64_t r_u64{};
    int16_t r_s16{};
    float r_f{};
    double r_d{};
    bool r_b{};
    std::string r_str;
    TestWeaponType r_weapon{};

    reader >> r_u8 >> r_u16 >> r_u32 >> r_u64 >> r_s16 >> r_f >> r_d >> r_b >> r_str >> r_weapon;

    assert(reader.IsOk());
    assert(r_u8 == u8);
    assert(r_u16 == u16);
    assert(r_u32 == u32);
    assert(r_u64 == u64);
    assert(r_s16 == s16);
    assert(std::abs(r_f - f) < 0.0001f);
    assert(std::abs(r_d - d) < 0.0000001);
    assert(r_b == b);
    assert(r_str == str);
    assert(r_weapon == weapon);
    assert(reader.Remaining() == 0);

    std::cout << "PASSED\n";
}

void TestVectorsAndStructs() {
    std::cout << "  [TEST] Vectors and Trivially Serializable Structs... ";

    darkfire::net::ByteWriter writer;
    std::vector<int32_t> numbers = { 10, -20, 30, -40, 50 };
    PlayerTransform transform{ 100.5f, 200.25f, -50.0f, 42 };

    writer << numbers << transform;

    darkfire::net::ByteReader reader(writer.Span());
    std::vector<int32_t> r_numbers;
    PlayerTransform r_transform{};

    reader >> r_numbers >> r_transform;

    assert(reader.IsOk());
    assert(r_numbers == numbers);
    assert(r_transform == transform);
    assert(reader.Remaining() == 0);

    std::cout << "PASSED\n";
}

void TestZeroCopyViews() {
    std::cout << "  [TEST] Zero-Copy StringView and Slices... ";

    darkfire::net::ByteWriter writer;
    std::string testStr = "Zero-copy high performance networking view";
    writer.WriteString(testStr);

    darkfire::net::ByteReader reader(writer.Span());
    auto svRes = reader.ReadStringView();
    assert(svRes.HasValue());
    assert(*svRes == testStr);
    // Ensure pointer is within writer's buffer!
    assert(svRes->data() >= reinterpret_cast<const char*>(writer.Data()) &&
           svRes->data() < reinterpret_cast<const char*>(writer.Data() + writer.Size()));

    std::cout << "PASSED\n";
}

void TestBoundsCheckingSafety() {
    std::cout << "  [TEST] Strict Bounds Checking & Underflow Defense... ";

    uint8_t dummy[2] = { 0x01, 0x02 };
    darkfire::net::ByteReader reader(dummy, sizeof(dummy));

    // Trying to read 4 bytes from a 2-byte buffer
    auto res32 = reader.ReadUInt32();
    assert(!res32.HasValue());
    assert(res32.Error() == darkfire::net::NetErrorCode::BufferUnderflow);
    assert(reader.HasError());
    assert(reader.LastError() == darkfire::net::NetErrorCode::BufferUnderflow);

    // Operator>> on underflow must not crash or corrupt memory
    uint64_t hugeVal = 0xAAAAAAAAAAAAAAAAull;
    reader >> hugeVal;
    assert(reader.HasError());
    assert(hugeVal == 0); // Safely defaulted

    std::cout << "PASSED\n";
}

void RunSerializationTests() {
    std::cout << "\n--- Running Serialization Engine Tests ---\n";
    TestSerializationPrimitives();
    TestVectorsAndStructs();
    TestZeroCopyViews();
    TestBoundsCheckingSafety();
}
