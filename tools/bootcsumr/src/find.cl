typedef uint  uint32_t;
typedef ulong uint64_t;

#define Y_SHIFT 16

static inline uint32_t checksum_helper (uint32_t op1, uint32_t op2, uint32_t op3) {
    if (op2 == 0) {
        op2 = op3;
    }

#if 0
    int low_mult = ((uint64_t)op1 * (uint64_t)op2) & 0x00000000FFFFFFFF;
    int high_mult = (((uint64_t)op1 * (uint64_t)op2) & 0xFFFFFFFF00000000) >> 32;
    if (high_mult - low_mult == 0) {
        return low_mult;
    } else {
        return high_mult - low_mult;
    }
#else
    int low_mult = op1 * op2;
    int high_mult = mul_hi(op1, op2);
    int diff = high_mult - low_mult;
    return (diff) ? diff : low_mult;
#endif
}

static inline void first(uint16 *frame, uint32_t prev_inst, uint32_t bcode_inst, uint32_t loop_count) {
    frame->s0 += checksum_helper(0x3EF - loop_count, bcode_inst, loop_count);
    frame->s1 = checksum_helper(frame->s1, bcode_inst, loop_count);
    frame->s2 ^= bcode_inst;
    frame->s3 += checksum_helper(bcode_inst + 5, 0x6c078965, loop_count);
    if (prev_inst < bcode_inst) {
        frame->s9 = checksum_helper(frame->s9, bcode_inst, loop_count);
    } else {
        frame->s9 += bcode_inst;
    }
#if 0
    frame->s4 += ((bcode_inst << (0x20 - (prev_inst & 0x1f))) | (bcode_inst >> (prev_inst & 0x1f)));
    frame->s7 = checksum_helper(frame->s7, ((bcode_inst >> (0x20 - (prev_inst & 0x1f))) | (bcode_inst << (prev_inst & 0x1f))), loop_count);
#else
    frame->s4 += rotate(bcode_inst, (0x20 - prev_inst));
    frame->s7 = checksum_helper(frame->s7, rotate(bcode_inst, prev_inst), loop_count);
#endif
    if (bcode_inst < frame->s6) {
        frame->s6 = (bcode_inst + loop_count) ^ (frame->s3 + frame->s6);
    } else {
        frame->s6 = (frame->s4 + bcode_inst) ^ frame->s6;
    }
#if 0
    frame->s5 += (bcode_inst >> (0x20 - (prev_inst >> 27))) | (bcode_inst << (prev_inst >> 27));
    frame->s8 = checksum_helper(frame->s8, (bcode_inst << (0x20 - (prev_inst >> 27))) | (bcode_inst >> (prev_inst >> 27)), loop_count);
#else
    frame->s5 += rotate(bcode_inst, (prev_inst >> 27));
    frame->s8 = checksum_helper(frame->s8, rotate(bcode_inst, (0x20 - (prev_inst >> 27))), loop_count);
#endif
}

static inline void second(uint16 *frame, uint32_t prev_inst, uint32_t bcode_inst, uint32_t next_inst, uint32_t loop_count) {
#if 0
    uint32_t tmp1 = checksum_helper(frame->sF, (bcode_inst >> (0x20 - (prev_inst >> 27))) | (bcode_inst << (prev_inst >> 27)), loop_count);
    frame->sF = checksum_helper(
        tmp1,
        (next_inst << (bcode_inst >> 27)) | (next_inst >> (0x20 - (bcode_inst >> 27))),
        loop_count
    );
    uint32_t tmp2 = ((bcode_inst << (0x20 - (prev_inst & 0x1f))) | (bcode_inst >> (prev_inst & 0x1f)));
#else
    uint32_t tmp1 = checksum_helper(frame->sF, rotate(bcode_inst, (prev_inst >> 27)), loop_count);
    frame->sF = checksum_helper(tmp1, rotate(next_inst, (bcode_inst >> 27)), loop_count);
    uint32_t tmp2 = rotate(bcode_inst, (0x20 - prev_inst));
#endif
    uint32_t tmp3 = checksum_helper(frame->sE, tmp2, loop_count); // v0 at 1384
#if 0
    uint32_t tmp4 = checksum_helper(tmp3, (next_inst >> (bcode_inst & 0x1f)) | (next_inst << (0x20 - (bcode_inst & 0x1f))), loop_count); // v0 at 13a4
    frame->sE = tmp4;
    frame->sD += ((bcode_inst >> (bcode_inst & 0x1f)) | (bcode_inst << (0x20 - (bcode_inst & 0x1f)))) + ((next_inst >> (next_inst & 0x1f)) | (next_inst << (0x20 - (next_inst & 0x1f))));
#else
    uint32_t tmp4 = checksum_helper(tmp3, rotate(next_inst, (0x20 - bcode_inst)), loop_count); // v0 at 13a4
    frame->sE = tmp4;
    frame->sD += rotate(bcode_inst, (0x20 - bcode_inst)) + rotate(next_inst, (0x20 - next_inst));
#endif
    frame->sA = checksum_helper(frame->sA + bcode_inst, next_inst, loop_count);
    frame->sB = checksum_helper(frame->sB ^ bcode_inst, next_inst, loop_count);
    frame->sC += (frame->s8 ^ bcode_inst);
}

