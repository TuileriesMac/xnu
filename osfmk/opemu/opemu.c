//
//  opemu.c
//  OPEMU
//
//  Created by Meowthra on 2017/11/16.
//  Copyright © 2017年 Meowthra. All rights reserved.
//  Made in Taiwan.

#include "opemu.h"

#include "ins/f16c.h"
#include "ins/bmi.h"
#include "ins/aes.h"
#include "ins/avx.h"
#include "ins/vgather.h"
#include "ins/fma.h"
#include "ins/vsse.h"
#include "ins/vsse2.h"
#include "ins/vsse3.h"
#include "ins/vssse3.h"
#include "ins/vsse41.h"
#include "ins/vsse42.h"

/** Runs the REX emulator. returns the number of bytes consumed. **/
int rex_ins(uint8_t *instruction, x86_saved_state_t *state, int kernel_trap)
{
    uint8_t *bytep = instruction;
    uint8_t ins_size = 0;
    uint8_t modbyte = 0; //Calculate byte 0 - modrm long
    uint16_t reg_size = 64;
    uint8_t operand_size = 0;
    uint8_t leading_opcode = 0;
    uint8_t simd_prefix = 0; //NP
    uint8_t addrs32 = 0; //Address-size override prefix (Legacy Prefixes 0x67)

    uint8_t high_reg = 0;
    uint8_t high_index = 0;
    uint8_t high_base = 0;

    uint8_t REX_W = 0; //REX.W
    uint8_t REX_R = 0; //REX.R
    uint8_t REX_X = 0; //REX.X
    uint8_t REX_B = 0; //REX.B


    // Legacy Prefixes (SIMD prefix)
    if (*bytep == 0x66) {
        reg_size = 128;
        simd_prefix = 1;
        bytep++;
        ins_size++;
        modbyte++;
    } else if (*bytep == 0xF3) {
        reg_size = 128;
        simd_prefix = 2;
        bytep++;
        ins_size++;
        modbyte++;
    } else if (*bytep == 0xF2) {
        reg_size = 128;
        simd_prefix = 3;
        bytep++;
        ins_size++;
        modbyte++;
    } else if (*bytep == 0x67) {
        addrs32 = 1;
        bytep++;
        ins_size++;
        modbyte++;
    } else if ( (*bytep == 0x26) || (*bytep == 0x2E) || (*bytep == 0x36) || (*bytep == 0x3E) ||  (*bytep == 0x64) || (*bytep == 0x65) ) {
        bytep++;
        ins_size++;
        modbyte++;
    }

    //REX Prefix
    if ((*bytep & 0xF0) == 0x40) {
        uint8_t *REX_BYTE = &bytep[0];
        REX_W = (*REX_BYTE >> 3) & 1;         //REX.W
        REX_R = (*REX_BYTE >> 2) & 1;         //REX.R
        REX_X = (*REX_BYTE >> 1) & 1;         //REX.X
        REX_B = *REX_BYTE & 1;                //REX.B

        bytep++;
        ins_size++;
        modbyte++;
    }

    if (REX_W == 1)
        operand_size = 64;
    else
        operand_size = 32;

    if (REX_R == 1) {
        if (reg_size == 128) {
            high_reg = 1;
        }
    }

    if (REX_X == 1) high_index = 1;
    if (REX_B == 1) high_base = 1;

    // leading opcode
    if (bytep[0] == 0x0F && bytep[1] == 0x3A) {
        leading_opcode = 3; //0F3A
        bytep+=2;
        ins_size+=2;
        modbyte+=2;
    } else if (bytep[0] == 0x0F && bytep[1] == 0x38) {
        leading_opcode = 2; //0F38
        bytep+=2;
        ins_size+=2;
        modbyte+=2;
    } else if (bytep[0] == 0x0F) {
        leading_opcode = 1; //0F
        bytep++;
        ins_size++;
        modbyte++;
    }

    /* opcode */
    uint8_t opcode = bytep[0];
    uint8_t *modrm = &bytep[1];

    bytep++;
    ins_size++;
    modbyte++;

    // AES Instruction set
    ins_size = aes_instruction(state, opcode, modrm, high_reg, high_index, high_base, reg_size, operand_size, leading_opcode, simd_prefix, bytep, ins_size, modbyte, kernel_trap);


    return ins_size;
}


