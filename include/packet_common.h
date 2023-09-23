#ifndef PACKET_COMMON_H
#define PACKET_COMMON_H

#include "protocol.h"

extern void pack_header(JEUX_PACKET_HEADER *jph, 
        uint8_t type, 
        uint8_t id,
        uint8_t role,
        uint16_t size);

extern void unpack_header(JEUX_PACKET_HEADER *jph);

#endif /* PACKET_COMMON_H */
