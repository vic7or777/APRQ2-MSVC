/*
Copyright (C) 2003 Iceware.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#include "../game/q_shared.h"
#include <amdlib.h>
#include <amath.h>
#include <amd3dx.h>

/* The following section is from the AMD 3DNow! SDK */
/* =================== AMD START ================== */
#define HALFH   0x3f000000
#define PINH    0x7f800000
#define MASKSMH 0x807fffff

static float ones[2] = { 1.0f, 1.0f };
static long  sh_masks[2] = {  0x80000000u, 0x80000000u };

DWORD GetCPUCaps (CPUCAPS cap)
{
    DWORD res = 0;

    static DWORD features       = 0;
    static DWORD ext_features   = 0;
    static DWORD processor      = 0;
    static int   init           = 0;

    // Detect CPUID presence once, since all other requests depend on it
    if (init == 0)
    {
        __asm {
            pushfd                  // save EFLAGS to stack.
            pop     eax             // store EFLAGS in EAX.
            mov     edx, eax        // save in EBX for testing later.
            xor     eax, 0x200000   // switch bit 21.
            push    eax             // copy "changed" value to stack.
            popfd                   // save "changed" EAX to EFLAGS.
            pushfd
            pop     eax
            xor     eax, edx        // See if bit changeable.
            jnz     short foundit   // if so, mark 
            mov     eax,-1
            jmp     short around

            ALIGN   4
        foundit:
            // Load up the features and (where appropriate) extended features flags
            mov     eax,1               // Check for processor features
            CPUID
            mov     [features],edx      // Store features bits
            mov     eax,0x80000000      // Check for support of extended functions.
            CPUID
            cmp     eax,0x80000001      // Make sure function 0x80000001 supported.
            jb      short around
            mov     eax,0x80000001      // Select function 0x80000001
            CPUID
            mov     [processor],eax     // Store processor family/model/step
            mov     [ext_features],edx  // Store extended features bits
            mov     eax,1               // Set "Has CPUID" flag to true

        around:
            mov     [init],eax
        }
    }
    if (init == -1)
    {
        // No CPUID, so no CPUID functions are supported
        return 0;
    }

    // Otherwise, perform the requested tests
    switch (cap)
    {
    // Synthesized Capabilities
    case HAS_CPUID:
        // Always true if this code gets executed
        res = 1;
        break;

    case CPU_MFG:
        __asm {
            // Query manufacturer string
            mov     eax,0           // function 0 = manufacturer string
            CPUID

            // These tests could probably just check the 'ebx' part of the string,
            // but the entire string is checked for completeness.  Plus, this function
            // should not be used in time-critical code, because the CPUID instruction
            // serializes the processor. (That is, it flushes out the instruction pipeline.)

            // Test for 'AuthenticAMD'
            cmp     ebx,'htuA'
            jne     short not_amd
            cmp     edx,'itne'
            jne     short not_amd
            cmp     ecx,'DMAc'
            jne     short not_amd
            mov     eax,MFG_AMD
            jmp     short next_test
 
            // Test for 'GenuineIntel'
         not_amd:
            cmp     ebx,'uneG'
            jne     short not_intel
            cmp     edx,'Ieni'
            jne     short not_intel
            cmp     ecx,'letn'
            jne     short not_intel
            mov     eax,MFG_INTEL
            jmp     short next_test
 
            // Test for 'CyrixInstead'
         not_intel:
            cmp     ebx,'iryC'
            jne     short not_cyrix
            cmp     edx,'snIx'
            jne     short not_cyrix
            cmp     ecx,'deat'
            jne     short not_cyrix
            mov     eax,MFG_CYRIX
            jmp     short next_test
 
            // Test for 'CentaurHauls'
         not_cyrix:
            cmp     ebx,'tneC'
            jne     short not_centaur
            cmp     edx,'Hrua'
            jne     short not_centaur
            cmp     ecx,'slua'
            jne     short not_centaur
            mov     eax,MFG_CENTAUR
            jmp     short next_test
 
         not_centaur:
            mov     eax,MFG_UNKNOWN
 
         next_test:
            mov     [res],eax       // store result of previous tests
        }
        break;

    case CPU_TYPE:
        // Return a member of the CPUTYPES enumeration
        // Note: do NOT use this for determining presence of chip features, such
        // as MMX and 3DNow!  Instead, use GetCPUCaps (HAS_MMX) and GetCPUCaps (HAS_3DNOW),
        // which will accurately detect the presence of these features on all chips which
        // support them.
        switch (GetCPUCaps (CPU_MFG))
        {
        case MFG_AMD:
            switch ((processor >> 8) & 0xf) // extract family code
            {
            case 4: // Am486/AM5x86
                res = AMD_Am486;
                break;

            case 5: // K6
                switch ((processor >> 4) & 0xf) // extract model code
                {
                case 0: res = AMD_K5;       break;
                case 1: res = AMD_K5;       break;
                case 2: res = AMD_K5;       break;
                case 3: res = AMD_K5;       break;
                case 4: res = AMD_K6_MMX;   break;
                case 5: res = AMD_K6_MMX;   break;
                case 6: res = AMD_K6_MMX;   break;
                case 7: res = AMD_K6_MMX;   break;
                case 8: res = AMD_K6_2;     break;
                case 9: res = AMD_K6_3;     break;
                }
                break;

            case 6: // K7 Athlon
                res = AMD_K7;
                break;
            }
            break;

        case MFG_INTEL:
            switch ((processor >> 8) & 0xf) // extract family code
            {
            case 4:
                switch ((processor >> 4) & 0xf) // extract model code
                {
                case 0: res = INTEL_486DX;  break;
                case 1: res = INTEL_486DX;  break;
                case 2: res = INTEL_486SX;  break;
                case 3: res = INTEL_486DX2; break;
                case 4: res = INTEL_486SL;  break;
                case 5: res = INTEL_486SX2; break;
                case 7: res = INTEL_486DX2E;break;
                case 8: res = INTEL_486DX4; break;
                }
                break;

            case 5:
                switch ((processor >> 4) & 0xf) // extract model code
                {
                case 1: res = INTEL_Pentium;    break;
                case 2: res = INTEL_Pentium;    break;
                case 3: res = INTEL_Pentium;    break;
                case 4: res = INTEL_Pentium_MMX;break;
                }
                break;

            case 6:
                switch ((processor >> 4) & 0xf) // extract model code
                {
                case 1: res = INTEL_Pentium_Pro;break;
                case 3: res = INTEL_Pentium_II; break;
                case 5: res = INTEL_Pentium_II; break;  // actual differentiation depends on cache settings
                case 6: res = INTEL_Celeron;    break;
                case 7: res = INTEL_Pentium_III;break;  // actual differentiation depends on cache settings
                }
                break;
            }
            break;

        case MFG_CYRIX:
            res = UNKNOWN;
            break;

        case MFG_CENTAUR:
            res = UNKNOWN;
            break;
        }
        break;

    // Feature Bit Test Capabilities
    case HAS_FPU:       res = (features >> 0) & 1;      break;  // bit 0 = FPU
    case HAS_VME:       res = (features >> 1) & 1;      break;  // bit 1 = VME
    case HAS_DEBUG:     res = (features >> 2) & 1;      break;  // bit 2 = Debugger extensions
    case HAS_PSE:       res = (features >> 3) & 1;      break;  // bit 3 = Page Size Extensions
    case HAS_TSC:       res = (features >> 4) & 1;      break;  // bit 4 = Time Stamp Counter
    case HAS_MSR:       res = (features >> 5) & 1;      break;  // bit 5 = Model Specific Registers
    case HAS_MCE:       res = (features >> 6) & 1;      break;  // bit 6 = Machine Check Extensions
    case HAS_CMPXCHG8:  res = (features >> 7) & 1;      break;  // bit 7 = CMPXCHG8 instruction
    case HAS_MMX:       res = (features >> 23) & 1;     break;  // bit 23 = MMX
    case HAS_3DNOW:     res = (ext_features >> 31) & 1; break;  // bit 31 (ext) = 3DNow!
    }

    return res;
}