/** Runs the VEX emulator. returns the number of bytes consumed. **/
int vex_ins(uint8_t *instruction, x86_saved_state_t *state, int kernel_trap)
{
    uint8_t *bytep = instruction;
    uint8_t ins_size = 0;
    uint8_t modbyte = 0; //Calculate byte 0 - modrm long
    //const char* opcodename;
    uint8_t addrs32 = 0; //Address-size override prefix (Legacy Prefixes 0x67)

    uint16_t reg_size = 0;
    uint8_t operand_size = 0;
    uint8_t leading_opcode = 0;
    uint8_t simd_prefix = 0;
    //uint8_t op_type = 0;

    uint8_t high_reg = 0;
    uint8_t high_index = 0;
    uint8_t high_base = 0;
    uint8_t vexreg = 0;

    uint8_t VEX_R = 0; //VEX.R
    uint8_t VEX_X = 0; //VEX.X
    uint8_t VEX_B = 0; //VEX.B
    uint8_t VEX_M = 0; //VEX.mmmmm
    uint8_t VEX_W = 0; //VEX.W
    uint8_t VEX_V = 0; //VEX.vvvv
    uint8_t VEX_L = 0; //VEX.L
    uint8_t VEX_P = 0; //VEX.pp

    // Legacy Prefixes
    if (*bytep == 0x67) {
        addrs32 = 1;
        bytep++;
        ins_size++;
        modbyte++;
    }

    // VEX Prefixes
    if(*bytep == 0xC4) {
        uint8_t *VEX_BYTE1 = &bytep[1];
        uint8_t *VEX_BYTE2 = &bytep[2];
        VEX_R = *VEX_BYTE1 >> 0x7;         //VEX.R
        VEX_X = (*VEX_BYTE1 >> 0x6) & 0x1; //VEX.X
        VEX_B = (*VEX_BYTE1 >> 5) & 0x1;   //VEX.B
        VEX_M = *VEX_BYTE1 & 0x1F;         //VEX.mmmmm
        VEX_W = *VEX_BYTE2 >> 7;           //VEX.W
        VEX_V = (*VEX_BYTE2 >> 3) & 0xF;   //VEX.vvvv
        VEX_L = (*VEX_BYTE2 >> 2) & 0x1;   //VEX.L
        VEX_P = *VEX_BYTE2 & 0x3;          //VEX.pp

        bytep+=3;
        ins_size+=3;
        modbyte+=3;
    } else {
        if(*bytep == 0xC5) {
            uint8_t *VEX_BYTE1 = &bytep[1];
            VEX_R = *VEX_BYTE1 >> 0x7;        //VEX.R
            VEX_V = (*VEX_BYTE1 >> 3) & 0xF;  //VEX.vvvv
            VEX_L = (*VEX_BYTE1 >> 2) & 0x1;  //VEX.L
            VEX_P = *VEX_BYTE1 & 0x3;         //VEX.pp

            VEX_X = 1;
            VEX_B = 1;
            //VEX_W = 1;

            bytep+=2;
            ins_size+=2;
            modbyte+=2;
        } else {
            return 0;
        }
    }

    if (VEX_R == 0) high_reg = 1;
    if (VEX_X == 0) high_index = 1;
    if (VEX_B == 0) high_base = 1;

    //L0=128bit XMM / L1=256bit YMM
    if (VEX_L == 1)
        reg_size = 256;
    else
        reg_size = 128;

    if (VEX_W == 1)
        operand_size = 64;
    else
        operand_size = 32;

    if (VEX_M == 1)
        leading_opcode = 1; //0F
    else if (VEX_M == 2)
        leading_opcode = 2; //0F38
    else if (VEX_M == 3)
        leading_opcode = 3; //0F3A
    else
        leading_opcode = 1; //0F

    if (VEX_P == 1)
        simd_prefix = 1; //66
    else if (VEX_P == 2)
        simd_prefix = 2; //F3
    else if (VEX_P == 3)
        simd_prefix = 3; //F2
    else
        simd_prefix = 0; //None

    /* VEX.vvvv Register */
    if (VEX_V == 15) vexreg = 0;
    else if (VEX_V == 14) vexreg = 1;
    else if (VEX_V == 13) vexreg = 2;
    else if (VEX_V == 12) vexreg = 3;
    else if (VEX_V == 11) vexreg = 4;
    else if (VEX_V == 10) vexreg = 5;
    else if (VEX_V == 9) vexreg = 6;
    else if (VEX_V == 8) vexreg = 7;
    else if (VEX_V == 7) vexreg = 8;
    else if (VEX_V == 6) vexreg = 9;
    else if (VEX_V == 5) vexreg = 10;
    else if (VEX_V == 4) vexreg = 11;
    else if (VEX_V == 3) vexreg = 12;
    else if (VEX_V == 2) vexreg = 13;
    else if (VEX_V == 1) vexreg = 14;
    else if (VEX_V == 0) vexreg = 15;

    /* opcode */
    uint8_t opcode = bytep[0];
    uint8_t *modrm = &bytep[1];
    //uint8_t imm;

    bytep++;
    ins_size++;
    modbyte++;

    //uint8_t modreg = (*modrm >> 3) & 0x7;

    // BMI1/2 Instruction set
    ins_size = bmi_instruction(state, vexreg, opcode, modrm, high_reg, high_index, high_base, operand_size, leading_opcode, simd_prefix, bytep, ins_size, modbyte, kernel_trap);

    // F16C Instruction set
    if (ins_size == 0) {
        ins_size = f16c_instruction(state, vexreg, opcode, modrm, high_reg, high_index, high_base, reg_size, leading_opcode, simd_prefix, bytep, ins_size, modbyte, kernel_trap);
    }
    // VAES Instruction set
    if (ins_size == 0) {
        ins_size = vaes_instruction(state, vexreg, opcode, modrm, high_reg, high_index, high_base, reg_size, operand_size, leading_opcode, simd_prefix, bytep, ins_size, modbyte, kernel_trap);
    }
    // AVX / AVX2 Instruction set
    if (ins_size == 0) {
        ins_size = avx_instruction(state, vexreg, opcode, modrm, high_reg, high_index, high_base, reg_size, operand_size, leading_opcode, simd_prefix, bytep, ins_size, modbyte, kernel_trap);
    }
    // AVX Gather Instruction set
    if (ins_size == 0) {
        ins_size = vgather_instruction(state, vexreg, opcode, modrm, high_reg, high_index, high_base, reg_size, operand_size, leading_opcode, simd_prefix, bytep, ins_size, modbyte, kernel_trap);
    }
    // FMA Instruction set
    if (ins_size == 0) {
        ins_size = fma_instruction(state, vexreg, opcode, modrm, high_reg, high_index, high_base, reg_size, operand_size, leading_opcode, simd_prefix, bytep, ins_size, modbyte, kernel_trap);
    }
    // VSSE Instruction set
    if (ins_size == 0) {
        ins_size = vsse_instruction(state, vexreg, opcode, modrm, high_reg, high_index, high_base, reg_size, operand_size, leading_opcode, simd_prefix, bytep, ins_size, modbyte, kernel_trap);
    }
    // VSSE2 Instruction set
    if (ins_size == 0) {
        ins_size = vsse2_instruction(state, vexreg, opcode, modrm, high_reg, high_index, high_base, reg_size, operand_size, leading_opcode, simd_prefix, bytep, ins_size, modbyte, kernel_trap);
    }
    // VSSE3 Instruction set
    if (ins_size == 0) {
        ins_size = vsse3_instruction(state, vexreg, opcode, modrm, high_reg, high_index, high_base, reg_size, operand_size, leading_opcode, simd_prefix, bytep, ins_size, modbyte, kernel_trap);
    }
    // VSSSE3 Instruction set
    if (ins_size == 0) {
        ins_size = vssse3_instruction(state, vexreg, opcode, modrm, high_reg, high_index, high_base, reg_size, operand_size, leading_opcode, simd_prefix, bytep, ins_size, modbyte, kernel_trap);
    }
    // VSSE4.1 Instruction set
    if (ins_size == 0) {
        ins_size = vsse41_instruction(state, vexreg, opcode, modrm, high_reg, high_index, high_base, reg_size, operand_size, leading_opcode, simd_prefix, bytep, ins_size, modbyte, kernel_trap);
    }
    // VSSE4.2 Instruction set
    if (ins_size == 0) {
        ins_size = vsse42_instruction(state, vexreg, opcode, modrm, high_reg, high_index, high_base, reg_size, operand_size, leading_opcode, simd_prefix, bytep, ins_size, modbyte, kernel_trap);
    }

    return ins_size;
}

