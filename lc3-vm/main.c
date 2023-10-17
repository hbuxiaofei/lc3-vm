#define uint16_t unsigned int
#define int16_t int

#define __virtio64 uint16_t
#define __virtio32 uint16_t
#define __virtio16 uint16_t

enum { VRING_SIZE = 10 };

#define INTERRUPT_VIRTIO 0X0100
#define DEVICE_VIRTIO    0X7FFF

struct vring_desc {
    __virtio64 addr;
    __virtio32 len;
    __virtio16 flags;
    __virtio16 next;
};

struct vring_avail {
    __virtio16 flags;
    __virtio16 idx;
};

struct vring_used {
    __virtio16 flags;
    __virtio16 idx;
};

struct vring {
    uint16_t num;
    struct vring_desc desc[VRING_SIZE];
    struct vring_avail avail;
    struct vring_used used;
};

#define VIRTIO_BLK_R 0X0001
#define VIRTIO_BLK_W 0X0002

struct virtio_blk {
    int16_t flag;
    int16_t pos;
    int16_t len;
};

int16_t virtio_find_empty()
{
    int16_t i;
    struct vring *virt_ring = (struct vring *)DEVICE_VIRTIO;

    for (i = 0; i < VRING_SIZE; i++) {
        if (virt_ring->desc[i].flags == 0) {
            return i;
        }
    }

    return (int16_t)-1;
}

void virtio_blk_read(int16_t pos, int16_t len, uint16_t *buf)
{
    struct vring *virt_ring = (struct vring *)DEVICE_VIRTIO;
    uint16_t *memory = (uint16_t *)0;
    int16_t idx;

    int16_t i, start;
    struct virtio_blk *vb;
    uint16_t *desc_addr;

    idx = virtio_find_empty();
    printf("- find empty desc idx: %d \n", idx);
    if (idx < 0) {
        printf("- read error idx: %d\n", idx);
        return;
    }

    vb = (struct virtio_blk *)(&(memory[virt_ring->desc[idx].addr]));
    virt_ring->desc[idx].flags = (uint16_t)0x01;

    vb->flag = VIRTIO_BLK_R;
    vb->pos = pos;
    vb->len = len;

    virt_ring->avail.flags = (uint16_t)0x01;
    virt_ring->avail.idx = (uint16_t)idx;

    virtio_kick();

    virtio_wait_replay();

    if (virt_ring->used.flags == (uint16_t)0x01) {
        virt_ring->used.flags = (uint16_t)0x00;

        idx = virt_ring->used.idx;
        if (virt_ring->desc[idx].flags == (uint16_t)0x01) {
            virt_ring->desc[idx].flags = (uint16_t)0x00;

            desc_addr = (uint16_t *)&(memory[virt_ring->desc[idx].addr]);

            start = sizeof(struct virtio_blk);
            for (i = 0; i < len; i++) {
                buf[i] = desc_addr[start + i];
            }
        }
    }
}

void virtio_blk_write(int16_t pos, int16_t len, uint16_t *buf)
{
    struct vring *virt_ring = (struct vring *)DEVICE_VIRTIO;
    uint16_t *memory = (uint16_t *)0;
    int16_t idx;

    int16_t i, start;
    struct virtio_blk *vb;
    uint16_t *desc_addr;

    idx = virtio_find_empty();
    printf("- find empty desc idx: %d \n", idx);
    if (idx < 0) {
        printf("- write error idx: %d\n", idx);
        return;
    }

    vb = (struct virtio_blk *)(&(memory[virt_ring->desc[idx].addr]));
    virt_ring->desc[idx].flags = (uint16_t)0x01;

    vb->flag = VIRTIO_BLK_W;
    vb->pos = pos;
    vb->len = len;

    desc_addr = (uint16_t *)&(memory[virt_ring->desc[idx].addr]);
    start = sizeof(struct virtio_blk);
    for (i = 0; i < len; i++) {
        desc_addr[start + i] = buf[i];
    }

    virt_ring->avail.flags = (uint16_t)0x01;
    virt_ring->avail.idx = (uint16_t)idx;

    virtio_kick();

    virtio_wait_replay();
}

void virtio_kick()
{
    uint16_t *virtio_flags = (uint16_t *)INTERRUPT_VIRTIO;
    *virtio_flags = (uint16_t)0x0001;
}

void virtio_wait_replay()
{
    uint16_t *virtio_flags = (uint16_t *)INTERRUPT_VIRTIO;
    while (1) {
       if (*virtio_flags == (uint16_t)0x0002) {
            break;
       }
    }
}

int main()
{
    uint16_t buf16[16];
    uint16_t *buf = &(buf16[0]);
    int16_t i;
    int16_t len = 10;

    virtio_blk_read(20, len, buf);
    printf("- read buf: %s \n", buf);

    printf("\n");
    for (i = 0; i < len; i++) {
        buf[i] = buf[i] - 3;
    }
    virtio_blk_write(10, len, buf);
    printf("\n");

    return 0;
}