int has3DNow (void)
{
    return GetCPUCaps (HAS_3DNOW) != 0;
}
/* ==================== AMD END =================== */

int hasMMX (void)
{
    return GetCPUCaps (HAS_MMX) != 0;
}

/*
======================================================================

					    MATH REPLACEMENT FUNCTIONS

This section covers some common mathematical functions,
optimized for MMX/3DNow!/SSE/SSE2.
======================================================================
*/

/*
======================================================================
Routine:   ice_sqrt
Input:     x - float
Output:    return (float)sqrt(x)
Comment:   Uses the reciprical square root opcodes, as per the 3DNow!
           guide. Performs the Newton-Raphson iteration steps for
           best precision.
======================================================================
*/

float ice_sqrt(float x)
{
	float fval;

	if (has3DNow())
	{
		__asm
		{
			FEMMS
			movd	mm0,x
			pfrsqrt     (mm1,mm0)
			movq        mm2,mm1
			pfmul       (mm1,mm1)
			pfrsqit1    (mm1,mm0)
			pfrcpit2    (mm1,mm2)

			pfmul       (mm0,mm1)
			movd        fval,mm0
			FEMMS
		}
		return fval;
	}
	else
		return (float)sqrt(x);
}

float ice_atan2 (float x, float y)
{
    float res = 0.0f;

	if (has3DNow())						// 3DNow!
	{
		__asm
		{
			FEMMS
			movd    mm0,x       // aice_atan2 takes 'x' in mm0
			movd    mm1,y       // aice_atan2 takes 'y' in mm1
			call    _atan2
			movd    res,mm0     // result in mm0
			FEMMS
		}
		return res;
	}
	else
		return (float)atan2(x, y);
}