/*********************************************************/
int get_consumed(uint8_t *ModRM) {
    uint8_t mod = *ModRM >> 6; // ModRM.mod
    uint8_t num_src = *ModRM & 0x7; // ModRM.r/m (SRC1:register or memory)
    int consumed = 1; //modrm byte +1

    if (mod == 0) {
        if (num_src == 4) {
            //sib
            uint8_t *SIB = (uint8_t *)&ModRM[1];
            uint8_t base = *SIB & 0x7;
            if (base == 5) {
                consumed += 5;
            } else {
                consumed++;
            }
        } else {
            if (num_src == 5) {
                consumed += 4;
            }
        }
    }

    if (mod == 1) {
        if (num_src == 4) {
            consumed+= 2;
        } else {
            consumed++;
        }
    }

    if (mod == 2) {
        if (num_src == 4) {
            consumed += 5;
        } else {
            consumed += 4;
        }
    }

    return consumed;
}

/*********************************************************
 *** Get the register or memory address value.         ***
 *********************************************************/
void get_x64regs(uint8_t *modrm,
                 uint8_t high_reg,
                 uint8_t high_index,
                 uint8_t high_base,
                 void *src,
                 void *vsrc,
                 void *dst,
                 uint8_t vexreg,
                 x86_saved_state_t *saved_state,
                 int kernel_trap,
                 uint8_t modbyte,
                 uint64_t *rmaddrs
                )
{
    uint8_t mod = *modrm >> 6; // ModRM.mod
    uint8_t num_dst = (*modrm >> 3) & 0x7; // ModRM.reg (DEST)
    uint8_t num_src = *modrm & 0x7; // ModRM.r/m (SRC1:register or memory)
    uint8_t bytelen = 0;

    if (is_saved_state64(saved_state)) {

        if (high_reg) num_dst += 8;
        if (high_base) num_src += 8;

        _store_m64(vexreg, (M64*)vsrc, saved_state);
        _store_m64(num_dst, (M64*)dst, saved_state);

        if(mod == 3) //mod field = 11b
        {
            _store_m64(num_src, (M64*)src, saved_state);

        } else {
            // Get the Mod.R/M memory address value.
            uint64_t maddr = 0;
            maddr = addressing64(saved_state, modrm, mod, num_src, high_index, high_base, modbyte, bytelen);
            *rmaddrs = maddr;
            //((M64*)src)->u64 = *((uint64_t*)maddr);
            copyin(maddr, (char*) &((M64*)src)->u64, 8);
        }

    } else {
        _store_m32(vexreg, (M32*)vsrc, saved_state);
        _store_m32(num_dst, (M32*)dst, saved_state);

        if(mod == 3) //mod field = 11b
        {
            _store_m32(num_src, (M32*)src, saved_state);

        } else {
            uint32_t maddr = 0;
            // Get the Mod.R/M memory address value.
            maddr = addressing32(saved_state, modrm, mod, num_src, high_index, high_base, modbyte, bytelen);
            *rmaddrs = maddr;
            //((M32*)src)->u32 = *((uint32_t*)&maddr);
            copyin(maddr, (char*) &((M32*)src)->u32, 4);

        }
    }
}

void get_rexregs(uint8_t *modrm,
                 uint8_t high_reg,
                 uint8_t high_index,
                 uint8_t high_base,
                 void *src,
                 void *dst,
                 x86_saved_state_t *saved_state,
                 int kernel_trap,
                 uint16_t reg_size,
                 uint16_t rm_size,
                 uint8_t modbyte,
                 uint64_t *rmaddrs
                )
{

    /*** ModRM Is First Addressing Modes ***/
    /*** SIB Is Second Addressing Modes ***/
    uint8_t mod = *modrm >> 6; // ModRM.mod
    uint8_t num_dst = (*modrm >> 3) & 0x7; // ModRM.reg (DEST)
    uint8_t num_src = *modrm & 0x7; // ModRM.r/m (SRC1:register or memory)
    uint8_t bytelen = 0;

    if (high_reg) num_dst += 8;
    if (high_base) num_src += 8;

    if (reg_size == 128)
        _store_xmm(num_dst, (XMM*)dst);
    else
        _store_mmx(num_dst, (MM*)dst);

    if(mod == 3) //mod field = 11b
    {
        if (rm_size == 128)
            _store_xmm(num_src, (XMM*)src);
        else
            _store_mmx(num_src, (MM*)src);

    } else {
        // Get the Mod.R/M memory address value.
        if (is_saved_state64(saved_state)) {
            uint64_t maddr = 0;
            maddr = addressing64(saved_state, modrm, mod, num_src, high_index, high_base, modbyte, bytelen);
            *rmaddrs = maddr;
            if (rm_size == 128) {
                //((XMM*)src)->u128 = *((__uint128_t*)maddr);
                copyin(maddr, (char*) &((XMM*)src)->u128, 16);
            } else if(rm_size == 64) {
                //((MM*)src)->u64 = *((uint64_t*)maddr);
                copyin(maddr, (char*) &((MM*)src)->u64, 8);
            } else if(rm_size == 32) {
                copyin(maddr, (char*) &((MM*)src)->u32, 4);
            }
        } else {
            uint32_t maddr = 0;
            maddr = addressing32(saved_state, modrm, mod, num_src, high_index, high_base, modbyte, bytelen);
            *rmaddrs = maddr;
            if (rm_size == 128) {
                //((XMM*)src)->u128 = *((__uint128_t*)&maddr);
                copyin(maddr, (char*) &((XMM*)src)->u128, 16);
            } else if(rm_size == 64) {
                //((MM*)src)->u64 = *((uint64_t*)&maddr);
                copyin(maddr, (char*) &((MM*)src)->u64, 8);
            } else if(rm_size == 32) {
                copyin(maddr, (char*) &((MM*)src)->u32, 4);
            }
        }
    }
}

