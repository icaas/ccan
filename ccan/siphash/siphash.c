/* Licensed under GPL v2+ - see LICENSE file for details */

#include <ccan/endian/endian.h>

#include <string.h>

#include "siphash.h"

typedef uint64_t u64;

enum sip_index { A=0, B=2, C=1, D=3, E=4 };

#define rol(x,l) (((x) << (l)) | ((x) >> (64-(l))))

#define SIP_HALF_ROUND(a,b,c,d,L1,L2) \
    (a) += (b); \
    (c) += (d); \
    (b)  = (a) ^ rol((b),L1); \
    (d)  = (c) ^ rol((d),L2); \
    (a)  = rol((a),32);

#define SIP_ROUND(W) \
    do  { \
        SIP_HALF_ROUND((W)[A], (W)[B], (W)[C], (W)[D], 13, 16); \
        SIP_HALF_ROUND((W)[C], (W)[B], (W)[A], (W)[D], 17, 21); \
    } while(0)


static inline u64 W64(const void *p, size_t I)
{
    uint64_t x;
    memcpy(&x, (char *)p + I*sizeof(x), sizeof(x));
    return le64_to_cpu(x);
}

static void siphash_init(u64 v[5], const unsigned char key[16])
{
    v[A] = W64(key, 0) ^ UINT64_C(0x736f6d6570736575);
    v[B] = W64(key, 1) ^ UINT64_C(0x646f72616e646f6d);
    v[C] = W64(key, 0) ^ UINT64_C(0x6c7967656e657261);
    v[D] = W64(key, 1) ^ UINT64_C(0x7465646279746573);
    v[E] = 0;  /* message continuation */
}

/* Load the last 0-7 bytes of `in` and put in len & 255 */
static void siphash_epilogue(u64 *m, const unsigned char *in, size_t len)
{
    in += len & ~(size_t)7;
    *m = (u64)(len & 255) << 56;
    switch (len & 7) {
        case 7: *m |= (u64) in[6] << 48;
        case 6: *m |= (u64) in[5] << 40;
        case 5: *m |= (u64) in[4] << 32;
        case 4: *m |= (u64) in[3] << 24;
        case 3: *m |= (u64) in[2] << 16;
        case 2: *m |= (u64) in[1] << 8;
        case 1: *m |= (u64) in[0];
        case 0: ;
    }
}

u64 siphash_2_4(const void *in, size_t len, const unsigned char key[16])
{
    u64 v[5];
    size_t j;

    siphash_init(v, key);

    for (j = 0; j < len/8; j++) {
        v[E] = W64(in, j);
        v[D] ^= v[E];
        SIP_ROUND(v);
        SIP_ROUND(v);
        v[A] ^= v[E];
    }
    siphash_epilogue(&v[E], in, len);

    v[D] ^= v[E];
    SIP_ROUND(v);
    SIP_ROUND(v);
    v[A] ^= v[E];

    /* Finalize */
    v[C] ^= 0xff;
    SIP_ROUND(v);
    SIP_ROUND(v);
    SIP_ROUND(v);
    SIP_ROUND(v);

    return v[A]^v[B]^v[C]^v[D];
}

