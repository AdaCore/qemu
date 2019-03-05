#include "../cheri_compressed_cap.h"
#include <cinttypes>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include "test_util.h"

static void test_compressed_null_cap_is_zero() {
    {
        cap_register_t null_cap;
        memset(&null_cap, 0, sizeof(null_cap));
        null_cap.cr_otype = CC128_OTYPE_UNSEALED;
        null_cap._cr_length = CC128_NULL_LENGTH;
        auto pesbt = compress_128cap(&null_cap);
        check(pesbt, (uint64_t)0, "compressing NULL should result in zero pesbt");
        cap_register_t decompressed;
        memset(&decompressed, 'b', sizeof(decompressed));
        decompress_128cap(pesbt, 0, &decompressed);
        CHECK_FIELD(decompressed, base, 0);
        CHECK_FIELD(decompressed, offset, 0);
        CHECK_FIELD(decompressed, uperms, 0);
        CHECK_FIELD(decompressed, perms, 0);
        CHECK_FIELD(decompressed, pesbt, 0); // loaded pesbt xored with mask
        CHECK_FIELD_RAW(decompressed._cr_length, CC128_NULL_LENGTH);
        CHECK_FIELD(decompressed, otype, CC128_OTYPE_UNSEALED);
    }
    {
        // Same for CHERI256
        cap_register_t null_cap;
        memset(&null_cap, 0, sizeof(null_cap));
        null_cap.cr_otype = CC256_OTYPE_UNSEALED;
        null_cap._cr_length = CC256_NULL_LENGTH;
        inmemory_chericap256 buffer;
        compress_256cap(&buffer, &null_cap);
        check(buffer.u64s[0], (uint64_t)0, "compressing NULL should result in zero[0]");
        check(buffer.u64s[1], (uint64_t)0, "compressing NULL should result in zero[1]");
        check(buffer.u64s[2], (uint64_t)0, "compressing NULL should result in zero[2]");
        check(buffer.u64s[3], (uint64_t)0, "compressing NULL should result in zero[3]");

        // compressing this result should give 0 again
        cap_register_t decompressed;
        memset(&decompressed, 'b', sizeof(decompressed));
        decompress_256cap(buffer, &decompressed);
        CHECK_FIELD(decompressed, base, 0);
        CHECK_FIELD(decompressed, offset, 0);
        CHECK_FIELD(decompressed, uperms, 0);
        CHECK_FIELD(decompressed, perms, 0);
        CHECK_FIELD_RAW(decompressed._cr_length, CC256_NULL_LENGTH);
        CHECK_FIELD(decompressed, otype, CC256_OTYPE_UNSEALED);
    }
}

static void test_decompress_zero_is_null_cap() {
    cap_register_t result;
    memset(&result, 'a', sizeof(result));
    decompress_128cap(0, 0, &result);
    fprintf(stderr, "Decompressed 128-bit NULL cap:\n");
    dump_cap_fields(result);
    fprintf(stderr, "\n");
    CHECK_FIELD(result, base, 0);
    CHECK_FIELD(result, offset, 0);
    CHECK_FIELD(result, uperms, 0);
    CHECK_FIELD(result, perms, 0);
    CHECK_FIELD(result, pesbt, 0); // loaded pesbt xored with mask
    CHECK_FIELD_RAW(result._cr_length, CC128_NULL_LENGTH);
    CHECK_FIELD(result, otype, CC128_OTYPE_UNSEALED);

    // Same for CHERI256
    inmemory_chericap256 buffer;
    memset(&buffer.bytes, 0, sizeof(buffer));
    memset(&result, 'a', sizeof(result));
    decompress_256cap(buffer, &result);
    fprintf(stderr, "Decompressed 256-bit NULL cap:\n");
    dump_cap_fields(result);
    fprintf(stderr, "\n");
    CHECK_FIELD(result, base, 0);
    CHECK_FIELD(result, offset, 0);
    CHECK_FIELD(result, uperms, 0);
    CHECK_FIELD(result, perms, 0);
    CHECK_FIELD_RAW(result._cr_length, CC256_NULL_LENGTH);
    CHECK_FIELD(result, otype, CC256_OTYPE_UNSEALED);
}

