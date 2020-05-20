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

bool find_collision (uint32_t *bcode, uint64_t desired_checksum);

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
        printf("COLLISION FOUND!\n");
        printf("hword: %04x, word: %08x\n", rom[0x3fe] & 0xffff, rom[0x3ff]);
        return 0;
    }

    fprintf(stderr, "NOT found!!\n");
    return -1;
}
