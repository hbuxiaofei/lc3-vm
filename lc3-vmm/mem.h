#ifndef _MEM_H_
#define _MEM_H_

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

// Memory Storage
// LC-3 有 65,536 个寻址空间(16 位无符号整数 2^16)
// 每个地址存储一个 16 位值, 这意味着它总共有 128KB 字节的存储空间.
#define MEMORY_MAX (1 << 16)

// x0000 − x00FF Trap Vector Table
// x0100 − x01FF Interrupt Vector Table
// x0200 − x2FFF OS and Supervisor Stack
// x3000 − xFDFF User Program Area
// xFE00 − xFFFF Device Register Addresses
#define INTERRUPT_START  0X0100
#define INTERRUPT_VIRTIO 0X0100
#define INTERRUPT_END    0X01FF

#define DEVICE_START  0X7FFF
#define DEVICE_VIRTIO 0X7FFF
#define DEVICE_END    0XFFFF

void mem_init();
void mem_destroy();
void mem_set(uint16_t address, uint16_t val);
uint16_t mem_get(uint16_t address);
uint16_t *mem_addr();
int mem_sync();

#endif

