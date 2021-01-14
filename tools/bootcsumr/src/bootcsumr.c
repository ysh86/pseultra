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
#include <errno.h>
#include <string.h>

bool find_collision (uint32_t *bcode, uint64_t desired_checksum);

int main (int argc, char *argv[]) {
    // If arguments not adequate
    if (argc != 3) {
        fprintf(stderr, "Usage: bootcsumr <rom file> <type(1,2,3,5,6)>\n");
        return -1;
    }

    static const size_t nwords = 0x1000 / sizeof(uint32_t);
    uint32_t rom[nwords];

    FILE* fd = fopen(argv[1], "rb");
    int e = errno;
    if (!fd) {
        fprintf(stderr, "err: %s\n", strerror(e));
        return -1;
    }
    size_t n = fread(rom, sizeof(uint32_t), nwords, fd);
    e = errno;
    fclose(fd);
    if (n != nwords) {
        fprintf(stderr, "err: %s\n", strerror(e));
        return -1;
    }
    // LE to BE
    for (int i = 0; i < nwords; i++) {
        uint32_t le = rom[i];
        uint32_t be = ((le & 0xff) << 24) | ((le & 0xff00) << 8) | ((le & 0xff0000) >> 8) | ((le & 0xff000000) >> 24);
        rom[i]  = be;
    }

    int type = atoi(argv[2]);
    bool found = find_collision(&rom[0x10], type);
    if (found) {
        printf("COLLISION FOUND!\n");
        printf("hword: %04x, word: %08x\n", rom[0x3fe] & 0xffff, rom[0x3ff]);
        return 0;
    }

    fprintf(stderr, "NOT found!!\n");
    return -1;
}
