#pragma OPENCL EXTENSION cl_khr_byte_addressable_store : enable

#define NULL 0

#define W 1024
#define H 1024
#define LW 16
#define LH 16

static inline uint32_t checksum_helper (uint32_t op1, uint32_t op2, uint32_t op3) {
    int low_mult;
    int high_mult;

    if (op2 == 0) {
        op2 = op3;
    }

    low_mult = ((uint64_t)op1 * (uint64_t)op2) & 0x00000000FFFFFFFF;
    high_mult = (((uint64_t)op1 * (uint64_t)op2) & 0xFFFFFFFF00000000) >> 32;

    if (high_mult - low_mult == 0) {
        return low_mult;
    } else {
        return high_mult - low_mult;
    }
}

static inline void first(uint32_t frame[], uint32_t prev_inst, uint32_t bcode_inst, uint32_t loop_count) {
    frame[0] += checksum_helper(0x3EF - loop_count, bcode_inst, loop_count);
    frame[1] = checksum_helper(frame[1], bcode_inst, loop_count);
    frame[2] ^= bcode_inst;
    frame[3] += checksum_helper(bcode_inst + 5, 0x6c078965, loop_count);
    if (prev_inst < bcode_inst) {
        frame[9] = checksum_helper(frame[9], bcode_inst, loop_count);
    } else {
        frame[9] += bcode_inst;
    }
    frame[4] += ((bcode_inst << (0x20 - (prev_inst & 0x1f))) | (bcode_inst >> (prev_inst & 0x1f)));
    frame[7] = checksum_helper(frame[7], ((bcode_inst >> (0x20 - (prev_inst & 0x1f))) | (bcode_inst << (prev_inst & 0x1f))), loop_count);
    if (bcode_inst < frame[6]) {
        frame[6] = (bcode_inst + loop_count) ^ (frame[3] + frame[6]);
    } else {
        frame[6] = (frame[4] + bcode_inst) ^ frame[6];
    }
    frame[5] += (bcode_inst >> (0x20 - (prev_inst >> 27))) | (bcode_inst << (prev_inst >> 27));
    frame[8] = checksum_helper(frame[8], (bcode_inst << (0x20 - (prev_inst >> 27))) | (bcode_inst >> (prev_inst >> 27)), loop_count);
}

static inline void second(uint32_t frame[], uint32_t prev_inst, uint32_t bcode_inst, uint32_t next_inst, uint32_t loop_count) {
    uint32_t tmp1 = checksum_helper(frame[15], (bcode_inst >> (0x20 - (prev_inst >> 27))) | (bcode_inst << (prev_inst >> 27)), loop_count);
    frame[15] = checksum_helper(
        tmp1,
        (next_inst << (bcode_inst >> 27)) | (next_inst >> (0x20 - (bcode_inst >> 27))),
        loop_count
    );
    uint32_t tmp2 = ((bcode_inst << (0x20 - (prev_inst & 0x1f))) | (bcode_inst >> (prev_inst & 0x1f)));
    uint32_t tmp3 = checksum_helper(frame[14], tmp2, loop_count); // v0 at 1384
    uint32_t tmp4 = checksum_helper(tmp3, (next_inst >> (bcode_inst & 0x1f)) | (next_inst << (0x20 - (bcode_inst & 0x1f))), loop_count); // v0 at 13a4
    frame[14] = tmp4;
    frame[13] += ((bcode_inst >> (bcode_inst & 0x1f)) | (bcode_inst << (0x20 - (bcode_inst & 0x1f)))) + ((next_inst >> (next_inst & 0x1f)) | (next_inst << (0x20 - (next_inst & 0x1f))));
    frame[10] = checksum_helper(frame[10] + bcode_inst, next_inst, loop_count);
    frame[11] = checksum_helper(frame[11] ^ bcode_inst, next_inst, loop_count);
    frame[12] += (frame[8] ^ bcode_inst);
}

