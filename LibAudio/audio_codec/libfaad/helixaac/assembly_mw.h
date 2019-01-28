/* ***** BEGIN LICENSE BLOCK *****
 * Source last modified: $Id: assembly.h,v 1.7 2005/11/10 00:04:40 margotm Exp $
 *
 * Portions Copyright (c) 1995-2005 RealNetworks, Inc. All Rights Reserved.
 *
 * The contents of this file, and the files included with this file,
 * are subject to the current version of the RealNetworks Public
 * Source License (the "RPSL") available at
 * http://www.helixcommunity.org/content/rpsl unless you have licensed
 * the file under the current version of the RealNetworks Community
 * Source License (the "RCSL") available at
 * http://www.helixcommunity.org/content/rcsl, in which case the RCSL
 * will apply. You may also obtain the license terms directly from
 * RealNetworks.  You may not use this file except in compliance with
 * the RPSL or, if you have a valid RCSL with RealNetworks applicable
 * to this file, the RCSL.  Please see the applicable RPSL or RCSL for
 * the rights, obligations and limitations governing use of the
 * contents of the file.
 *
 * This file is part of the Helix DNA Technology. RealNetworks is the
 * developer of the Original Code and owns the copyrights in the
 * portions it created.
 *
 * This file, and the files included with this file, is distributed
 * and made available on an 'AS IS' basis, WITHOUT WARRANTY OF ANY
 * KIND, EITHER EXPRESS OR IMPLIED, AND REALNETWORKS HEREBY DISCLAIMS
 * ALL SUCH WARRANTIES, INCLUDING WITHOUT LIMITATION, ANY WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, QUIET
 * ENJOYMENT OR NON-INFRINGEMENT.
 *
 * Technology Compatibility Kit Test Suite(s) Location:
 *    http://www.helixcommunity.org/content/tck
 *
 * Contributor(s):
 *
 * ***** END LICENSE BLOCK ***** */

/**************************************************************************************
 * Fixed-point HE-AAC decoder
 * Jon Recker (jrecker@real.com)
 * February 2005
 *
 * assembly.h - inline assembly language functions and prototypes
 *
 * MULSHIFT32(x, y)         signed multiply of two 32-bit integers (x and y),
 *                            returns top 32-bits of 64-bit result
 * CLIPTOSHORT(x)           convert 32-bit integer to 16-bit short,
 *                            clipping to [-32768, 32767]
 * FASTABS(x)               branchless absolute value of signed integer x
 * CLZ(x)                   count leading zeros on signed integer x
 * MADD64(sum64, x, y)      64-bit multiply accumulate: sum64 += (x*y)
 **************************************************************************************/

#ifndef _ASSEMBLY_H
#define _ASSEMBLY_H

//#define _Inline inline
#define _ARC32
/* toolchain:           MSFT Visual C++
 * target architecture: x86
 */
#if (defined (_WIN32) && !defined (_WIN32_WCE)) || (defined (__WINS__) && defined (_SYMBIAN)) || (defined (WINCE_EMULATOR)) || (defined (_OPENWAVE_SIMULATOR))

#pragma warning( disable : 4035 )   /* complains about inline asm not returning a value */

static __inline int MULSHIFT32(int x, int y)
{
    __asm {
        mov     eax, x
        imul    y
        mov     eax, edx
    }
}

static __inline short CLIPTOSHORT(int x)
{
    int sign;

    /* clip to [-32768, 32767] */
    sign = x >> 31;
    if (sign != (x >> 15)) {
        x = sign ^((1 << 15) - 1);
    }

    return (short)x;
}

static __inline int FASTABS(int x)
{
    int sign;

    sign = x >> (sizeof(int) * 8 - 1);
    x ^= sign;
    x -= sign;

    return x;
}

static __inline int CLZ(int x)
{
    int numZeros;

    if (!x) {
        return 32;
    }

    /* count leading zeros with binary search */
    numZeros = 1;
    if (!((unsigned int)x >> 16))   {
        numZeros += 16;
        x <<= 16;
    }
    if (!((unsigned int)x >> 24))   {
        numZeros +=  8;
        x <<=  8;
    }
    if (!((unsigned int)x >> 28))   {
        numZeros +=  4;
        x <<=  4;
    }
    if (!((unsigned int)x >> 30))   {
        numZeros +=  2;
        x <<=  2;
    }

    numZeros -= ((unsigned int)x >> 31);

    return numZeros;
}

#ifdef __CW32__
typedef long long Word64;
#else
typedef __int64 Word64;
#endif

typedef union _U64 {
    Word64 w64;
    struct {
        /* x86 = little endian */
        unsigned int lo32;
        signed int   hi32;
    } r;
} U64;

