/*
 *  Copyright (C) 2002-2021  The DOSBox Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef DOSBOX_FPU_H
#define DOSBOX_FPU_H

#include "logging.h"
#include "mmx.h"

void FPU_ESC0_Normal(Bitu rm);
void FPU_ESC0_EA(Bitu rm,PhysPt addr);
void FPU_ESC1_Normal(Bitu rm);
void FPU_ESC1_EA(Bitu rm,PhysPt addr);
void FPU_ESC2_Normal(Bitu rm);
void FPU_ESC2_EA(Bitu rm,PhysPt addr);
void FPU_ESC3_Normal(Bitu rm);
void FPU_ESC3_EA(Bitu rm,PhysPt addr);
void FPU_ESC4_Normal(Bitu rm);
void FPU_ESC4_EA(Bitu rm,PhysPt addr);
void FPU_ESC5_Normal(Bitu rm);
void FPU_ESC5_EA(Bitu rm,PhysPt addr);
void FPU_ESC6_Normal(Bitu rm);
void FPU_ESC6_EA(Bitu rm,PhysPt addr);
void FPU_ESC7_Normal(Bitu rm);
void FPU_ESC7_EA(Bitu rm,PhysPt addr);

/* Floating point register, in the form the native host uses for "double".
 * This is slightly less precise than the 80-bit extended IEEE used by Intel,
 * but can be faster using the host processor "double" support. Most DOS games
 * using the FPU for 3D rendering are unaffected by the loss of precision.
 * However, there are cases where the full 80-bit precision is required such
 * as the "Fast Pentium memcpy trick" using the 80-bit versions of FLD/FST to
 * copy memory. */
typedef union {
    double d;
#ifndef WORDS_BIGENDIAN
    struct {
        uint32_t lower;
        int32_t upper;
    } l;
#else
    struct {
        int32_t upper;
        uint32_t lower;
    } l;
#endif
    int64_t ll;
	MMX_reg reg_mmx;
} FPU_Reg;

// dynamic x86 core needs this
typedef struct {
    uint32_t m1;
    uint32_t m2;
    uint16_t m3;

    uint16_t d1;
    uint32_t d2;
} FPU_P_Reg;

// memory barrier macro. to ensure that reads/stores to one half of the FPU reg struct
// do not overlap with reads/stores from the other half. things can go wrong if the
// compiler writes code to write the mantissa, then load the overall as float, then store
// the exponent. note this is not a hardware level memory barrier, this is a compiler
// level memory barrier against the optimization engine.
#if defined(__GCC__)
# define FPU_Reg_m_barrier()	__asm__ __volatile__ ("":::"memory")
#else
# define FPU_Reg_m_barrier()
#endif

#pragma pack(push,1)
typedef union {
// TODO: The configure script needs to use "long double" on x86/x86_64 and verify sizeof(long double) == 10,
//       else undef a macro to let the code emulate long double 80-bit IEEE. Also needs to determine host
//       byte order here so host long double matches our struct.
	struct {
		uint64_t	mantissa;		// [63:0]
		unsigned int	exponent:15;		// [78:64]
		unsigned int	sign:1;			// [79:79]
	} f;
#if defined(HAS_LONG_DOUBLE)
	long double		v;			// [79:0]
#endif
	struct {
		uint64_t	l;
		uint16_t	h;
	} raw;
} FPU_Reg_80;
// ^ Remember that in 80-bit extended, the mantissa contains both the fraction and integer bit. There is no
//   "implied bit" like 32-bit and 64-bit formats.
#pragma pack(pop)

#define FPU_Reg_80_exponent_bias	(16383)

#pragma pack(push,1)
typedef union {
	struct {
		uint64_t	mantissa:52;		// [51:0]
		uint64_t	exponent:11;		// [62:52]
		uint64_t	sign:1;			// [63:63]
	} f;
	double			v;
	uint64_t		raw;
} FPU_Reg_64;
#pragma pack(pop)

#define FPU_Reg_64_exponent_bias	(1023)
static const uint64_t FPU_Reg_64_implied_bit = ((uint64_t)1ULL << (uint64_t)52ULL);

#pragma pack(push,1)
typedef union {
	struct {
		uint32_t	mantissa:23;		// [22:0]
		uint32_t	exponent:8;		// [30:23]
		uint32_t	sign:1;			// [31:31]
	} f;
	float			v;
	uint32_t		raw;
} FPU_Reg_32;
#pragma pack(pop)

#define FPU_Reg_32_exponent_bias	(127)
static const uint32_t FPU_Reg_32_implied_bit = ((uint32_t)1UL << (uint32_t)23UL);

enum FPU_Tag {
	TAG_Valid = 0,
	TAG_Zero  = 1,
	TAG_Weird = 2,
	TAG_Empty = 3
};