void get_vexregs(uint8_t *modrm,
                 uint8_t high_reg,
                 uint8_t high_index,
                 uint8_t high_base,
                 void *src,
                 void *vsrc,
                 void *dst,
                 uint8_t vexreg,
                 x86_saved_state_t *saved_state,
                 int kernel_trap,
                 uint16_t reg_size,
                 uint16_t rm_size,
                 uint8_t modbyte,
                 uint64_t *rmaddrs
                )
{

    /*** ModRM Is First Addressing Modes ***/
    /*** SIB Is Second Addressing Modes ***/
    uint8_t mod = *modrm >> 6; // ModRM.mod
    uint8_t num_dst = (*modrm >> 3) & 0x7; // ModRM.reg (DEST)
    uint8_t num_src = *modrm & 0x7; // ModRM.r/m (SRC1:register or memory)
    uint8_t bytelen = 0;

    if (high_reg) num_dst += 8;
    if (high_base) num_src += 8;

    if( reg_size == 256)
        _store_ymm(vexreg, (YMM*)vsrc);
    else {
        if (reg_size == 128)
            _store_xmm(vexreg, (XMM*)vsrc);
        else {
            _store_mmx(vexreg, (MM*)vsrc);
        }
    }

    if (reg_size == 256)
        _store_ymm(num_dst, (YMM*)dst);
    else {
        if (reg_size == 128)
            _store_xmm(num_dst, (XMM*)dst);
        else {
            _store_mmx(num_dst, (MM*)dst);
        }
    }

    if(mod == 3) //mod field = 11b
    {
        if (rm_size == 256)
            _store_ymm(num_src, (YMM*)src);
        else {
            if(rm_size == 128)
                _store_xmm(num_src, (XMM*)src);
            else {
                _store_mmx(num_src, (MM*)src);
            }
        }
    } else {
        // Get the Mod.R/M memory address value.
        if (is_saved_state64(saved_state)) {
            uint64_t maddr = 0;
            maddr = addressing64(saved_state, modrm, mod, num_src, high_index, high_base, modbyte, bytelen);
            *rmaddrs = maddr;
            if(rm_size == 256) {
                //((YMM*)src)->u256 = *((__uint256_t*)maddr);
                copyin(maddr, (char*) &((YMM*)src)->u256, 32);
            } else if(rm_size == 128) {
                //((XMM*)src)->u128 = *((__uint128_t*)maddr);
                copyin(maddr, (char*) &((XMM*)src)->u128, 16);
            } else if(rm_size == 64) {
                //((MM*)src)->u64 = *((uint64_t*)maddr);
                copyin(maddr, (char*) &((MM*)src)->u64, 8);
            } else if(rm_size == 32) {
                copyin(maddr, (char*) &((MM*)src)->u32, 4);
            }
        } else {
            uint32_t maddr = 0;
            maddr = addressing32(saved_state, modrm, mod, num_src, high_index, high_base, modbyte, bytelen);
            *rmaddrs = maddr;
            if(rm_size == 256) {
                //((YMM*)src)->u256 = *((__uint256_t*)maddr);
                copyin(maddr, (char*) &((YMM*)src)->u256, 32);
            } else if(rm_size == 128) {
                //((XMM*)src)->u128 = *((__uint128_t*)&maddr);
                copyin(maddr, (char*) &((XMM*)src)->u128, 16);
            } else if(rm_size == 64) {
                //((MM*)src)->u64 = *((uint64_t*)&maddr);
                copyin(maddr, (char*) &((MM*)src)->u64, 8);
            } else if(rm_size == 32) {
                copyin(maddr, (char*) &((MM*)src)->u32, 4);
            }
        }

    }
}

