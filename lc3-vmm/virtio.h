#ifndef _VIRTIO_H_
#define _VIRTIO_H_

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

#define __virtio64 uint16_t
#define __virtio32 uint16_t
#define __virtio16 uint16_t

enum { VRING_SIZE = 10 };

#define VRING_BUF_SIZE (VRING_SIZE * 32)

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
    uint16_t buf[0]; // only for vmm
};

void virtio_init();
int virtio_handler(uint16_t flags);
int virtio_replay();

#endif
