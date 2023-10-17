#include <stdio.h>
#include <stdint.h>
#include <signal.h>
/* unix only */
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/termios.h>
#include <sys/mman.h>

#include "mem.h"
#include "virtio.h"
#include "interrupt.h"

// Registers
// LC-3 共有 10 个寄存器, 每个都是 16 位, 大部分是通用寄存器.
// - 8 个通用寄存器(R0-R7)
// - 1 个程序计数器 (PC) 寄存器
// - 1 个条件标志 (COND) 寄存器
enum
{
    R_R0 = 0,
    R_R1,
    R_R2,
    R_R3,
    R_R4,
    R_R5,
    R_R6,
    R_R7,
    R_PC, /* program counter */
    R_COND,
    R_COUNT
};

// LC-3 有两个内存映射寄存器需要实现. 它们是键盘状态寄存器 (KBSR)
// 和键盘数据寄存器 (KBDR). 键盘状态寄存器（KBSR）指示是否有按键被按下,
// 键盘数据寄存器（KBDR）则识别被按下的按键。
enum
{
    MR_KBSR = 0xFE00, /* keyboard status */
    MR_KBDR = 0xFE02  /* keyboard data */
};

// TRAP 定义
enum
{
    TRAP_GETC  = 0x20,  /* get character from keyboard, not echoed onto the terminal */
    TRAP_OUT   = 0x21,  /* output a character */
    TRAP_PUTS  = 0x22,  /* output a word string */
    TRAP_IN    = 0x23,  /* get character from keyboard, echoed onto the terminal */
    TRAP_PUTSP = 0x24,  /* output a byte string */
    TRAP_HALT  = 0x25   /* halt the program */
};

// Register Storage
uint16_t reg[R_COUNT];

// Instruction set
// LC-3 中只有 16 条指令, 每条指令长 16 位.
// 左侧 4 位存储操作码, 其余位用于存储参数.
enum
{
    OP_BR = 0, /* 0000 branch */
    OP_ADD,    /* 0001 add  */
    OP_LD,     /* 0010 load */
    OP_ST,     /* 0011 store */
    OP_JSR,    /* 0100 jump register */
    OP_AND,    /* 0101 bitwise and */
    OP_LDR,    /* 0110 load register */
    OP_STR,    /* 0111 store register */
    OP_RTI,    /* 1000 unused */
    OP_NOT,    /* 1001 bitwise not */
    OP_LDI,    /* 1010 load indirect */
    OP_STI,    /* 1011 store indirect */
    OP_JMP,    /* 1100 jump */
    OP_RES,    /* 1101 reserved (unused) */
    OP_LEA,    /* 1110 load effective address */
    OP_TRAP    /* 1111 execute trap */
};

// Condition flags
// R_COND 寄存器存储条件标志, 提供最近执行结果的信息.
// 这样程序就可以执行逻辑/循环语句, 如 if (x > 0) { ... }.
enum
{
    FL_POS = 1 << 0, /* P: positive (greater than zero) */
    FL_ZRO = 1 << 1, /* Z: zero */
    FL_NEG = 1 << 2, /* N: negative (smaller than zero) */
};

// 带符号的数值扩展
// 最高位正数填充0, 负数填充1, 以便保留原始值
uint16_t sign_extend(uint16_t x, int bit_count)
{
    if ((x >> (bit_count - 1)) & 1) {
        x |= (0xFFFF << bit_count);
    }
    return x;
}

int is_little_endian(void) {
	union {
		char c;
		int i;
	} un;

	un.i = 1;

    // 如果是小端则返回 1，如果是大端则返回 0
	return un.c;
}

// LC-3 程序是大端程序, 如果是小端程序则需要交换高低字节
uint16_t swap16(uint16_t x)
{
    return (x << 8) | (x >> 8);
}

void update_flags(uint16_t r)
{
    if (reg[r] == 0) {
        reg[R_COND] = FL_ZRO;
    } else if (reg[r] >> 15) { // 最左边的位为 1 表示负数
        reg[R_COND] = FL_NEG;
    } else {
        reg[R_COND] = FL_POS;
    }
}

void mem_write(uint16_t address, uint16_t val)
{
    mem_set(address, val);

    if (address >= INTERRUPT_START && address <= INTERRUPT_END) {
        int_handler(address);
    }

}

struct termios original_tio;

void disable_input_buffering()
{
    tcgetattr(STDIN_FILENO, &original_tio);
    struct termios new_tio = original_tio;
    new_tio.c_lflag &= ~ICANON & ~ECHO;
    tcsetattr(STDIN_FILENO, TCSANOW, &new_tio);
}

