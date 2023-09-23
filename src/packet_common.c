#include <time.h>

#include "packet_common.h"

void pack_header(JEUX_PACKET_HEADER *jph, 
        uint8_t type,
        uint8_t id,
        uint8_t role,
        uint16_t size) {
    jph->type = type;
    jph->id = id;
    jph->role = role;
    jph->size = htons(size);

    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    uint32_t tsec = ts.tv_sec;
    uint32_t tnsec = ts.tv_nsec;

    jph->timestamp_sec = htonl(tsec);
    jph->timestamp_nsec = htonl(tnsec);
}

void unpack_header(JEUX_PACKET_HEADER *jph) {
    jph->timestamp_sec = ntohl(jph->timestamp_sec);
    jph->timestamp_nsec = ntohl(jph->timestamp_nsec);
    jph->size = ntohs(jph->size);
}

