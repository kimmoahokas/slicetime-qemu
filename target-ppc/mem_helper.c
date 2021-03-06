/*
 *  PowerPC memory access emulation helpers for QEMU.
 *
 *  Copyright (c) 2003-2007 Jocelyn Mayer
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */
#include "cpu.h"
#include "host-utils.h"
#include "helper.h"

#include "helper_regs.h"

#if !defined(CONFIG_USER_ONLY)
#include "softmmu_exec.h"
#endif /* !defined(CONFIG_USER_ONLY) */

//#define DEBUG_OP

/*****************************************************************************/
/* Memory load and stores */

static inline target_ulong addr_add(CPUPPCState *env, target_ulong addr,
                                    target_long arg)
{
#if defined(TARGET_PPC64)
    if (!msr_is_64bit(env, env->msr)) {
        return (uint32_t)(addr + arg);
    } else
#endif
    {
        return addr + arg;
    }
}

void helper_lmw(CPUPPCState *env, target_ulong addr, uint32_t reg)
{
    for (; reg < 32; reg++) {
        if (msr_le) {
            env->gpr[reg] = bswap32(cpu_ldl_data(env, addr));
        } else {
            env->gpr[reg] = cpu_ldl_data(env, addr);
        }
        addr = addr_add(env, addr, 4);
    }
}

void helper_stmw(CPUPPCState *env, target_ulong addr, uint32_t reg)
{
    for (; reg < 32; reg++) {
        if (msr_le) {
            cpu_stl_data(env, addr, bswap32((uint32_t)env->gpr[reg]));
        } else {
            cpu_stl_data(env, addr, (uint32_t)env->gpr[reg]);
        }
        addr = addr_add(env, addr, 4);
    }
}

void helper_lsw(CPUPPCState *env, target_ulong addr, uint32_t nb, uint32_t reg)
{
    int sh;

    for (; nb > 3; nb -= 4) {
        env->gpr[reg] = cpu_ldl_data(env, addr);
        reg = (reg + 1) % 32;
        addr = addr_add(env, addr, 4);
    }
    if (unlikely(nb > 0)) {
        env->gpr[reg] = 0;
        for (sh = 24; nb > 0; nb--, sh -= 8) {
            env->gpr[reg] |= cpu_ldub_data(env, addr) << sh;
            addr = addr_add(env, addr, 1);
        }
    }
}
/* PPC32 specification says we must generate an exception if
 * rA is in the range of registers to be loaded.
 * In an other hand, IBM says this is valid, but rA won't be loaded.
 * For now, I'll follow the spec...
 */
void helper_lswx(CPUPPCState *env, target_ulong addr, uint32_t reg,
                 uint32_t ra, uint32_t rb)
{
    if (likely(xer_bc != 0)) {
        if (unlikely((ra != 0 && reg < ra && (reg + xer_bc) > ra) ||
                     (reg < rb && (reg + xer_bc) > rb))) {
            helper_raise_exception_err(env, POWERPC_EXCP_PROGRAM,
                                       POWERPC_EXCP_INVAL |
                                       POWERPC_EXCP_INVAL_LSWX);
        } else {
            helper_lsw(env, addr, xer_bc, reg);
        }
    }
}

void helper_stsw(CPUPPCState *env, target_ulong addr, uint32_t nb,
                 uint32_t reg)
{
    int sh;

    for (; nb > 3; nb -= 4) {
        cpu_stl_data(env, addr, env->gpr[reg]);
        reg = (reg + 1) % 32;
        addr = addr_add(env, addr, 4);
    }
    if (unlikely(nb > 0)) {
        for (sh = 24; nb > 0; nb--, sh -= 8) {
            cpu_stb_data(env, addr, (env->gpr[reg] >> sh) & 0xFF);
            addr = addr_add(env, addr, 1);
        }
    }
}

static void do_dcbz(CPUPPCState *env, target_ulong addr, int dcache_line_size)
{
    int i;

    addr &= ~(dcache_line_size - 1);
    for (i = 0; i < dcache_line_size; i += 4) {
        cpu_stl_data(env, addr + i, 0);
    }
    if (env->reserve_addr == addr) {
        env->reserve_addr = (target_ulong)-1ULL;
    }
}

void helper_dcbz(CPUPPCState *env, target_ulong addr)
{
    do_dcbz(env, addr, env->dcache_line_size);
}

void helper_dcbz_970(CPUPPCState *env, target_ulong addr)
{
    if (((env->spr[SPR_970_HID5] >> 7) & 0x3) == 1) {
        do_dcbz(env, addr, 32);
    } else {
        do_dcbz(env, addr, env->dcache_line_size);
    }
}