void restore_input_buffering()
{
    tcsetattr(STDIN_FILENO, TCSANOW, &original_tio);
}

uint16_t check_key()
{
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(STDIN_FILENO, &readfds);

    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;
    return select(1, &readfds, NULL, NULL, &timeout) != 0;
}

uint16_t mem_read(uint16_t address)
{
    if (address == MR_KBSR) {
        if (check_key()) {
            mem_set(MR_KBSR, 1 << 15);
            mem_set(MR_KBDR, getchar());
        } else {
            mem_set(MR_KBDR, 0);
        }
    }
    return mem_get(address);
}

void read_image_file(FILE* file)
{
    uint16_t origin;
    fread(&origin, sizeof(origin), 1, file);
    if (is_little_endian()) {
        origin = swap16(origin);
    }

    uint16_t max_read = MEMORY_MAX - origin;
    uint16_t* p = mem_addr() + origin;
    size_t read = fread(p, sizeof(uint16_t), max_read, file);

    if (is_little_endian()) {
        while (read-- > 0) {
            *p = swap16(*p);
            ++p;
        }
    }
}

int read_image(const char* image_path)
{
    FILE* file = fopen(image_path, "rb");
    if (!file) {
        return 0;
    }

    read_image_file(file);
    fclose(file);
    return 1;
}


void handle_interrupt(int signal)
{
    restore_input_buffering();
    printf("\n");
    exit(-2);
}

