#include "mem.h"

static uint16_t *memory = NULL;  /* 65536 locations */

void mem_init()
{
    if (memory)
        return;

    memory = (uint16_t *)malloc(MEMORY_MAX * sizeof(uint16_t));
}

void mem_destroy()
{
    if (memory) {
        free(memory);
    }
}

void mem_set(uint16_t address, uint16_t val)
{
    memory[address] = val;
}

uint16_t mem_get(uint16_t address)
{
    return memory[address];
}

uint16_t *mem_addr()
{
    return memory;
}

int mem_sync()
{
    if (!memory)
        return -1;
    return msync((void *)memory, ((int)MEMORY_MAX) * 2, MS_SYNC | MS_INVALIDATE);
}
