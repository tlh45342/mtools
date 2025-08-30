#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#define VERSION "0.0.1"
#define SECTOR_SIZE 512
#define DIR_ENTRY_SIZE 32
#define DELETED_MARKER 0xE5

#include <ctype.h>  // For toupper()

void usage(const char *progname) {
    fprintf(stderr, "Usage: %s -i <image> <filename>\n", progname);
    exit(1);
}

void format_83(char *out, const char *in) {
    memset(out, ' ', 11);
    const char *dot = strchr(in, '.');
    size_t name_len = dot ? (size_t)(dot - in) : strlen(in);
    size_t ext_len  = dot ? strlen(dot + 1) : 0;

    for (size_t i = 0; i < name_len && i < 8; i++) {
        out[i] = toupper((unsigned char)in[i]);
    }
    if (dot) {
        for (size_t i = 0; i < ext_len && i < 3; i++) {
            out[8 + i] = toupper((unsigned char)dot[1 + i]);
        }
    }
}

int del(const char *image, const char *target) {
    FILE *fp = fopen(image, "r+b");
    if (!fp) {
        perror("fopen");
        return 0;
    }

    // Read BPB to locate root directory
    fseek(fp, 11, SEEK_SET);  // BPB offset
    uint16_t bytesPerSector;
    fread(&bytesPerSector, 2, 1, fp);

    uint8_t sectorsPerCluster;
    fread(&sectorsPerCluster, 1, 1, fp);

    uint16_t reservedSectors;
    fread(&reservedSectors, 2, 1, fp);

    uint8_t numFATs;
    fread(&numFATs, 1, 1, fp);

    uint16_t rootEntryCount;
    fread(&rootEntryCount, 2, 1, fp);

    uint16_t totalSectors16;
    fread(&totalSectors16, 2, 1, fp);

    fseek(fp, 22, SEEK_SET);  // BPB_FATSz16
    uint16_t sectorsPerFAT;
    fread(&sectorsPerFAT, 2, 1, fp);

    uint32_t rootDirOffset = (reservedSectors + numFATs * sectorsPerFAT) * bytesPerSector;

    // Convert to 8.3 format
    char target83[12];
    format_83(target83, target);

    // Scan root directory
    fseek(fp, rootDirOffset, SEEK_SET);
    for (int i = 0; i < rootEntryCount; i++) {
        long entryPos = ftell(fp);
        uint8_t entry[DIR_ENTRY_SIZE];
        fread(entry, 1, DIR_ENTRY_SIZE, fp);

        if (entry[0] == 0x00 || entry[0] == DELETED_MARKER) continue;

        if (memcmp(entry, target83, 11) == 0) {
            fseek(fp, entryPos, SEEK_SET);
            fputc(DELETED_MARKER, fp);
            printf("Deleted: %s\n", target);
            fclose(fp);
            return 1;
        }
    }

    fclose(fp);
    return 0;
}


int main(int argc, char *argv[]) {
    const char *image = NULL;
    const char *target = NULL;

    if (argc == 2 && strcmp(argv[1], "--version") == 0) {
        printf("%s version %s\n", argv[0], VERSION);
        return 0;
    }

    // Parse args
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-i")) {
            if (++i >= argc) usage(argv[0]);
            image = argv[i];
        } else if (!target) {
            target = argv[i];
        } else {
            usage(argv[0]);
        }
    }

    if (!image || !target) usage(argv[0]);

    if (!del(image, target)) {
        fprintf(stderr, "File not found: %s\n", target);
        return 1;
    }

    return 0;
}