__kernel void find(
    __global uint32_t *frame,
    uint64_t desired_checksum,
    uint32_t prev_inst,
    uint32_t bcode_inst
)
{
    {
        // Calculate frame

        // Copy preframe over
        uint32_t frame[16] = {
            preframe[0], preframe[1], preframe[2], preframe[3],
            preframe[4], preframe[5], preframe[6], preframe[7],
            preframe[8], preframe[9], preframe[10], preframe[11],
            preframe[12], preframe[13], preframe[14], preframe[15],
        };
        // Frame calculations for 0x3ee
        second(frame, prev_inst, bcode_inst, word, 0x3ee + 1);
        // Frame calculations for 0x3ef
        first(frame, bcode_inst, word, 0x3ef + 1);

        // Calculates sframe

        // First calculate sframe 2 and 3, they are independent and allow for faster checking
        // Every value in sframe is initialized to frame[0]
        uint32_t sframe2 = frame[0];
        uint32_t sframe3 = frame[0];
        uint32_t i = 0;
        do {
            uint32_t frame_word = frame[i];

            if (((frame_word & 0x02) >> 1) == (frame_word & 0x01)) {
                sframe2 += frame_word;
            } else {
                sframe2 = checksum_helper(sframe2, frame_word, i);
            }

            if ((frame_word & 0x01) == 1) {
                sframe3 ^= frame_word;
            } else {
                sframe3 = checksum_helper(sframe3, frame_word, i);
            }
        } while (++i != 16);
        uint32_t high_part = (sframe2 ^ sframe3);
        if (high_part != (desired_checksum & 0xffffffff)) {
            continue;
        }

        // If high part of checksum matches continue to calculate sframe 1 and 0
        // Every value in sframe is initialized to frame[0]
        uint32_t sframe0 = frame[0];
        uint32_t sframe1 = frame[0];
        i = 0;
        do {
            uint32_t frame_word = frame[i];

            sframe0 += ((frame_word << ((0x20 - frame_word) & 0x1f)) | frame_word >> (frame_word & 0x1f));

            if (frame_word < sframe0) {
                sframe1 += frame_word;
            } else {
                sframe1 = checksum_helper(sframe1, frame_word, 0);
            }
        } while (++i != 16);
        uint32_t low_part = (checksum_helper(sframe0, sframe1, 0x10) & 0xffff);
        if (low_part != (desired_checksum >> 32)) {
            continue;
        }

        return true;//found = true;
        break;
    }



    /*const*/ int globalX = get_global_id(0);
    /*const*/ int globalY = get_global_id(1);
    //const int groupX = get_group_id(0);
    //const int lastGroupX = get_num_groups(0) - 1;
    /*const*/ int groupY = get_group_id(1);
    const int localX = get_local_id(0);
    const int localY = get_local_id(1);

    // -> xth
    // grpY0(0<<1): 0 1 2 3 4 5 6 7
    // grpY1(1<<1):     2 3 4 5 6 7 8 9
    // grpY2(2<<1):         4 5 6 7 8 9 10 11
    globalY += offsetY << LH_SHIFT;
    groupY += offsetY;
    const int groupX = xth - (groupY << 1);
    const int lastGroupX = (W >> LW_SHIFT) - 1;
    globalX += groupX << LW_SHIFT;
    if (groupX < 0 || groupX > lastGroupX) {
        return;
    }


    __global uchar *pgroup = yPlane + W * LH * groupY + LW * groupX;

    __local int a0[16];
    if (localX == 0) {
        a0[localY] = (globalX != 0) ? *(pgroup + W * localY - 1) + 1 : groupY;
    }

    __local int b1c2c3d4[16];
    if (globalY != 0) {
        if (localX < 4 && localY == 0) {
            uint *p;
            p = pgroup - (W + 4);
            p += localX << 1;
            uint add2110 = 2 - ((localX + 1) >> 1); // 00:2, 01:1, 10:1, 11:0
            if (globalX == 0) {
                p += 2;
                add2110 = 1;
            }
            if (localX == 3 && groupX == lastGroupX) {
                p -= 2;
                add2110 = 1;
            }
            uint value0123 = *p;
            b1c2c3d4[(localX << 2) + 0] = (value0123 >> 24) + add2110;
            b1c2c3d4[(localX << 2) + 1] = ((value0123 >> 16) & 0xff) + add2110;
            b1c2c3d4[(localX << 2) + 2] = ((value0123 >>  8) & 0xff) + add2110;
            b1c2c3d4[(localX << 2) + 3] = (value0123 & 0xff) + add2110;
        }
    } else {
        if (localY == 0) {
            b1c2c3d4[localX] = groupX;
        }
    }

    barrier(CLK_LOCAL_MEM_FENCE);
    //mem_fence(CLK_GLOBAL_MEM_FENCE|CLK_LOCAL_MEM_FENCE);

    uchar value = (a0[localY] + b1c2c3d4[localX]) >> 1;
    yPlane[W * globalY + globalX] = value;
}