uint64_t addressing64(
    x86_saved_state_t *saved_state,
    uint8_t *modrm,
    uint8_t mod,
    uint8_t num_src,
    uint8_t high_index,
    uint8_t high_base,
    uint8_t modbyte,
    uint8_t bytelen
)
{
    uint64_t address = 0;
    uint64_t reg_sel[16];
    reg_sel[0] = saved_state64(saved_state)->rax;
    reg_sel[1] = saved_state64(saved_state)->rcx;
    reg_sel[2] = saved_state64(saved_state)->rdx;
    reg_sel[3] = saved_state64(saved_state)->rbx;
    reg_sel[4] = saved_state64(saved_state)->isf.rsp;
    reg_sel[5] = saved_state64(saved_state)->rbp;
    reg_sel[6] = saved_state64(saved_state)->rsi;
    reg_sel[7] = saved_state64(saved_state)->rdi;
    reg_sel[8] = saved_state64(saved_state)->r8;
    reg_sel[9] = saved_state64(saved_state)->r9;
    reg_sel[10] = saved_state64(saved_state)->r10;
    reg_sel[11] = saved_state64(saved_state)->r11;
    reg_sel[12] = saved_state64(saved_state)->r12;
    reg_sel[13] = saved_state64(saved_state)->r13;
    reg_sel[14] = saved_state64(saved_state)->r14;
    reg_sel[15] = saved_state64(saved_state)->r15;

    // MOD 0
    if (mod == 0) {
        //SIB START
        if ((num_src == 4) || (num_src == 12)) { //SIB
            uint8_t *sib = (uint8_t *)&modrm[1]; //Second Addressing Modes
            uint8_t scale = *sib >> 6; //SIB Scale field
            uint8_t base = *sib & 0x7; //SIB Base
            uint8_t index = (*sib >> 3) & 0x7; //SIB Index
            uint8_t factor = 0; //Scaling factor
            if (scale == 0) factor = 1;
            else if (scale == 1) factor = 2;
            else if (scale == 2) factor = 4;
            else if (scale == 3) factor = 8;
            if (high_index) index += 8;
            if (high_base) base += 8;

            if (index == 4) { //INDEX4
                if ((base == 5) || (base == 13)) {
                    //[DISP32] MODRM + 5
                    bytelen = modbyte + 5;
                    address = saved_state64(saved_state)->isf.rip + bytelen + *((int32_t*)&modrm[2]);
                    //consumed += 5;
                } else {
                    //[BASE] MODRM + 1
                    address = reg_sel[base];
                    //consumed++;
                }
            } else { //Not INDEX4
                if ((base == 5) || (base == 13)) {
                    //[DISP32 + (INDEX * FACTOR)] MODRM + 5
                    bytelen = modbyte + 5;
                    address = (saved_state64(saved_state)->isf.rip + bytelen + *((int32_t*)&modrm[2])) + (reg_sel[index] * factor);
                    //consumed += 5;
                } else {
                    //[BASE + (INDEX * FACTOR)] MODRM + 1
                    address = reg_sel[base] + (reg_sel[index] * factor);
                    //consumed++;
                }
            }
        }
        //SIB END
        else {
            if ((num_src == 5) || (num_src == 13)) {
                //[DISP32] MODRM + 4
                bytelen = modbyte + 4;
                address = saved_state64(saved_state)->isf.rip + bytelen + *((int32_t*)&modrm[1]);
                //consumed += 4;
            } else {
                //[SRC]
                address = reg_sel[num_src];
            }
        }
    }

    // MOD 1
    if (mod == 1) {
        //SIB START
        if ((num_src == 4) || (num_src == 12)) { //SIB
            uint8_t *sib = (uint8_t *)&modrm[1]; //Second Addressing Modes
            uint8_t scale = *sib >> 6; //SIB Scale field
            uint8_t base = *sib & 0x7; //SIB Base
            uint8_t index = (*sib >> 3) & 0x7; //SIB Index
            uint8_t factor = 0; //Scaling factor
            if (scale == 0) factor = 1;
            else if (scale == 1) factor = 2;
            else if (scale == 2) factor = 4;
            else if (scale == 3) factor = 8;
            if (high_index) index += 8;
            if (high_base) base += 8;

            if (index == 4) { //INDEX4
                //[BASE + DISP8] MODRM + 2
                address = reg_sel[base] + *((int8_t*)&modrm[2]);
                //consumed+= 2;
            } else { //Not INDEX4
                //[BASE + (INDEX * FACTOR) + DISP8] MODRM + 2
                address = reg_sel[base] + (reg_sel[index] * factor) + *((int8_t*)&modrm[2]);
                //consumed+= 2;
            }
        }
        //SIB END
        else {
            //[SRC + DISP8] MODRM + 1
            address = reg_sel[num_src] + *((int8_t*)&modrm[1]);
            //consumed++;
        }
    }

    // MOD 2
    if (mod == 2) {
        //SIB START
        if ((num_src == 4) || (num_src == 12)) { //SIB
            uint8_t *sib = (uint8_t *)&modrm[1]; //Second Addressing Modes
            uint8_t scale = *sib >> 6; //SIB Scale field
            uint8_t base = *sib & 0x7; //SIB Base
            uint8_t index = (*sib >> 3) & 0x7; //SIB Index
            uint8_t factor = 0; //Scaling factor
            if (scale == 0) factor = 1;
            else if (scale == 1) factor = 2;
            else if (scale == 2) factor = 4;
            else if (scale == 3) factor = 8;
            if (high_index) index += 8;
            if (high_base) base += 8;

            if (index == 4) { //INDEX4
                //[BASE + DISP32] MODRM + 5
                bytelen = modbyte + 5;
                address = reg_sel[base] + (saved_state64(saved_state)->isf.rip + bytelen + *((int32_t*)&modrm[2]));
                //consumed += 5;
            } else { //Not INDEX4
                //[BASE + (INDEX * FACTOR) + DISP32] MODRM + 5
                bytelen = modbyte + 5;
                address = reg_sel[base] + (reg_sel[index] * factor) + (saved_state64(saved_state)->isf.rip + bytelen + *((int32_t*)&modrm[2]));
                //consumed += 5;
            }
        }
        //SIB END
        else {
            //[SRC + DISP32] MODRM + 4
            bytelen = modbyte + 4;
            address = reg_sel[num_src] + (saved_state64(saved_state)->isf.rip + bytelen + *((int32_t*)&modrm[1]));
            //consumed += 4;
        }
    }
    // 64-bit address
    return address;
}