static inline void sframe23(uint32_t *sframe2, uint32_t *sframe3, uint32_t frame_word, uint32_t i) {
    if (((frame_word & 0x02) >> 1) == (frame_word & 0x01)) {
        *sframe2 += frame_word;
    } else {
        *sframe2 = checksum_helper(*sframe2, frame_word, i);
    }

    if ((frame_word & 0x01) == 1) {
        *sframe3 ^= frame_word;
    } else {
        *sframe3 = checksum_helper(*sframe3, frame_word, i);
    }
}

static inline void sframe01(uint32_t *sframe0, uint32_t *sframe1, uint32_t frame_word) {
#if 0
    *sframe0 += ((frame_word << ((0x20 - frame_word) & 0x1f)) | frame_word >> (frame_word & 0x1f));
#else
    *sframe0 += rotate(frame_word, (0x20 - frame_word));
#endif

    if (frame_word < *sframe0) {
        *sframe1 += frame_word;
    } else {
        *sframe1 = checksum_helper(*sframe1, frame_word, 0);
    }
}

__kernel void find(
    __global uint32_t *result,
    __constant uint32_t *preframe,
    const uint64_t desired_checksum,
    const uint32_t prev_inst,
    const uint32_t bcode_inst
)
{
    // Copy preframe over
    uint16 frame = (uint16)(
        preframe[0], preframe[1], preframe[2], preframe[3],
        preframe[4], preframe[5], preframe[6], preframe[7],
        preframe[8], preframe[9], preframe[10], preframe[11],
        preframe[12], preframe[13], preframe[14], preframe[15]
    );

    uint32_t word = (get_global_id(1) << Y_SHIFT) + get_global_id(0);

    // Calculate frame
    {
        // Frame calculations for 0x3ee
        second(&frame, prev_inst, bcode_inst, word, 0x3ee + 1);
        // Frame calculations for 0x3ef
        first(&frame, bcode_inst, word, 0x3ef + 1);

        // Calculates sframe

        // First calculate sframe 2 and 3, they are independent and allow for faster checking
        // Every value in sframe is initialized to frame[0]
        uint32_t sframe2 = frame.s0;
        uint32_t sframe3 = frame.s0;
        sframe23(&sframe2, &sframe3, frame.s0, 0);
        sframe23(&sframe2, &sframe3, frame.s1, 1);
        sframe23(&sframe2, &sframe3, frame.s2, 2);
        sframe23(&sframe2, &sframe3, frame.s3, 3);
        sframe23(&sframe2, &sframe3, frame.s4, 4);
        sframe23(&sframe2, &sframe3, frame.s5, 5);
        sframe23(&sframe2, &sframe3, frame.s6, 6);
        sframe23(&sframe2, &sframe3, frame.s7, 7);
        sframe23(&sframe2, &sframe3, frame.s8, 8);
        sframe23(&sframe2, &sframe3, frame.s9, 9);
        sframe23(&sframe2, &sframe3, frame.sA, 10);
        sframe23(&sframe2, &sframe3, frame.sB, 11);
        sframe23(&sframe2, &sframe3, frame.sC, 12);
        sframe23(&sframe2, &sframe3, frame.sD, 13);
        sframe23(&sframe2, &sframe3, frame.sE, 14);
        sframe23(&sframe2, &sframe3, frame.sF, 15);
        uint32_t high_part = (sframe2 ^ sframe3);
        if (high_part != (desired_checksum & 0xffffffff)) {
            return;
        }

        // If high part of checksum matches continue to calculate sframe 1 and 0
        // Every value in sframe is initialized to frame[0]
        uint32_t sframe0 = frame.s0;
        uint32_t sframe1 = frame.s0;
        sframe01(&sframe0, &sframe1, frame.s0);
        sframe01(&sframe0, &sframe1, frame.s1);
        sframe01(&sframe0, &sframe1, frame.s2);
        sframe01(&sframe0, &sframe1, frame.s3);
        sframe01(&sframe0, &sframe1, frame.s4);
        sframe01(&sframe0, &sframe1, frame.s5);
        sframe01(&sframe0, &sframe1, frame.s6);
        sframe01(&sframe0, &sframe1, frame.s7);
        sframe01(&sframe0, &sframe1, frame.s8);
        sframe01(&sframe0, &sframe1, frame.s9);
        sframe01(&sframe0, &sframe1, frame.sA);
        sframe01(&sframe0, &sframe1, frame.sB);
        sframe01(&sframe0, &sframe1, frame.sC);
        sframe01(&sframe0, &sframe1, frame.sD);
        sframe01(&sframe0, &sframe1, frame.sE);
        sframe01(&sframe0, &sframe1, frame.sF);
        uint32_t low_part = (checksum_helper(sframe0, sframe1, 0x10) & 0xffff);
        if (low_part != (desired_checksum >> 32)) {
            return;
        }

        // found
        // TODO: atomic store!
        result[0] = 1;
        result[1] = word;
        return;
    }
}