int main(int argc, const char* argv[])
{
    int ret = 0;
    // Load Arguments
    if (argc < 2) {
        /* show usage string */
        printf("Using: main.out [image-file1] ...\n");
        ret = 2;
        goto exit;
    }

    mem_init();
    virtio_init();
    mem_sync();

    if (!read_image(argv[1])) {
        printf("failed to load image: %s\n", argv[1]);
        ret = 1;
        goto exit;
    }

    signal(SIGINT, handle_interrupt);
    disable_input_buffering();


    // 条件标志清零, 设置 Z(zero) 标志
    reg[R_COND] = FL_ZRO;

    // 设置 PC 起始位置 0x3000
    enum { PC_START = 0x3000 };
    reg[R_PC] = PC_START;

    int running = 1;
    while (running) {
        // FETCH 取指令
        uint16_t instr = mem_read(reg[R_PC]++);
        uint16_t op = instr >> 12; /* 左移12位, 取操作码 */

        // printf(">>> op: 0x%x\n", op);
        switch (op) {
            // 两个变量相加（+）
            // ADD DR,SR1,SR2 或者 ADD DR,SR1,imm
            case OP_ADD:
                {
                    // 目的寄存器 (DR)
                    uint16_t r0 = (instr >> 9) & 0x7;
                    // 源寄存器1 (SR1)
                    uint16_t r1 = (instr >> 6) & 0x7;
                    // 立即数标志
                    uint16_t imm_flag = (instr >> 5) & 0x1;

                    if (imm_flag == 0) {
                        uint16_t r2 = instr & 0x7;
                        reg[r0] = reg[r1] + reg[r2];
                    } else {
                        uint16_t imm5 = sign_extend(instr & 0x1F, 5);
                        reg[r0] = reg[r1] + imm5;
                    }
                    update_flags(r0);
                }
                break;
            case OP_AND:
                {
                    uint16_t r0 = (instr >> 9) & 0x7;
                    uint16_t r1 = (instr >> 6) & 0x7;
                    uint16_t imm_flag = (instr >> 5) & 0x1;

                    if (imm_flag) {
                        uint16_t imm5 = sign_extend(instr & 0x1F, 5);
                        reg[r0] = reg[r1] & imm5;
                    } else {
                        uint16_t r2 = instr & 0x7;
                        reg[r0] = reg[r1] & reg[r2];
                    }
                    update_flags(r0);
                }
                break;
            case OP_NOT:
                {
                    uint16_t r0 = (instr >> 9) & 0x7;
                    uint16_t r1 = (instr >> 6) & 0x7;

                    reg[r0] = ~reg[r1];
                    update_flags(r0);
                }
                break;
            case OP_BR:
                {
                    uint16_t n_flag = (instr >> 11) & 0x1;
                    uint16_t z_flag = (instr >> 10) & 0x1;
                    uint16_t p_flag = (instr >> 9) & 0x1;
                    uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);

                    if ((n_flag && (reg[R_COND] & FL_NEG)) ||
                            (z_flag && (reg[R_COND] & FL_ZRO)) ||
                            (p_flag && (reg[R_COND] & FL_POS))) {
                        reg[R_PC] += pc_offset;
                    }
                }
                break;
            case OP_JMP:
                {
                    uint16_t r1 = (instr >> 6) & 0x7;
                    reg[R_PC] = reg[r1];
                }
                break;
            case OP_JSR:
                {
                    uint16_t long_flag = (instr >> 11) & 1;
                    if (long_flag) {
                        reg[R_R7] = reg[R_PC];
                        uint16_t long_pc_offset = sign_extend(instr & 0x7FF, 11);
                        reg[R_PC] += long_pc_offset;  /* JSR 直接跳转 */
                    } else {
                        uint16_t tmp = reg[(instr >> 6) & 0x7];
                        reg[R_R7] = reg[R_PC];
                        reg[R_PC] = tmp; /* JSRR 寄存器间接跳转 */
                    }
                }
                break;
            case OP_LD:
                {
                    uint16_t r0 = (instr >> 9) & 0x7;
                    uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
                    reg[r0] = mem_read(reg[R_PC] + pc_offset);
                    update_flags(r0);
                }
                break;
            case OP_LDI:
                {
                    // 目的寄存器 (DR)
                    uint16_t r0 = (instr >> 9) & 0x7;
                    // PC偏移 9bit
                    uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
                    // 将 pc_offset 加到 PC 上, 然后查看该内存位置获取最终地址
                    reg[r0] = mem_read(mem_read(reg[R_PC] + pc_offset));
                    update_flags(r0);
                }
                break;
            case OP_LDR:
                {
                    uint16_t r0 = (instr >> 9) & 0x7;
                    uint16_t r1 = (instr >> 6) & 0x7;
                    uint16_t offset = sign_extend(instr & 0x3F, 6);
                    reg[r0] = mem_read(reg[r1] + offset);
                    update_flags(r0);
                }
                break;
            case OP_LEA:
                {
                    uint16_t r0 = (instr >> 9) & 0x7;
                    uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
                    reg[r0] = reg[R_PC] + pc_offset;
                    update_flags(r0);
                }
                break;
            case OP_ST:
                {
                    uint16_t r0 = (instr >> 9) & 0x7;
                    uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
                    mem_write(reg[R_PC] + pc_offset, reg[r0]);
                }
                break;
            case OP_STI:
                {
                    uint16_t r0 = (instr >> 9) & 0x7;
                    uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
                    mem_write(mem_read(reg[R_PC] + pc_offset), reg[r0]);
                }
                break;
            case OP_STR:
                {
                    uint16_t r0 = (instr >> 9) & 0x7;
                    uint16_t r1 = (instr >> 6) & 0x7;
                    uint16_t offset = sign_extend(instr & 0x3F, 6);
                    mem_write(reg[r1] + offset, reg[r0]);
                }
                break;
            case OP_TRAP:
                reg[R_R7] = reg[R_PC];

                // printf(">>> trap: 0x%x \n", instr & 0xff);
                switch (instr & 0xFF)
                {
                    case TRAP_GETC:
                        reg[R_R0] = (uint16_t)getchar();
                        update_flags(R_R0);
                        break;
                    case TRAP_OUT:
                        putc((char)reg[R_R0], stdout);
                        fflush(stdout);
                        break;
                    case TRAP_PUTS:
                        {
                            // 16 bit 表示一个字符
                            uint16_t* c = mem_addr() + reg[R_R0];
                            while (*c) {
                                putc((char)*c, stdout);
                                ++c;
                            }
                            fflush(stdout);
                        }
                        break;
                    case TRAP_IN:
                        {
                            char c = getchar();
                            putc(c, stdout);
                            fflush(stdout);
                            reg[R_R0] = (uint16_t)c;
                            update_flags(R_R0);
                        }
                        break;
                    case TRAP_PUTSP:
                        {
                            uint16_t* c = mem_addr() + reg[R_R0];
                            while (*c) {
                                char char1 = (*c) & 0xFF;
                                putc(char1, stdout);
                                char char2 = (*c) >> 8;
                                if (char2) {
                                    putc(char2, stdout);
                                }
                                ++c;
                            }
                            fflush(stdout);
                        }
                        break;
                    case TRAP_HALT:
                        fflush(stdout);
                        running = 0;
                        break;
                }
                break;
            case OP_RTI:
            case OP_RES:
                abort(); /* RTI RES 未使用 */
            default:
                printf("error: bad op code\n");
                break;
        }
    }
    restore_input_buffering();

exit:
    mem_destroy();

    return ret;
}