/* returns 64-bit value in [edx:eax] */
static __inline Word64 madd64(Word64 sum64, int x, int y)
{
#if (defined (_SYMBIAN_61_) || defined (_SYMBIAN_70_)) && defined (__WINS__) && !defined (__CW32__)
    /* Workaround for the Symbian emulator because of non existing longlong.lib and
     * hence __allmul not defined. */
    __asm {
        mov     eax, x
        imul    y
        add     dword ptr sum64, eax
        adc     dword ptr sum64 + 4, edx
    }
#else
    sum64 += (Word64)x * (Word64)y;

    /* equivalent to return (sum + ((__int64)x * y)); */
#endif
}

#define SET_ZERO(x) x=0
#define MADD64(sum64, x, y) sum64=madd64(sum64, x, y)
#define ADD64(x64, y64) x64 += y64;

/* toolchain:           MSFT Embedded Visual C++
 * target architecture: ARM v.4 and above (require 'M' type processor for 32x32->64 multiplier)
 */
#elif defined (_WIN32) && defined (_WIN32_WCE) && defined (ARM)

static __inline short CLIPTOSHORT(int x)
{
    int sign;

    /* clip to [-32768, 32767] */
    sign = x >> 31;
    if (sign != (x >> 15)) {
        x = sign ^((1 << 15) - 1);
    }

    return (short)x;
}

static __inline int FASTABS(int x)
{
    int sign;

    sign = x >> (sizeof(int) * 8 - 1);
    x ^= sign;
    x -= sign;

    return x;
}

static __inline int CLZ(int x)
{
    int numZeros;

    if (!x) {
        return 32;
    }

    /* count leading zeros with binary search (function should be 17 ARM instructions total) */
    numZeros = 1;
    if (!((unsigned int)x >> 16))   {
        numZeros += 16;
        x <<= 16;
    }
    if (!((unsigned int)x >> 24))   {
        numZeros +=  8;
        x <<=  8;
    }
    if (!((unsigned int)x >> 28))   {
        numZeros +=  4;
        x <<=  4;
    }
    if (!((unsigned int)x >> 30))   {
        numZeros +=  2;
        x <<=  2;
    }

    numZeros -= ((unsigned int)x >> 31);

    return numZeros;
}

/* implemented in asmfunc.s */
#ifdef __cplusplus
extern "C" {
#endif

    typedef __int64 Word64;

    typedef union _U64 {
        Word64 w64;
        struct {
            /* ARM WinCE = little endian */
            unsigned int lo32;
            signed int   hi32;
        } r;
    } U64;

    /* manual name mangling for just this platform (must match labels in .s file) */
#define MULSHIFT32  raac_MULSHIFT32
#define MADD64      raac_MADD64

    int MULSHIFT32(int x, int y);
    Word64 MADD64(Word64 sum64, int x, int y);

#ifdef __cplusplus
}
#endif

/* toolchain:           ARM ADS or RealView
 * target architecture: ARM v.4 and above (requires 'M' type processor for 32x32->64 multiplier)
 */
#elif (defined (__arm) && defined (__ARMCC_VERSION)) || (defined(HELIX_CONFIG_SYMBIAN_GENERATE_MMP) && !defined(__GCCE__))

static __inline int MULSHIFT32(int x, int y)
{
    /* rules for smull RdLo, RdHi, Rm, Rs:
     *   RdHi != Rm
     *   RdLo != Rm
     *   RdHi != RdLo
     */
    int zlow;
    __asm {
        smull zlow, y, x, y
    }

    return y;
}

static __inline short CLIPTOSHORT(int x)
{
    int sign;

    /* clip to [-32768, 32767] */
    sign = x >> 31;
    if (sign != (x >> 15)) {
        x = sign ^((1 << 15) - 1);
    }

    return (short)x;
}

static __inline int FASTABS(int x)
{
    int sign;

    sign = x >> (sizeof(int) * 8 - 1);
    x ^= sign;
    x -= sign;

    return x;
}