uint32_t addressing32(
    x86_saved_state_t *saved_state,
    uint8_t *modrm,
    uint8_t mod,
    uint8_t num_src,
    uint8_t high_index,
    uint8_t high_base,
    uint8_t modbyte,
    uint8_t bytelen
)
{
    uint32_t address = 0;
    uint32_t reg_sel[8];
    reg_sel[0] = saved_state32(saved_state)->eax;
    reg_sel[1] = saved_state32(saved_state)->ecx;
    reg_sel[2] = saved_state32(saved_state)->edx;
    reg_sel[3] = saved_state32(saved_state)->ebx;
    reg_sel[4] = saved_state32(saved_state)->uesp;
    reg_sel[5] = saved_state32(saved_state)->ebp;
    reg_sel[6] = saved_state32(saved_state)->esi;
    reg_sel[7] = saved_state32(saved_state)->edi;

    // MOD 0
    if (mod == 0) {
        //SIB START
        if (num_src == 4) { //SIB
            uint8_t *sib = (uint8_t *)&modrm[1]; //Second Addressing Modes
            uint8_t scale = *sib >> 6; //SIB Scale field
            uint8_t base = *sib & 0x7; //SIB Base
            uint8_t index = (*sib >> 3) & 0x7; //SIB Index
            uint8_t factor = 0; //Scaling factor
            if (scale == 0) factor = 1;
            else if (scale == 1) factor = 2;
            else if (scale == 2) factor = 4;
            else if (scale == 3) factor = 8;

            if (index == 4) { //INDEX4
                if (base == 5) {
                    //[DISP32] MODRM + 5
                    address = *((uint32_t*)&modrm[2]);
                    //consumed += 5;
                } else {
                    //[BASE] MODRM + 1
                    address = reg_sel[base];
                    //consumed++;
                }
            } else { //Not INDEX4
                if (base == 5) {
                    //[DISP32 + (INDEX * FACTOR)] MODRM + 5
                    address = *((int32_t*)&modrm[2]) + (reg_sel[index] * factor);
                    //consumed += 5;
                } else {
                    //[BASE + (INDEX * FACTOR)] MODRM + 1
                    address = reg_sel[base] + (reg_sel[index] * factor);
                    //consumed++;
                }
            }
        }
        //SIB END
        else {
            if (num_src == 5) {
                //[DISP32] MODRM + 4
                address = *((uint32_t*)&modrm[1]);
                //consumed += 4;
            } else {
                //[SRC]
                address = reg_sel[num_src];
            }
        }
    }

    // MOD 1
    if (mod == 1) {
        //SIB START
        if (num_src == 4) { //SIB
            uint8_t *sib = (uint8_t *)&modrm[1]; //Second Addressing Modes
            uint8_t scale = *sib >> 6; //SIB Scale field
            uint8_t base = *sib & 0x7; //SIB Base
            uint8_t index = (*sib >> 3) & 0x7; //SIB Index
            uint8_t factor = 0; //Scaling factor
            if (scale == 0) factor = 1;
            else if (scale == 1) factor = 2;
            else if (scale == 2) factor = 4;
            else if (scale == 3) factor = 8;

            if (index == 4) { //INDEX4
                //[BASE + DISP8] MODRM + 2
                address = reg_sel[base] + *((int8_t*)&modrm[2]);
                //consumed+= 2;
            } else { //Not INDEX4
                //[BASE + (INDEX * FACTOR) + DISP8] MODRM + 2
                address = reg_sel[base] + (reg_sel[index] * factor) + *((int8_t*)&modrm[2]);
                //consumed+= 2;
            }
        }
        //SIB END
        else {
            //[SRC + DISP8] MODRM + 1
            address = reg_sel[num_src] + *((int8_t*)&modrm[1]);
            //consumed++;
        }
    }

    // MOD 2
    if (mod == 2) {
        //SIB START
        if (num_src == 4) { //SIB
            uint8_t *sib = (uint8_t *)&modrm[1]; //Second Addressing Modes
            uint8_t scale = *sib >> 6; //SIB Scale field
            uint8_t base = *sib & 0x7; //SIB Base
            uint8_t index = (*sib >> 3) & 0x7; //SIB Index
            uint8_t factor = 0; //Scaling factor
            if (scale == 0) factor = 1;
            else if (scale == 1) factor = 2;
            else if (scale == 2) factor = 4;
            else if (scale == 3) factor = 8;

            if (index == 4) { //INDEX4
                //[BASE + DISP32] MODRM + 5
                address = reg_sel[base] + *((int32_t*)&modrm[2]);
                //consumed += 5;
            } else { //Not INDEX4
                //[BASE + (INDEX * FACTOR) + DISP32] MODRM + 5
                address = reg_sel[base] + (reg_sel[index] * factor) + *((int32_t*)&modrm[2]);
                //consumed += 5;
            }
        }
        //SIB END
        else {
            //[SRC + DISP32] MODRM + 4
            address = reg_sel[num_src] + *((int32_t*)&modrm[1]);
            //consumed += 4;
        }
    }
    // 32-bit address
    return address;
}

/*********************************************************
 *** AVX 2.0 Gather Instruction Addressing             ***
 *********************************************************/
uint64_t vmaddrs(x86_saved_state_t *saved_state,
                 uint8_t *modrm,
                 uint8_t high_base,
                 XMM vaddr,
                 uint8_t ins_size
                )
{
    uint8_t mod = *modrm >> 6; // ModRM.mod
    uint64_t address = 0;
    int64_t vindex = 0;

    uint8_t *sib = (uint8_t *)&modrm[1];
    uint8_t scale = *sib >> 6;
    uint8_t base = *sib & 0x7;
    uint8_t factor = 0; //Scaling factor
    if (scale == 0) factor = 1;
    else if (scale == 1) factor = 2;
    else if (scale == 2) factor = 4;
    else if (scale == 3) factor = 8;

    if (high_base) base += 8;

    uint64_t reg_sel[16];
    reg_sel[0] = saved_state64(saved_state)->rax;
    reg_sel[1] = saved_state64(saved_state)->rcx;
    reg_sel[2] = saved_state64(saved_state)->rdx;
    reg_sel[3] = saved_state64(saved_state)->rbx;
    reg_sel[4] = saved_state64(saved_state)->isf.rsp;
    reg_sel[5] = saved_state64(saved_state)->rbp;
    reg_sel[6] = saved_state64(saved_state)->rsi;
    reg_sel[7] = saved_state64(saved_state)->rdi;
    reg_sel[8] = saved_state64(saved_state)->r8;
    reg_sel[9] = saved_state64(saved_state)->r9;
    reg_sel[10] = saved_state64(saved_state)->r10;
    reg_sel[11] = saved_state64(saved_state)->r11;
    reg_sel[12] = saved_state64(saved_state)->r12;
    reg_sel[13] = saved_state64(saved_state)->r13;
    reg_sel[14] = saved_state64(saved_state)->r14;
    reg_sel[15] = saved_state64(saved_state)->r15;

    vindex = vaddr.a64[0];

    if (mod == 0) {
        if ((base == 5) || (base == 13)) {
            address = (saved_state64(saved_state)->isf.rip + ins_size + *((int32_t*)&modrm[2])) + (vindex * factor);
        } else {
            address = reg_sel[base] + (vindex * factor);
        }
    }
    if (mod == 1) {
        address = reg_sel[base] + (vindex * factor) + *((int8_t*)&modrm[2]);
    }
    if (mod == 2) {
        address = reg_sel[base] + (vindex * factor) + (saved_state64(saved_state)->isf.rip + ins_size + *((int32_t*)&modrm[2]));
    }

    return address;
}

/**
 * Store xmm register somewhere in memory
 */
void _store_xmm (uint8_t n, XMM *where)
{
    switch (n) {
    case 0:
        storedqu_template(0, where);
        break;
    case 1:
        storedqu_template(1, where);
        break;
    case 2:
        storedqu_template(2, where);
        break;
    case 3:
        storedqu_template(3, where);
        break;
    case 4:
        storedqu_template(4, where);
        break;
    case 5:
        storedqu_template(5, where);
        break;
    case 6:
        storedqu_template(6, where);
        break;
    case 7:
        storedqu_template(7, where);
        break;
    case 8:
        storedqu_template(8, where);
        break;
    case 9:
        storedqu_template(9, where);
        break;
    case 10:
        storedqu_template(10, where);
        break;
    case 11:
        storedqu_template(11, where);
        break;
    case 12:
        storedqu_template(12, where);
        break;
    case 13:
        storedqu_template(13, where);
        break;
    case 14:
        storedqu_template(14, where);
        break;
    case 15:
        storedqu_template(15, where);
        break;
    }
}