#ifdef CC128_OLD_FORMAT
static void test1() {
    auto result = decompress_representable(0xffae000000000000, 0x9000000040001650);
    CHECK_FIELD(result, base, 0);
    CHECK_FIELD(result, offset, 0x9000000040001650);
    CHECK_FIELD(result, uperms, 0xf);
    CHECK_FIELD(result, perms, 0xfae);
    CHECK_FIELD_RAW(result._cr_length, CAP_MAX_ADDRESS_PLUS_ONE);
    CHECK_FIELD(result, otype, CC128_OTYPE_UNSEALED);
}

static void test2() {
    auto result = decompress_representable(0xfffa200b1f001633, 0x9000000040001636);
    CHECK_FIELD(result, base, 0x9000000040001636);
    CHECK_FIELD(result, offset, 0);
    CHECK_FIELD(result, uperms, 0xf);
    CHECK_FIELD(result, perms, 0xffa);
    CHECK_FIELD_RAW(result._cr_length, 6);
    CHECK_FIELD(result, otype, CC128_OTYPE_UNSEALED);
}

static void test3() {
    auto result = decompress_representable(0xfffa201a19003035, 0x9000000040003296);
    CHECK_FIELD(result, base, 0x9000000040003030);
    CHECK_FIELD(result, offset, 0x266);
    CHECK_FIELD(result, uperms, 0xf);
    CHECK_FIELD(result, perms, 0xffa);
    CHECK_FIELD_RAW(result._cr_length, 0x400);
    CHECK_FIELD(result, otype, CC128_OTYPE_UNSEALED);
}

// 4 and 5 are from test_cp2_cincoffset_rep_underflow.log
static void test4() {
    // 0x9000000040000f6c:  cincoffset c2,c1,t4
    //    Write C02|v:1 s:0 p:00007fff b:0000000000000000 l:0000000130000000
    //             |o:0000000000001010 t:0
    // 0x9000000040000f70:  csc        c2,a1,0(c18)
    //    Cap Memory Write [9000000040001030] = v:1 PESBT:fffe030000000004 Cursor:0000000000001010
    auto result = decompress_representable(0xfffe030000000004, 0x1010);
    CHECK_FIELD(result, base, 0);
    CHECK_FIELD(result, offset, 0x1010);
    CHECK_FIELD(result, uperms, 0xf);
    CHECK_FIELD(result, perms, 0xffe);
    CHECK_FIELD(result, length, 0x130000000);
    CHECK_FIELD_RAW(result._cr_length, 0x130000000);
    CHECK_FIELD(result, otype, CC128_OTYPE_UNSEALED);
}

static void test5() {
    // 0x9000000040000f88:  cincoffset c2,c1,t4
    //    Write C02|v:1 s:0 p:00007fff b:0000000000000000 l:0000000130000000
    //             |o:0000000000000ff0 t:0
    // 0x9000000040000f8c:  csc        c2,a1,64(c18)
    //    Cap Memory Write [9000000040001070] = v:1 PESBT:fffe030000000004 Cursor:0000000000000ff0
    auto result = decompress_representable(0xfffe030000000004, 0xff0);
    CHECK_FIELD(result, base, 0);
    CHECK_FIELD(result, offset, 0xff0);
    CHECK_FIELD(result, uperms, 0xf);
    CHECK_FIELD(result, perms, 0xffe);
    CHECK_FIELD_RAW(result._cr_length, 0x130000000);
    CHECK_FIELD(result, otype, CC128_OTYPE_UNSEALED);
}
#endif

int main() {
    fprintf(stderr, "NULL PESBT= %016" PRIx64 "\n", (uint64_t)CC128_NULL_PESBT);
    test_compressed_null_cap_is_zero();
    test_decompress_zero_is_null_cap();
#ifdef CC128_OLD_FORMAT
    test1();
    test2();
    test3();
    test4();
    test5();
#endif
    if (!failed)
        printf("\nAll tests passed!\n");
    return failed ? 1 : 0;
}