void helper_icbi(CPUPPCState *env, target_ulong addr)
{
    addr &= ~(env->dcache_line_size - 1);
    /* Invalidate one cache line :
     * PowerPC specification says this is to be treated like a load
     * (not a fetch) by the MMU. To be sure it will be so,
     * do the load "by hand".
     */
    cpu_ldl_data(env, addr);
}

/* XXX: to be tested */
target_ulong helper_lscbx(CPUPPCState *env, target_ulong addr, uint32_t reg,
                          uint32_t ra, uint32_t rb)
{
    int i, c, d;

    d = 24;
    for (i = 0; i < xer_bc; i++) {
        c = cpu_ldub_data(env, addr);
        addr = addr_add(env, addr, 1);
        /* ra (if not 0) and rb are never modified */
        if (likely(reg != rb && (ra == 0 || reg != ra))) {
            env->gpr[reg] = (env->gpr[reg] & ~(0xFF << d)) | (c << d);
        }
        if (unlikely(c == xer_cmp)) {
            break;
        }
        if (likely(d != 0)) {
            d -= 8;
        } else {
            d = 24;
            reg++;
            reg = reg & 0x1F;
        }
    }
    return i;
}

/*****************************************************************************/
/* Altivec extension helpers */
#if defined(HOST_WORDS_BIGENDIAN)
#define HI_IDX 0
#define LO_IDX 1
#else
#define HI_IDX 1
#define LO_IDX 0
#endif

#define LVE(name, access, swap, element)                        \
    void helper_##name(CPUPPCState *env, ppc_avr_t *r,          \
                       target_ulong addr)                       \
    {                                                           \
        size_t n_elems = ARRAY_SIZE(r->element);                \
        int adjust = HI_IDX*(n_elems - 1);                      \
        int sh = sizeof(r->element[0]) >> 1;                    \
        int index = (addr & 0xf) >> sh;                         \
                                                                \
        if (msr_le) {                                           \
            r->element[LO_IDX ? index : (adjust - index)] =     \
                swap(access(env, addr));                        \
        } else {                                                \
            r->element[LO_IDX ? index : (adjust - index)] =     \
                access(env, addr);                              \
        }                                                       \
    }
#define I(x) (x)
LVE(lvebx, cpu_ldub_data, I, u8)
LVE(lvehx, cpu_lduw_data, bswap16, u16)
LVE(lvewx, cpu_ldl_data, bswap32, u32)
#undef I
#undef LVE

#define STVE(name, access, swap, element)                               \
    void helper_##name(CPUPPCState *env, ppc_avr_t *r,                  \
                       target_ulong addr)                               \
    {                                                                   \
        size_t n_elems = ARRAY_SIZE(r->element);                        \
        int adjust = HI_IDX * (n_elems - 1);                            \
        int sh = sizeof(r->element[0]) >> 1;                            \
        int index = (addr & 0xf) >> sh;                                 \
                                                                        \
        if (msr_le) {                                                   \
            access(env, addr, swap(r->element[LO_IDX ? index :          \
                                              (adjust - index)]));      \
        } else {                                                        \
            access(env, addr, r->element[LO_IDX ? index :               \
                                         (adjust - index)]);            \
        }                                                               \
    }
#define I(x) (x)
STVE(stvebx, cpu_stb_data, I, u8)
STVE(stvehx, cpu_stw_data, bswap16, u16)
STVE(stvewx, cpu_stl_data, bswap32, u32)
#undef I
#undef LVE

#undef HI_IDX
#undef LO_IDX

/*****************************************************************************/
/* Softmmu support */
#if !defined(CONFIG_USER_ONLY)

#define MMUSUFFIX _mmu

#define SHIFT 0
#include "softmmu_template.h"

#define SHIFT 1
#include "softmmu_template.h"

#define SHIFT 2
#include "softmmu_template.h"

#define SHIFT 3
#include "softmmu_template.h"

/* try to fill the TLB and return an exception if error. If retaddr is
   NULL, it means that the function was called in C code (i.e. not
   from generated code or from helper.c) */
/* XXX: fix it to restore all registers */
void tlb_fill(CPUPPCState *env, target_ulong addr, int is_write, int mmu_idx,
              uintptr_t retaddr)
{
    int ret;

    ret = cpu_ppc_handle_mmu_fault(env, addr, is_write, mmu_idx);
    if (unlikely(ret != 0)) {
        if (likely(retaddr)) {
            /* now we have a real cpu fault */
            cpu_restore_state(env, retaddr);
        }
        helper_raise_exception_err(env, env->exception_index, env->error_code);
    }
}
#endif /* !CONFIG_USER_ONLY */