/**
 * Load xmm register from memory
 */
void _load_xmm (uint8_t n, XMM *where)
{
    switch (n) {
    case 0:
        loaddqu_template(0, where);
        break;
    case 1:
        loaddqu_template(1, where);
        break;
    case 2:
        loaddqu_template(2, where);
        break;
    case 3:
        loaddqu_template(3, where);
        break;
    case 4:
        loaddqu_template(4, where);
        break;
    case 5:
        loaddqu_template(5, where);
        break;
    case 6:
        loaddqu_template(6, where);
        break;
    case 7:
        loaddqu_template(7, where);
        break;
    case 8:
        loaddqu_template(8, where);
        break;
    case 9:
        loaddqu_template(9, where);
        break;
    case 10:
        loaddqu_template(10, where);
        break;
    case 11:
        loaddqu_template(11, where);
        break;
    case 12:
        loaddqu_template(12, where);
        break;
    case 13:
        loaddqu_template(13, where);
        break;
    case 14:
        loaddqu_template(14, where);
        break;
    case 15:
        loaddqu_template(15, where);
        break;
    }
}

/**
 * Store mmx register somewhere in memory
 */
void _store_mmx (uint8_t n, MM *where)
{
    switch (n) {
    case 0:
        storeq_template(0, where);
        break;
    case 1:
        storeq_template(1, where);
        break;
    case 2:
        storeq_template(2, where);
        break;
    case 3:
        storeq_template(3, where);
        break;
    case 4:
        storeq_template(4, where);
        break;
    case 5:
        storeq_template(5, where);
        break;
    case 6:
        storeq_template(6, where);
        break;
    case 7:
        storeq_template(7, where);
        break;
    }
}

/**
 * Load mmx register from memory
 */
void _load_mmx (uint8_t n, MM *where)
{
    switch (n) {
    case 0:
        loadq_template(0, where);
        break;
    case 1:
        loadq_template(1, where);
        break;
    case 2:
        loadq_template(2, where);
        break;
    case 3:
        loadq_template(3, where);
        break;
    case 4:
        loadq_template(4, where);
        break;
    case 5:
        loadq_template(5, where);
        break;
    case 6:
        loadq_template(6, where);
        break;
    case 7:
        loadq_template(7, where);
        break;
    }
}

/**
 * Load Memory from XMM/YMM register
 */
void _load_maddr_from_ymm (uint64_t rmaddrs, YMM *where, uint16_t rm_size, x86_saved_state_t *state) {

    if (is_saved_state64(state)) {
        if (rm_size == 256) {
            __uint256_t M256RES = ((YMM*)where)->u256;
            *((__uint256_t*)&rmaddrs) = M256RES;
        } else if (rm_size == 128) {
            __uint128_t M128RES = ((YMM*)where)->u128[0];
            *((__uint128_t*)&rmaddrs) = M128RES;
        } else if (rm_size == 64) {
            uint64_t M64RES = ((YMM*)where)->u64[0];
            *((uint64_t*)&rmaddrs) = M64RES;
        } else if (rm_size == 32) {
            uint32_t M32RES = ((YMM*)where)->u32[0];
            *((uint32_t*)&rmaddrs) = M32RES;
        }
    } else {
        uint32_t rmaddrs32 = rmaddrs & 0xffffffff;
        if (rm_size == 256) {
            __uint256_t M256RES = ((YMM*)where)->u256;
            *((__uint256_t*)&rmaddrs32) = M256RES;
        } else if (rm_size == 128) {
            __uint128_t M128RES = ((YMM*)where)->u128[0];
            *((__uint128_t*)&rmaddrs32) = M128RES;
        } else if (rm_size == 64) {
            uint64_t M64RES = ((YMM*)where)->u64[0];
            *((uint64_t*)&rmaddrs32) = M64RES;
        } else if (rm_size == 32) {
            uint32_t M32RES = ((YMM*)where)->u32[0];
            *((uint32_t*)&rmaddrs) = M32RES;
        }
    }
}

void _load_maddr_from_xmm (uint64_t rmaddrs, XMM *where, uint16_t rm_size, x86_saved_state_t *state) {

    if (is_saved_state64(state)) {
        if (rm_size == 128) {
            __uint128_t M128RES = ((XMM*)where)->u128;
            *((__uint128_t*)&rmaddrs) = M128RES;
        } else if (rm_size == 64) {
            uint64_t M64RES = ((XMM*)where)->u64[0];
            *((uint64_t*)&rmaddrs) = M64RES;
        } else if (rm_size == 32) {
            uint32_t M32RES = ((XMM*)where)->u32[0];
            *((uint32_t*)&rmaddrs) = M32RES;
        }
    } else {
        uint32_t rmaddrs32 = rmaddrs & 0xffffffff;
        if (rm_size == 128) {
            __uint128_t M128RES = ((XMM*)where)->u128;
            *((__uint128_t*)&rmaddrs32) = M128RES;
        } else if (rm_size == 64) {
            uint64_t M64RES = ((XMM*)where)->u64[0];
            *((uint64_t*)&rmaddrs32) = M64RES;
        } else if (rm_size == 32) {
            uint32_t M32RES = ((XMM*)where)->u32[0];
            *((uint32_t*)&rmaddrs) = M32RES;
        }
    }
}

/**
 * Store VYMM Register Somewhere in Memory
 */
void _store_ymm (uint8_t n, YMM *where)
{
    YMM vymmres;
    XMM lowymm;

    switch (n) {
    case 0:
        vymmres = VYMM0;
        break;
    case 1:
        vymmres = VYMM1;
        break;
    case 2:
        vymmres = VYMM2;
        break;
    case 3:
        vymmres = VYMM3;
        break;
    case 4:
        vymmres = VYMM4;
        break;
    case 5:
        vymmres = VYMM5;
        break;
    case 6:
        vymmres = VYMM6;
        break;
    case 7:
        vymmres = VYMM7;
        break;
    case 8:
        vymmres = VYMM8;
        break;
    case 9:
        vymmres = VYMM9;
        break;
    case 10:
        vymmres = VYMM10;
        break;
    case 11:
        vymmres = VYMM11;
        break;
    case 12:
        vymmres = VYMM12;
        break;
    case 13:
        vymmres = VYMM13;
        break;
    case 14:
        vymmres = VYMM14;
        break;
    case 15:
        vymmres = VYMM15;
        break;
    }

    ((YMM*)where)->u256 = vymmres.u256;

    _store_xmm(n, &lowymm);
    ((YMM*)where)->u128[0] = lowymm.u128;

}