static __inline int CLZ(int x)
{
    int numZeros;

    if (!x) {
        return 32;
    }

    /* count leading zeros with binary search (function should be 17 ARM instructions total) */
    numZeros = 1;
    if (!((unsigned int)x >> 16))   {
        numZeros += 16;
        x <<= 16;
    }
    if (!((unsigned int)x >> 24))   {
        numZeros +=  8;
        x <<=  8;
    }
    if (!((unsigned int)x >> 28))   {
        numZeros +=  4;
        x <<=  4;
    }
    if (!((unsigned int)x >> 30))   {
        numZeros +=  2;
        x <<=  2;
    }

    numZeros -= ((unsigned int)x >> 31);

    return numZeros;

    /* ARM code would look like this, but do NOT use inline asm in ADS for this,
       because you can't safely use the status register flags intermixed with C code

        __asm {
            mov     numZeros, #1
            tst     x, 0xffff0000
            addeq   numZeros, numZeros, #16
            moveq   x, x, lsl #16
            tst     x, 0xff000000
            addeq   numZeros, numZeros, #8
            moveq   x, x, lsl #8
            tst     x, 0xf0000000
            addeq   numZeros, numZeros, #4
            moveq   x, x, lsl #4
            tst     x, 0xc0000000
            addeq   numZeros, numZeros, #2
            moveq   x, x, lsl #2
            sub     numZeros, numZeros, x, lsr #31
        }
    */
    /* reference:
        numZeros = 0;
        while (!(x & 0x80000000)) {
            numZeros++;
            x <<= 1;
        }
    */
}

typedef __int64 Word64;

typedef union _U64 {
    Word64 w64;
    struct {
        /* ARM ADS = little endian */
        unsigned int lo32;
        signed int   hi32;
    } r;
} U64;

static __inline Word64 MADD64(Word64 sum64, int x, int y)
{
    U64 u;
    u.w64 = sum64;

    __asm {
        smlal u.r.lo32, u.r.hi32, x, y
    }

    return u.w64;
}

/* toolchain:           ARM gcc
 * target architecture: ARM v.4 and above (requires 'M' type processor for 32x32->64 multiplier)
 */
#elif defined(__GNUC__) && defined(__arm__)

static __inline__ int MULSHIFT32(int x, int y)
{
    int zlow;
    __asm__ volatile("smull %0,%1,%2,%3" : "=&r"(zlow), "=r"(y) : "r"(x), "1"(y) : "cc");
    return y;
}

static __inline short CLIPTOSHORT(int x)
{
    int sign;

    /* clip to [-32768, 32767] */
    sign = x >> 31;
    if (sign != (x >> 15)) {
        x = sign ^((1 << 15) - 1);
    }

    return (short)x;
}

static __inline int FASTABS(int x)
{
    int sign;

    sign = x >> (sizeof(int) * 8 - 1);
    x ^= sign;
    x -= sign;

    return x;
}

static __inline int CLZ(int x)
{
    int numZeros;

    if (!x) {
        return (sizeof(int) * 8);
    }

    numZeros = 0;
    while (!(x & 0x80000000)) {
        numZeros++;
        x <<= 1;
    }

    return numZeros;
}

typedef long long Word64;

typedef union _U64 {
    Word64 w64;
    struct {
        /* ARM ADS = little endian */
        unsigned int lo32;
        signed int   hi32;
    } r;
} U64;

static __inline Word64 MADD64(Word64 sum64, int x, int y)
{
    U64 u;
    u.w64 = sum64;

    __asm__ volatile("smlal %0,%1,%2,%3" : "+&r"(u.r.lo32), "+&r"(u.r.hi32) : "r"(x), "r"(y) : "cc");

    return u.w64;
}

/* toolchain:           x86 gcc
 * target architecture: x86
 */
#elif defined(__GNUC__) && (defined(__i386__) || defined(__amd64__)) || (defined (_SOLARIS) && !defined (__GNUC__) && defined(_SOLARISX86))

typedef long long Word64;

static __inline__ int MULSHIFT32(int x, int y)
{
    int z;

    z = (Word64)x * (Word64)y >> 32;

    return z;
}

static __inline short CLIPTOSHORT(int x)
{
    int sign;

    /* clip to [-32768, 32767] */
    sign = x >> 31;
    if (sign != (x >> 15)) {
        x = sign ^((1 << 15) - 1);
    }

    return (short)x;
}

static __inline int FASTABS(int x)
{
    int sign;

    sign = x >> (sizeof(int) * 8 - 1);
    x ^= sign;
    x -= sign;

    return x;
}

static __inline int CLZ(int x)
{
    int numZeros;

    if (!x) {
        return 32;
    }

    /* count leading zeros with binary search (function should be 17 ARM instructions total) */
    numZeros = 1;
    if (!((unsigned int)x >> 16))   {
        numZeros += 16;
        x <<= 16;
    }
    if (!((unsigned int)x >> 24))   {
        numZeros +=  8;
        x <<=  8;
    }
    if (!((unsigned int)x >> 28))   {
        numZeros +=  4;
        x <<=  4;
    }
    if (!((unsigned int)x >> 30))   {
        numZeros +=  2;
        x <<=  2;
    }

    numZeros -= ((unsigned int)x >> 31);

    return numZeros;
}