float ice_sin(float x)
{
    float fval;

	if (has3DNow())						// 3DNow!
	{

		// Setup registers for a_sin call, and return results
		// Note that a_sin trashes ebx,esi, so these must be preserved
		__asm
		{
			FEMMS
			movd    mm0,x
			push    ebx
			push    esi
			call    a_sin
			pop     esi
			pop     ebx
			movd    fval,mm0
			FEMMS
		}
		return fval;
	}
	else
		return (float)sin(x);
}

float ice_cos(float x)
{
    float fval;

	if (has3DNow())						// 3DNow!
	{
		// Setup registers for a_cos call, and return results
		// Note that a_cos trashes ebx,esi, so these must be preserved
		__asm
		{
			FEMMS
			movd    mm0,x
			push    ebx
			push    esi
			call    a_cos
			pop     esi
			pop     ebx
			movd    fval,mm0
			FEMMS
		}
		return fval;
	}
	else
		return (float)cos(x);
}

float ice_fabs (float x)
{
    float fval;

	if (has3DNow())						// 3DNow!
	{
		__asm
		{
			mov     eax,x           // starting with 
			and     eax,0x7fffffff  // And out the sign bit
			mov     fval,eax        // result in mm0
		}
		return fval;
	}
	else
		return (float)fabs(x);
}

float ice_ceil(float x)
{
    float fval;

	if (has3DNow())						// 3DNow!
	{
		__asm
		{
			FEMMS
			movd        mm0,[x]
			pxor        mm4,mm4         // mm4 = 0:0
			movq        mm3,[sh_masks]
			pf2id       (mm2,mm0)       // I = mm2
			movq        mm1,[ones]      // mm1 = 1:1
			pi2fd       (mm2,mm2)
			pand        mm3,mm0         // mm3 = sign bit
			pfsub       (mm0,mm2)       // F   = mm0
			pfcmpgt     (mm0,mm4)       // mm0 = F > 0.0
			pand        mm0,mm1         // mm0 = (F > 0) ? 1: 0
			por         mm2,mm3         // re-assert the sign bit
			por         mm0,mm3         // add sign bit
			pfadd       (mm0,mm2)
			movd        [fval],mm0
			FEMMS
		}
		return fval;
	}
	else
		return (float)ceil(x);
}

float ice_floor(float x)
{
    float fval;

	if (has3DNow())						// 3DNow!
	{
		__asm
		{
			FEMMS
			movd        mm0,x
			pf2id       (mm2,mm0)       // I = mm2
			pxor        mm1,mm1         // mm1 = 0|0
			pi2fd       (mm2,mm2)
			movq        mm3,mm2         // I = mm3
			pfsubr      (mm2,mm0)       // F = mm0
			pfcmpgt     (mm1,mm2)       // is F > I? (result becomes a bit mask)
			movq        mm0,[ones]      // mm0 = 1|1
			pand        mm0,mm1         // mm0 = F > I ? 1 : 0
			pfsubr      (mm0,mm3)       // mm0 = I - (F > I ? 1 : 0)
			movd        fval,mm0
			FEMMS
		}
		return fval;
	}
	else
		return (float)floor(x);
}

float ice_fmod(float x, float y)
{
    float res;

	if (has3DNow())						// 3DNow!
	{
		// Setup registers for a_fmod call, and return results
		__asm {
			FEMMS
			movd        mm0,x
			movd        mm1,y
			call        a_fmod
			movd        res,mm0
			FEMMS
		}

		return res;
	}
	else
		return (float)fmod(x, y);
}

float ice_acos(float x)
{
    float fval;

	if (has3DNow())						// 3DNow!
	{
		__asm
		{
			FEMMS
			movd    mm0,x
			call    a_acos
			movd    fval,mm0     // result in mm0
			FEMMS
		}
		return fval;
	}
	else
		return (float)acos(x);
}

float ice_atan (float x)
{
    float res = 0.0f;

	if (has3DNow())
	{
		__asm
		{
			FEMMS
			movd    mm0,x       // a_atan takes 'x' in mm0
			call    a_atan
			movd    res,mm0     // a_atan returns atan(x) in mm0
			FEMMS
		}
		return res;
	}
	else
		return (float)atan(x);
}

float ice_tan(float x)
{
    float fval;

	if (has3DNow())
	{        
		__asm
		{
			FEMMS
			movd    mm0,x
			push    ebx
			push    esi
			call    a_tan
			pop     esi
			pop     ebx
			movd    fval,mm0
			FEMMS
		}
		return fval;
	}
	else
		return (float)tan(x);
}

float ice_DotProduct(const vec3_t a, const vec3_t b)
{
	float	r;

	if (has3DNow())
	{
		__asm
		{
			FEMMS
			mov		eax, a
			mov		edx, b
			movq	mm0, [eax]
			movq	mm3, [edx]
			movd	mm1, [eax+8]
			movd	mm2, [edx+8]
			pfmul	(mm0, mm3)
			pfmul	(mm1, mm2)
			pfacc	(mm0, mm0)
			pfadd	(mm0, mm1)
			movd	r, mm0
			FEMMS
		}
		return r;
	}
	else
		return DotProduct(a, b);
}

