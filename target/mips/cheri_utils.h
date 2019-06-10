/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019 Alex Richardson
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract FA8750-10-C-0237
 * ("CTSRD"), as part of the DARPA CRASH research programme.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#pragma once

#include "qemu/error-report.h"

#ifdef TARGET_CHERI
#include "tcg/tcg.h"  // for tcg_debug_assert()
#define cheri_debug_assert(cond) tcg_debug_assert(cond)

/* Don't define the functions for CHERI256 (but we need CAP_MAX_OTYPE) */
#ifdef CHERI_128
// Don't use _sbit_for_memory in cheri256 cap_register_t
#define CC256_DEFINE_FUNCTIONS 0
#define CAP_MAX_LENGTH CC128_NULL_LENGTH
#undef CC128_OLD_FORMAT // don't use the old format
#else
// Don't use cr_pesbt_xored_for_mem in cheri256 cap_register_t
#define CC128_DEFINE_FUNCTIONS 0
#define CAP_MAX_LENGTH CC256_NULL_LENGTH
#endif
/* Don't let cheri_compressed_cap define cap_register_t */
#define HAVE_CAP_REGISTER_T
#include "cheri-compressed-cap/cheri_compressed_cap.h"

#define PRINT_CAP_FMTSTR_L1 "v:%d s:%d p:%08x b:%016" PRIx64 " l:%016" PRIx64
#define PRINT_CAP_ARGS_L1(cr) (cr)->cr_tag, cap_is_sealed_with_type(cr), \
            ((((cr)->cr_uperms & CAP_UPERMS_ALL) << CAP_UPERMS_SHFT) | ((cr)->cr_perms & CAP_PERMS_ALL)), \
            cap_get_base(cr), cap_get_length(cr)
#define PRINT_CAP_FMTSTR_L2 "o:%016" PRIx64 " t:%x"
#define PRINT_CAP_ARGS_L2(cr) cap_get_offset(cr), (cr)->cr_otype


#define PRINT_CAP_FMTSTR PRINT_CAP_FMTSTR_L1 " " PRINT_CAP_FMTSTR_L2
#define PRINT_CAP_ARGS(cr) PRINT_CAP_ARGS_L1(cr), PRINT_CAP_ARGS_L2(cr)

static inline uint64_t cap_get_cursor(const cap_register_t* c) {
    return c->cr_base + c->cr_offset;
}

// Getters to allow future changes to the cap_register_t struct layout:
static inline uint64_t cap_get_base(const cap_register_t* c) {
    return c->cr_base;
}

static inline uint64_t cap_get_offset(const cap_register_t* c) {
    return c->cr_offset;
}

static inline uint64_t cap_get_length(const cap_register_t* c) {
    // TODO: should handle last byte of address space properly
    return c->_cr_length > UINT64_MAX ? UINT64_MAX : (uint64_t)c->_cr_length;
}

// The top of the capability (exclusive -- i.e., one past the end)
static inline uint64_t cap_get_top(const cap_register_t* c) {
    // TODO: should handle last byte of address space properly
    unsigned __int128 top = c->cr_base + c->_cr_length;
    return top > UINT64_MAX ? UINT64_MAX : (uint64_t)top;
}

static inline uint64_t cap_get_otype(const cap_register_t* c) {
    uint64_t result = c->cr_otype;
    if (result > CAP_MAX_REPRESENTABLE_OTYPE) {
        // raw bits loaded from memory
        assert(!c->cr_tag && "Capabilities with otype > max cannot be tagged!");
        return result;
    }
    // "sign" extend to a 64-bit number by subtracting the maximum: 2^24-1 -> 2^64-1
    return result < CAP_MAX_SEALED_OTYPE ? result : result - CAP_MAX_REPRESENTABLE_OTYPE - 1;
}

static inline bool cap_is_sealed_with_type(const cap_register_t* c) {
    // TODO: how should we treat the other reserved types? as sealed?
    // TODO: what about untagged capabilities with out-of-range otypes?
#ifndef CHERI_128
    if (c->cr_tag) {
        if (c->_sbit_for_memory)
            cheri_debug_assert(c->cr_otype <= CAP_MAX_SEALED_OTYPE);
        else
            cheri_debug_assert(c->cr_otype == CAP_OTYPE_UNSEALED || c->cr_otype == CAP_OTYPE_SENTRY);
    }
#endif
    return c->cr_otype <= CAP_MAX_SEALED_OTYPE;
}

// Check if num_bytes bytes at addr can be read using capability c
static inline bool cap_is_in_bounds(const cap_register_t* c, uint64_t addr, uint64_t num_bytes) {
    if (addr < cap_get_base(c)) {
        return false;
    }
    // Use __builtin_add_overflow to avoid wrapping around the end of the addres space
    uint64_t access_end_addr = 0;
    if (__builtin_add_overflow(addr, num_bytes, &access_end_addr)) {
        warn_report("Found capability access that wraps around: 0x%" PRIx64
                    " + %" PRId64 ". Authorizing cap: " PRINT_CAP_FMTSTR,
                    addr, num_bytes, PRINT_CAP_ARGS(c));
        return false;
    }
    if (access_end_addr > cap_get_top(c)) {
        return false;
    }
    return true;
}