typedef union _U64 {
    Word64 w64;
    struct {
        /* x86 = little endian */
        unsigned int lo32;
        signed int   hi32;
    } r;
} U64;

static __inline Word64 MADD64(Word64 sum64, int x, int y)
{
    sum64 += (Word64)x * (Word64)y;

    return sum64;
}

#elif defined(__GNUC__) && (defined(__powerpc__) || defined(__POWERPC__)) || (defined (_SOLARIS) && !defined (__GNUC__) && !defined (_SOLARISX86))

typedef long long Word64;

static __inline__ int MULSHIFT32(int x, int y)
{
    int z;

    z = (Word64)x * (Word64)y >> 32;

    return z;
}

static __inline short CLIPTOSHORT(int x)
{
    int sign;

    /* clip to [-32768, 32767] */
    sign = x >> 31;
    if (sign != (x >> 15)) {
        x = sign ^((1 << 15) - 1);
    }

    return (short)x;
}

static __inline int FASTABS(int x)
{
    int sign;

    sign = x >> (sizeof(int) * 8 - 1);
    x ^= sign;
    x -= sign;

    return x;
}

static __inline int CLZ(int x)
{
    int numZeros;

    if (!x) {
        return 32;
    }

    /* count leading zeros with binary search (function should be 17 ARM instructions total) */
    numZeros = 1;
    if (!((unsigned int)x >> 16))   {
        numZeros += 16;
        x <<= 16;
    }
    if (!((unsigned int)x >> 24))   {
        numZeros +=  8;
        x <<=  8;
    }
    if (!((unsigned int)x >> 28))   {
        numZeros +=  4;
        x <<=  4;
    }
    if (!((unsigned int)x >> 30))   {
        numZeros +=  2;
        x <<=  2;
    }

    numZeros -= ((unsigned int)x >> 31);

    return numZeros;
}

typedef union _U64 {
    Word64 w64;
    struct {
        /* PowerPC = big endian */
        signed int   hi32;
        unsigned int lo32;
    } r;
} U64;

static __inline Word64 MADD64(Word64 sum64, int x, int y)
{
    sum64 += (Word64)x * (Word64)y;

    return sum64;
}

#elif defined(_ARC32)

_Asm _Inline int MULSHIFT32(int x, int y)
{
    % reg x, y
    mullw 0, x, y
    machlw % r0, x, y
    % error
}

_Asm _Inline short CLIPTOSHORT(int x)
{
    % reg x
    min % r0, x, 0x7fff
    max % r0, % r0, -0x8000
    % error
}

_Asm _Inline int FASTABS(int x)
{
    % reg x
    abs % r0, x
    % error
}

_Asm _Inline int CLZ(int x)
{
    /* assume x>0, if x<0 should return 0 */
    % reg x;
    norm % r0, x
    add % r0, % r0, 1
    % error
}
#endif
typedef struct {
    unsigned int lo32;
    signed int hi32;
} Word64;

typedef union _U64 {
    Word64 w64;
    struct {
        unsigned int lo32;
        signed int   hi32;
    } r;
} U64;

_Asm _Inline unsigned add64_lo(unsigned int xlo, unsigned int ylo)
{
    % reg xlo, ylo;
    add.f % r0, xlo, ylo
    % error
}

_Asm _Inline int add64_hi(unsigned int xhi, unsigned int yhi)
{
    % reg xhi, yhi;
    adc % r0, xhi, yhi
    % error
}

_Asm _Inline unsigned madd64_lo(unsigned lo, int a, int b)
{
    % reg lo, a, b;
    mpy % r0, a, b
    add.f % r0, lo, % r0
    % error
}


_Asm _Inline int madd64_hi(int hi, int a, int b)
{
    % reg hi, a, b;
    mpyh % r0, a, b
    adc % r0, hi, % r0
    % error
}



_Asm _Inline void madd64(int a, int b)
{
    % reg  a, b;
    mulhlw 0, a, b
    maclw  0, a, b
    % error
}

_Asm _Inline int madd64hi(int hi)
{
    % reg hi
    //mov   %r0, %acc2
    adc % r0, hi, % acc1
    % error
}

_Asm _Inline int madd64lo(int lo)
{
    % reg lo
    //mov   %r0, %acc1
    add.f % r0, lo, % acc2
    % error
}



#define SET_ZERO(x) x.lo32 = x.hi32 = 0

#define MADD64(w64, a, b) madd64(a, b); w64.lo32 = madd64lo(w64.lo32);  w64.hi32 = madd64hi(w64.hi32);

#define ADD64(x64, y64) x64.lo32 = add64_lo(x64.lo32,y64.lo32); x64.hi32 = add64_hi(x64.hi32,y64.hi32);

#endif /* _ASSEMBLY_H */
