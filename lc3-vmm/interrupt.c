#include "mem.h"
#include "interrupt.h"
#include "virtio.h"

void int_handler(uint16_t entry)
{
    uint16_t flags;
    uint16_t *memory = mem_addr();

    printf(">>> int entry: 0x%x \n", entry);

    if (entry == INTERRUPT_VIRTIO) {
        flags = memory[entry];
        if (!virtio_handler(flags)) {
            virtio_replay();
        }
    }
}

