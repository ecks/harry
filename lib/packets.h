#ifndef PACKETS_H
#define PACKETS_H

#define IP_FMT "%"PRIu8".%"PRIu8".%"PRIu8".%"PRIu8
#define IP_ARGS(ip)                             \
        ((void) (ip)[0], ((uint8_t *) ip)[0]),  \
        ((uint8_t *) ip)[1],                    \
        ((uint8_t *) ip)[2],                    \
        ((uint8_t *) ip)[3]

#define IP6_FMT "%"PRIx16".%"PRIx16".%"PRIx16".%"PRIx16"%"PRIx16".%"PRIx16".%"PRIx16".%"PRIx16
#define IP6_ARGS(ip)                             \
        ((void) (ip)[0], ((uint16_t *) ip)[0]),  \
        ((uint16_t *) ip)[1],                    \
        ((uint16_t *) ip)[2],                    \
        ((uint16_t *) ip)[3],                    \
        ((uint16_t *) ip)[4],                    \
        ((uint16_t *) ip)[5],                    \
        ((uint16_t *) ip)[6],                    \
        ((uint16_t *) ip)[7]
#endif