/**
 * Load VYMM Register From Memory
 */
void _load_ymm (uint8_t n, YMM *where)
{
    YMM vymmres;
    XMM lowymm;

    vymmres.u256 = ((YMM*)where)->u256;
    lowymm.u128 = ((YMM*)where)->u128[0];

    switch (n) {
    case 0:
        VYMM0 = vymmres;
        break;
    case 1:
        VYMM1 = vymmres;
        break;
    case 2:
        VYMM2 = vymmres;
        break;
    case 3:
        VYMM3 = vymmres;
        break;
    case 4:
        VYMM4 = vymmres;
        break;
    case 5:
        VYMM5 = vymmres;
        break;
    case 6:
        VYMM6 = vymmres;
        break;
    case 7:
        VYMM7 = vymmres;
        break;
    case 8:
        VYMM8 = vymmres;
        break;
    case 9:
        VYMM9 = vymmres;
        break;
    case 10:
        VYMM10 = vymmres;
        break;
    case 11:
        VYMM11 = vymmres;
        break;
    case 12:
        VYMM12 = vymmres;
        break;
    case 13:
        VYMM13 = vymmres;
        break;
    case 14:
        VYMM14 = vymmres;
        break;
    case 15:
        VYMM15 = vymmres;
        break;
    }

    _load_xmm (n, &lowymm);
}

/**
 * Store x64 register somewhere in memory
 */
void _store_m64 (uint8_t n, M64 *where, x86_saved_state_t *state)
{
    uint64_t M64REG = 0;
    switch (n) {
    case 0:
        M64REG = saved_state64(state)->rax;
        break;
    case 1:
        M64REG = saved_state64(state)->rcx;
        break;
    case 2:
        M64REG = saved_state64(state)->rdx;
        break;
    case 3:
        M64REG = saved_state64(state)->rbx;
        break;
    case 4:
        M64REG = saved_state64(state)->isf.rsp;
        break;
    case 5:
        M64REG = saved_state64(state)->rbp;
        break;
    case 6:
        M64REG = saved_state64(state)->rsi;
        break;
    case 7:
        M64REG = saved_state64(state)->rdi;
        break;
    case 8:
        M64REG = saved_state64(state)->r8;
        break;
    case 9:
        M64REG = saved_state64(state)->r9;
        break;
    case 10:
        M64REG = saved_state64(state)->r10;
        break;
    case 11:
        M64REG = saved_state64(state)->r11;
        break;
    case 12:
        M64REG = saved_state64(state)->r12;
        break;
    case 13:
        M64REG = saved_state64(state)->r13;
        break;
    case 14:
        M64REG = saved_state64(state)->r14;
        break;
    case 15:
        M64REG = saved_state64(state)->r15;
        break;
    }

    ((M64*)where)->u64 = M64REG;
}

/**
 * Load x64 register from memory
 */
void _load_m64 (uint8_t n, M64 *where, x86_saved_state_t *state)
{
    uint64_t M64REG = ((M64*)where)->u64;

    switch (n) {
    case 0:
        saved_state64(state)->rax = M64REG;
        break;
    case 1:
        saved_state64(state)->rcx = M64REG;
        break;
    case 2:
        saved_state64(state)->rdx = M64REG;
        break;
    case 3:
        saved_state64(state)->rbx = M64REG;
        break;
    case 4:
        saved_state64(state)->isf.rsp = M64REG;
        break;
    case 5:
        saved_state64(state)->rbp = M64REG;
        break;
    case 6:
        saved_state64(state)->rsi = M64REG;
        break;
    case 7:
        saved_state64(state)->rdi = M64REG;
        break;
    case 8:
        saved_state64(state)->r8 = M64REG;
        break;
    case 9:
        saved_state64(state)->r9 = M64REG;
        break;
    case 10:
        saved_state64(state)->r10 = M64REG;
        break;
    case 11:
        saved_state64(state)->r11 = M64REG;
        break;
    case 12:
        saved_state64(state)->r12 = M64REG;
        break;
    case 13:
        saved_state64(state)->r13 = M64REG;
        break;
    case 14:
        saved_state64(state)->r14 = M64REG;
        break;
    case 15:
        saved_state64(state)->r15 = M64REG;
        break;
    }
}

/**
 * Store x86 register somewhere in memory
 */
void _store_m32 (uint8_t n, M32 *where, x86_saved_state_t *state)
{
    uint32_t M32REG = 0;
    switch (n) {
    case 0:
        M32REG = saved_state32(state)->eax;
        break;
    case 1:
        M32REG = saved_state32(state)->ecx;
        break;
    case 2:
        M32REG = saved_state32(state)->edx;
        break;
    case 3:
        M32REG = saved_state32(state)->ebx;
        break;
    case 4:
        M32REG = saved_state32(state)->uesp;
        break;
    case 5:
        M32REG = saved_state32(state)->ebp;
        break;
    case 6:
        M32REG = saved_state32(state)->esi;
        break;
    case 7:
        M32REG = saved_state32(state)->edi;
        break;
    }

    ((M32*)where)->u32 = M32REG;
}

/**
 * Load x86 register from memory
 */
void _load_m32 (uint8_t n, M32 *where, x86_saved_state_t *state)
{
    uint32_t M32REG = ((M32*)where)->u32;
    switch (n) {
    case 0:
        saved_state32(state)->eax = M32REG;
        break;
    case 1:
        saved_state32(state)->ecx = M32REG;
        break;
    case 2:
        saved_state32(state)->edx = M32REG;
        break;
    case 3:
        saved_state32(state)->ebx = M32REG;
        break;
    case 4:
        saved_state32(state)->uesp = M32REG;
        break;
    case 5:
        saved_state32(state)->ebp = M32REG;
        break;
    case 6:
        saved_state32(state)->esi = M32REG;
        break;
    case 7:
        saved_state32(state)->edi = M32REG;
        break;
    }
}