static inline bool cap_is_unsealed(const cap_register_t* c) {
    // TODO: how should we treat the other reserved types? as sealed?
    // TODO: what about untagged capabilities with out-of-range otypes?
    _Static_assert(CAP_MAX_REPRESENTABLE_OTYPE == CAP_OTYPE_UNSEALED, "");
#ifndef CHERI_128
    if (c->cr_tag) {
        if (c->_sbit_for_memory)
            cheri_debug_assert(c->cr_otype <= CAP_MAX_SEALED_OTYPE);
        else
            cheri_debug_assert(c->cr_otype == CAP_OTYPE_UNSEALED || c->cr_otype == CAP_OTYPE_SENTRY);
    }
#endif
    return c->cr_otype >= CAP_OTYPE_UNSEALED;
}

static inline void cap_set_sealed(cap_register_t* c, uint32_t type) {
    assert(c->cr_tag);
    assert(c->cr_otype == CAP_OTYPE_UNSEALED && "should not use this on caps with reserved otypes");
    assert(type <= CAP_MAX_SEALED_OTYPE);
    _Static_assert(CAP_MAX_SEALED_OTYPE < CAP_OTYPE_UNSEALED, "");
    c->cr_otype = type;
#ifndef CHERI_128
    assert(c->_sbit_for_memory == false);
    c->_sbit_for_memory = true;
#endif
}

static inline void cap_set_unsealed(cap_register_t* c) {
    assert(c->cr_tag);
    assert(cap_is_sealed_with_type(c));
    assert(c->cr_otype <= CAP_MAX_SEALED_OTYPE && "should not use this to unsealed reserved types");
    c->cr_otype = CAP_OTYPE_UNSEALED;
#ifndef CHERI_128
    assert(c->_sbit_for_memory == true);
    c->_sbit_for_memory = false;
#endif
}

static inline bool cap_is_sealed_entry(const cap_register_t* c) {
    return c->cr_otype == CAP_OTYPE_SENTRY;
}

static inline void cap_unseal_entry(cap_register_t* c) {
    assert(c->cr_tag && cap_is_sealed_entry(c) && "Should only be used with sentry capabilities");
    c->cr_otype = CAP_OTYPE_UNSEALED;
}

static inline void cap_make_sealed_entry(cap_register_t* c) {
    assert(c->cr_tag && cap_is_unsealed(c) && "Should only be used with unsealed capabilities");
    c->cr_otype = CAP_OTYPE_SENTRY;
}


static inline cap_register_t *null_capability(cap_register_t *cp)
{
    memset(cp, 0, sizeof(*cp)); // Set everything to zero including padding
    // For CHERI128 max length is 1 << 64 (__int128) for CHERI256 UINT64_MAX
    cp->_cr_length = CAP_MAX_LENGTH;
    cp->cr_otype = CAP_OTYPE_UNSEALED; // and otype should be unsealed
    return cp;
}

static inline bool is_null_capability(const cap_register_t *cp)
{
    cap_register_t null;
    // This also compares padding but it should always be NULL assuming
    // null_capability() was used
    return memcmp(null_capability(&null), cp, sizeof(cap_register_t)) == 0;
}

// Clear the tag bit and update the cursor:
static inline cap_register_t *nullify_epcc(uint64_t x, cap_register_t *cr)
{
    cr->cr_tag = 0;
    cr->cr_base = 0;
    cr->_cr_length = CAP_MAX_LENGTH;
    cr->cr_offset = x;
    return cr;
}

/*
 * Convert 64-bit integer into a capability that holds the integer in
 * its offset field.
 *
 *       cap.base = 0, cap.tag = false, cap.offset = x
 *
 * The contents of other fields of int to cap depends on the capability
 * compression scheme in use (e.g. 256-bit capabilities or 128-bit
 * compressed capabilities). In particular, with 128-bit compressed
 * capabilities, length is not always zero. The length of a capability
 * created via int to cap is not semantically meaningful, and programs
 * should not rely on it having any particular value.
 */
static inline const cap_register_t*
int_to_cap(uint64_t x, cap_register_t *cr)
{

    (void)null_capability(cr);
    cr->cr_offset = x;
    return cr;
}

static inline void set_max_perms_capability(cap_register_t *crp, uint64_t offset)
{
    crp->cr_tag = 1;
    crp->cr_perms = CAP_PERMS_ALL;
    crp->cr_uperms = CAP_UPERMS_ALL;
    // crp->cr_cursor = 0UL;
    crp->cr_offset = offset;
    crp->cr_base = 0UL;
    // For CHERI128 max length is 1 << 64 (__int128) for CHERI256 UINT64_MAX
    crp->_cr_length = CAP_MAX_LENGTH;
    crp->cr_otype = CAP_OTYPE_UNSEALED;
#ifdef CHERI_128
    crp->cr_pesbt_xored_for_mem = 0UL;
#else
    crp->_sbit_for_memory = false;
#endif
}

#if defined(CHERI_128) && !defined(CHERI_MAGIC128)

static inline bool
is_representable_cap(const cap_register_t* cap, uint64_t new_offset)
{
    return cc128_is_representable(cap_is_sealed_with_type(cap), cap_get_base(cap), cap->_cr_length, cap_get_offset(cap), new_offset);
}

static inline bool
is_representable_cap_when_sealed(const cap_register_t* cap, uint64_t new_offset)
{
    assert(cap_is_unsealed(cap));
    return cc128_is_representable(true, cap_get_base(cap), cap->_cr_length, cap_get_offset(cap), new_offset);
}

#else
static inline bool
is_representable_cap(const cap_register_t* cap, uint64_t new_offset)
{
    return true;
}

static inline bool
is_representable_cap_when_sealed(const cap_register_t* cap, uint64_t new_offset)
{
    return true;
}
#endif

#endif /* TARGET_CHERI */

