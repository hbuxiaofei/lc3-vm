#include "virtio.h"
#include "mem.h"

#define VIRTIO_IDX DEVICE_VIRTIO

void virtio_init()
{
    uint16_t *memory = mem_addr();
    uint16_t *virtio_memory = (uint16_t *)(&(memory[VIRTIO_IDX]));
    struct vring *virt_ring = (struct vring *)virtio_memory;

    int i;

    virt_ring->num = VRING_SIZE;
    for (i = 0; i < VRING_SIZE; i++) {
        virt_ring->desc[i].addr = VIRTIO_IDX + sizeof(struct vring) / 2 + i;
        virt_ring->desc[i].len = VRING_BUF_SIZE / VRING_SIZE;
        virt_ring->desc[i].flags = 0;
        virt_ring->desc[i].next = i + 1;
    }
    virt_ring->desc[VRING_SIZE - 1].next = 0;

    virt_ring->avail.idx = 0;
    virt_ring->avail.flags = 0;

    virt_ring->used.idx = 0;
    virt_ring->used.flags = 0;

    printf(">>> vring size:%d  addr: 0x%x\n", sizeof(struct vring), (uint16_t *)virt_ring - memory);
}

int virtio_handler(uint16_t flags)
{
    uint16_t *memory = mem_addr();
    uint16_t *virtio_memory = (uint16_t *)(&(memory[VIRTIO_IDX]));
    struct vring *virt_ring = (struct vring *)virtio_memory;

    uint16_t avail_idx, i;
    uint16_t *buf16 = NULL;
    struct virtio_blk *vb;

    printf(">>> virtio handler: 0x%x \n", flags);

    if (flags & 0x01) {
        if (virt_ring->avail.flags & 0x01) {
            virt_ring->avail.flags = 0;

            avail_idx = virt_ring->avail.idx;

            if (virt_ring->desc[avail_idx].flags & 0x01) {
                virt_ring->desc[avail_idx].flags = 0;

                vb = (struct virtio_blk *)&(memory[virt_ring->desc[avail_idx].addr]);
                if (vb->flag == VIRTIO_BLK_R) {
                    vb->flag = 0;

                    printf(">>> read pos: %d len: %d \n", vb->pos, vb->len);
                    for (i = 0; i < vb->len; i++) {
                        vb->buf[i] = '0' + vb->pos + i;
                    }

                    virt_ring->used.flags = 0x01;
                    virt_ring->used.idx = avail_idx;

                    virt_ring->desc[avail_idx].flags = 1;
                } else if (vb->flag == VIRTIO_BLK_W) {
                    vb->flag = 0;

                    printf(">>> write pos:%d len:%d \n", vb->pos, vb->len);
                    printf(">>> buf: ");
                    for (i = 0; i < vb->len; i++) {
                        printf("%c", vb->buf[i]);
                    }
                }
            }
        }

        return 0;
    }

    return -1;
}

int virtio_replay()
{
    uint16_t *memory = mem_addr();
    memory[INTERRUPT_VIRTIO] = 0x0002;
}