template<class T, unsigned bitno, unsigned nbits=1>
struct RegBit
{
	T data;
	enum { basemask = (1 << nbits) - 1 };
	enum { mask = basemask << bitno };
	template <class T2>
	RegBit& operator=(T2 val)
	{
		data = (data & ~mask) | ((nbits > 1 ? val & basemask : !!val) << bitno);
		return *this;
	}
	operator unsigned() const { return (data & mask) >> bitno; }
};

struct FPUControlWord
{
	enum
	{
		mask8087    = 0x1fff,
		maskNon8087 = 0x1f7f,
		initValue   = 0x37f
	};
	enum class RoundMode
	{
		Nearest = 0,
		Down    = 1,
		Up      = 2,
		Chop    = 3
	};
	union
	{
		uint16_t raw;
		RegBit<decltype(raw), 0>     IM;  // Invalid operation mask
		RegBit<decltype(raw), 1>     DM;  // Denormalized operand mask
		RegBit<decltype(raw), 2>     ZM;  // Zero divide mask
		RegBit<decltype(raw), 3>     OM;  // Overflow mask
		RegBit<decltype(raw), 4>     UM;  // Underflow mask
		RegBit<decltype(raw), 5>     PM;  // Precision mask
		RegBit<decltype(raw), 7>     M;   // Interrupt mask
		RegBit<decltype(raw), 8, 2>  PC;  // Precision control
		RegBit<decltype(raw), 10, 2> RC;  // Rounding control
		RegBit<decltype(raw), 12>    IC;  // Infinity control
	};
	template<class T>
	FPUControlWord& operator=(T val)
	{
		raw = val & (CPU_ArchitectureType==CPU_ARCHTYPE_8086 ? mask8087 : maskNon8087);
		return *this;
	}
	operator unsigned() const
	{
		return raw;
	}
	template <class T>
	FPUControlWord& operator |=(T val)
	{
		raw |= val;
		return *this;
	}
	void init() { raw = initValue; }
	FPUControlWord allMasked() const
	{
		auto masked = *this;
		masked |= IM.mask | DM.mask | ZM.mask | OM.mask | UM.mask | PM.mask;
		return masked;
	}
};

typedef struct {
#if defined(HAS_LONG_DOUBLE)//probably shouldn't allow struct to change size based on this
	FPU_Reg		_do_not_use__regs[9];
#else
	FPU_Reg		regs[9];
#endif
	FPU_P_Reg	p_regs[9];
	FPU_Reg_80	regs_80[9];
#if defined(HAS_LONG_DOUBLE)//probably shouldn't allow struct to change size based on this
	bool		_do_not_use__use80[9];		// if set, use the 80-bit precision version
#else
	bool		use80[9];		// if set, use the 80-bit precision version
#endif
	FPU_Tag		tags[9];
	FPUControlWord  cw;
	uint16_t		sw;
	uint32_t		top;
} FPU_rec;


//get pi from a real library
#define PI		3.14159265358979323846
#define L2E		1.4426950408889634
#define L2T		3.3219280948873623
#define LN2		0.69314718055994531
#define LG2		0.3010299956639812


extern FPU_rec fpu;

#define TOP fpu.top
#define STV(i)  ( (fpu.top+ (i) ) & 7 )


uint16_t FPU_GetTag(void);
void FPU_FLDCW(PhysPt addr);

static INLINE void FPU_SetTag(uint16_t tag){
	for(Bitu i=0;i<8;i++)
		fpu.tags[i] = static_cast<FPU_Tag>((tag >>(2*i))&3);
}

static INLINE uint8_t FPU_GET_TOP(void) {
	return (fpu.sw & 0x3800U) >> 11U;
}

static INLINE void FPU_SET_TOP(Bitu val){
	fpu.sw &= ~0x3800U;
	fpu.sw |= (val & 7U) << 11U;
}


static INLINE void FPU_SET_C0(Bitu C){
	fpu.sw &= ~0x0100U;
	if(C) fpu.sw |=  0x0100U;
}

static INLINE void FPU_SET_C1(Bitu C){
	fpu.sw &= ~0x0200U;
	if(C) fpu.sw |=  0x0200U;
}

static INLINE void FPU_SET_C2(Bitu C){
	fpu.sw &= ~0x0400U;
	if(C) fpu.sw |=  0x0400U;
}

static INLINE void FPU_SET_C3(Bitu C){
	fpu.sw &= ~0x4000U;
	if(C) fpu.sw |= 0x4000U;
}

static INLINE void FPU_SET_D(Bitu C){
	fpu.sw &= ~0x0002U;
	if(C) fpu.sw |= 0x0002U;
}

static INLINE void FPU_LOG_WARN(Bitu tree, bool ea, Bitu group, Bitu sub) {
	LOG(LOG_FPU,LOG_WARN)("ESC %lu%s:Unhandled group %lu subfunction %lu",(long unsigned int)tree,ea?" EA":"",(long unsigned int)group,(long unsigned int)sub);
}

#endif
