#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "protocol.h"
#include "debug.h"

static ssize_t fd_op(int fd, void *byte_ptr, size_t size, 
        ssize_t (*op_write)(int, const void *, size_t), 
        ssize_t (*op_read)(int, void *, size_t)) {
    while(size > 0) {
        ssize_t n_processed;  
        if(op_write == NULL) {
            n_processed = op_read(fd, byte_ptr, size);
            if(n_processed == 0) {
                return -1;
            }
        } else {
            n_processed = op_write(fd, byte_ptr, size);
        }
        if(n_processed == -1) {
            if(errno == EINTR) {
                n_processed = 0;
            } else {
                return -1;
            }
        }

        size -= n_processed; 
        byte_ptr += n_processed;
    }

    return 1;
}

int proto_send_packet(int fd, JEUX_PACKET_HEADER *hdr, void *data) {
    if(fd_op(fd, hdr, sizeof(JEUX_PACKET_HEADER), write, NULL) == -1) {
        return -1;
    }
    size_t sz = ntohs(hdr->size);
    if(data != NULL && sz > 0) {
        if(fd_op(fd, data, sz, write, NULL) == -1) {
            return -1;
        }
    }

    return 0;
}

int proto_recv_packet(int fd, JEUX_PACKET_HEADER *hdr, void **payloadp) {
    if(fd_op(fd, hdr, sizeof(JEUX_PACKET_HEADER), NULL, read) == -1) {
        return -1;
    }
    size_t payload_size = ntohs(hdr->size);
    if(payload_size > 0) {
        *payloadp = malloc(sizeof(uint8_t) * payload_size);
        if(fd_op(fd, *payloadp, payload_size, NULL, read) == -1) {
            return -1;
        }
    }

    return 0;
}
