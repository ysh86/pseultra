/*
 * pseultra/tools/bootcsum/src/bootcsumr.c
 * PIFrom ipl2 checksum reverser
 *
 * (C) pseudophpt 2018
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// for 6102
#define MAGIC 0x95DACFDC

bool find_collision (uint32_t *bcode, uint64_t desired_checksum);

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

int main (int argc, char *argv[]) {
    // If arguments not adequate
    if (argc != 3) {
        fprintf(stderr, "Usage: bootcsumr <rom file> <checksum to search for>\n");
        return -1;
    }

    size_t nwords = 0x1000 / sizeof(uint32_t);
    uint32_t rom[nwords];

    FILE* fd = fopen(argv[1], "rb");
    size_t n = fread(rom, sizeof(uint32_t), nwords, fd);
    fclose(fd);
    if (n != nwords) {
        return -1;
    }
    // LE to BE
    for (int i = 0; i < nwords; i++) {
        uint32_t le = rom[i];
        uint32_t be = ((le & 0xff) << 24) | ((le & 0xff00) << 8) | ((le & 0xff0000) >> 8) | ((le & 0xff000000) >> 24);
        rom[i]  = be;
    }

    uint64_t checksum = strtoll(argv[2], NULL, 0);

    bool found = find_collision(&rom[0x10], checksum);
    if (found) {
        return 0;
    }

    fprintf(stderr, "NOT found!!\n");
    return -1;
}

/*
 * Try to find checksum collision
 */
bool find_collision (uint32_t *bcode, uint64_t desired_checksum) {
    // Generate frame. This is done earlier in IPC2
    // Pre-calculated frame, up to what changes
    uint32_t preframe[16];
    uint32_t magic = MAGIC ^ bcode[0];
    for (int i = 0; i < 16; i ++) {
        preframe[i] = magic;
    };

    // Calculate pre-frame
    // Loop start, 11E8 - 11FC
    bcode[-1] = bcode[0]; // 1st prev
    for (int32_t i = 0; i < 0x3ed; i++) {
        first(preframe, bcode[i-1], bcode[i], i+1);
        second(preframe, bcode[i-1], bcode[i], bcode[i+1], i+1);
    }

    // Store starting hword into bootcode
    uint16_t starthword = 0;
    bcode[0x3ee] = (bcode[0x3ee] & 0xffff0000) | starthword;
    // Frame calculations for 0x3ed
    first(preframe, bcode[0x3ec], bcode[0x3ed], 0x3ed + 1);
    second(preframe, bcode[0x3ec], bcode[0x3ed], bcode[0x3ee], 0x3ed + 1);

    // Now let's try everything for the last word
    // Current frame being used to test
    uint32_t frame[16];
    uint32_t word = 0;
    do {
        // Copy preframe over
        memcpy(&frame, &preframe, sizeof(frame));

        // Write word to end of bootcode
        //bcode[0x3ef] = word;

        // Calculate frame
        uint32_t prev_inst = bcode[0x3ed];
        uint32_t bcode_inst = bcode[0x3ee];
        uint32_t next_inst = word;
        // Frame calculations for 0x3ee
        first(frame, prev_inst, bcode_inst, 0x3ee + 1);
        second(frame, prev_inst, bcode_inst, next_inst, 0x3ee + 1);
        // Frame calculations for 0x3ef
        first(frame, bcode_inst, next_inst, 0x3ef + 1);

        // Calculates sframe
        // Every value in sframe is initialized to frame[0]
        uint32_t sframe[4] = {frame[0],frame[0],frame[0],frame[0]};

        // First calculate sframe 2 and 3, they are independent and allow for faster checking
        uint32_t i = 0;
        do {
            uint32_t frame_word = frame[i];

            if (((frame_word & 0x02) >> 1) == (frame_word & 0x01)) {
                sframe[2] += frame_word;
            } else {
                sframe[2] = checksum_helper(sframe[2], frame_word, i);
            }

            if ((frame_word & 0x01) == 1) {
                sframe[3] ^= frame_word;
            } else {
                sframe[3] = checksum_helper(sframe[3], frame_word, i);
            }
        } while (++i != 16);
        uint32_t high_part = (sframe[2] ^ sframe[3]);
        if (high_part != (desired_checksum & 0xffffffff)) {
            continue;
        }

        // If high part of checksum matches continue to calculate sframe 1 and 0
        i = 0;
        do {
            uint32_t frame_word = frame[i];

            sframe[0] += ((frame_word << ((0x20 - frame_word) & 0x1f)) | frame_word >> (frame_word & 0x1f));

            if (frame_word < sframe[0]) {
                sframe[1] += frame_word;
            } else {
                sframe[1] = checksum_helper(sframe[1], frame_word, 0);
            }
        } while (++i != 16);
        uint32_t low_part = (checksum_helper(sframe[0], sframe[1], 0x10) & 0xffff);
        if (low_part != (desired_checksum >> 32)) {
            continue;
        }

        {
            printf("COLLISION FOUND!\n");
            printf("hword: %04x, word: %08x\n", starthword, word);
            return true;
        }
    } while (++word != 0);

    return false;
}
