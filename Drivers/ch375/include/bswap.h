#include <stdint.h>

#include "config.h"

static inline uint16_t bswap16(uint16_t x)
{
    return (((x & 0x00ff) << 8) |
            ((x & 0xff00) >> 8));
}

static inline uint32_t bswap32(uint32_t x)
{
    return (((x & 0x000000ffU) << 24) |
            ((x & 0x0000ff00U) <<  8) |
            ((x & 0x00ff0000U) >>  8) |
            ((x & 0xff000000U) >> 24));
}

static inline uint64_t bswap64(uint64_t x)
{
    return (((x & 0x00000000000000ffULL) << 56) |
            ((x & 0x000000000000ff00ULL) << 40) |
            ((x & 0x0000000000ff0000ULL) << 24) |
            ((x & 0x00000000ff000000ULL) <<  8) |
            ((x & 0x000000ff00000000ULL) >>  8) |
            ((x & 0x0000ff0000000000ULL) >> 24) |
            ((x & 0x00ff000000000000ULL) >> 40) |
            ((x & 0xff00000000000000ULL) >> 56));
}

#ifdef HOST_WORDS_BIGENDIAN
#define le16_to_cpu(x) bswap16(x)
#define le32_to_cpu(x) bswap32(x)
#define le64_to_cpu(x) bswap64(x)
#define cpu_to_le16(x) bswap16(x)
#define cpu_to_le32(x) bswap32(x)
#define cpu_to_le64(x) bswap64(x)

#define be16_to_cpu(x) (x)
#define be32_to_cpu(x) (x)
#define be64_to_cpu(x) (x)
#define cpu_to_be16(x) (x)
#define cpu_to_be32(x) (x)
#define cpu_to_be64(x) (x)

#else /* HOST_WORDS_LETTERENDIAN */
#define le16_to_cpu(x) (x)
#define le32_to_cpu(x) (x)
#define le64_to_cpu(x) (x)
#define cpu_to_le16(x) (x)
#define cpu_to_le32(x) (x)
#define cpu_to_le64(x) (x)

#define be16_to_cpu(x) bswap16(x)
#define be32_to_cpu(x) bswap32(x)
#define be64_to_cpu(x) bswap64(x)
#define cpu_to_be16(x) bswap16(x)
#define cpu_to_be32(x) bswap32(x)
#define cpu_to_be64(x) bswap64(x)

#endif /* HOST_WORDS_BIGENDIAN */
