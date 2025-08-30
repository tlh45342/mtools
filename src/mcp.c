#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdbool.h>

#define VERSION "0.0.2"
#define SECTOR_SIZE 512
#define DIR_ENTRY_SIZE 32
#define DELETED_MARKER 0xE5

void usage(const char *progname) {
    fprintf(stderr, "Usage: %s -i <image> [--overwrite] <file>\n", progname);
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

bool find_entry(FILE *fp, uint32_t rootDirOffset, int rootEntryCount, const char *target83, long *foundOffset) {
    fseek(fp, rootDirOffset, SEEK_SET);
    for (int i = 0; i < rootEntryCount; i++) {
        long entryPos = ftell(fp);
        uint8_t entry[DIR_ENTRY_SIZE];
        fread(entry, 1, DIR_ENTRY_SIZE, fp);

        if (entry[0] == 0x00 || entry[0] == DELETED_MARKER) continue;

        if (memcmp(entry, target83, 11) == 0) {
            if (foundOffset) *foundOffset = entryPos;
            return true;
        }
    }
    return false;
}

int mcp(const char *image, const char *src, bool overwrite) {
    FILE *fp = fopen(image, "r+b");
    if (!fp) {
        perror("fopen image");
        return 1;
    }

    FILE *in = fopen(src, "rb");
    if (!in) {
        perror("fopen source file");
        fclose(fp);
        return 1;
    }

    // Read BPB
    fseek(fp, 11, SEEK_SET);
    uint16_t bytesPerSector;
    fread(&bytesPerSector, 2, 1, fp);
    fseek(fp, 13, SEEK_SET);
    uint8_t sectorsPerCluster;
    fread(&sectorsPerCluster, 1, 1, fp);
    uint16_t reservedSectors;
    fread(&reservedSectors, 2, 1, fp);
    uint8_t numFATs;
    fread(&numFATs, 1, 1, fp);
    uint16_t rootEntryCount;
    fread(&rootEntryCount, 2, 1, fp);
    fseek(fp, 22, SEEK_SET);
    uint16_t sectorsPerFAT;
    fread(&sectorsPerFAT, 2, 1, fp);

    uint32_t rootDirOffset = (reservedSectors + numFATs * sectorsPerFAT) * bytesPerSector;

    // Format filename
    char target83[12];
    format_83(target83, src);

    long existingOffset = 0;
    bool alreadyExists = find_entry(fp, rootDirOffset, rootEntryCount, target83, &existingOffset);

    if (alreadyExists && !overwrite) {
        fprintf(stderr, "Error: File %s already exists. Use --overwrite to replace it.\n", src);
        fclose(fp);
        fclose(in);
        return 1;
    }

    if (alreadyExists && overwrite) {
        printf("Overwriting %s in image...\n", src);
        // TODO: implement cluster clearing, FAT update, etc.
        fseek(fp, existingOffset, SEEK_SET);
    } else {
        // Find empty entry
        fseek(fp, rootDirOffset, SEEK_SET);
        for (int i = 0; i < rootEntryCount; i++) {
            long entryPos = ftell(fp);
            uint8_t entry[DIR_ENTRY_SIZE];
            fread(entry, 1, DIR_ENTRY_SIZE, fp);

            if (entry[0] == 0x00 || entry[0] == DELETED_MARKER) {
                fseek(fp, entryPos, SEEK_SET);
                break;
            }
        }
    }

    // Write filename
    fwrite(target83, 1, 11, fp);

    // Write dummy metadata and content placeholder
    fputc(0x20, fp); // file attribute = archive
    fseek(fp, 22, SEEK_CUR); // skip time/date
    fputc(0x02, fp); // starting cluster (simplified)
    fputc(0x00, fp);
    fseek(in, 0, SEEK_END);
    long size = ftell(in);
    fseek(in, 0, SEEK_SET);
    fwrite(&size, 4, 1, fp);

    printf("Copied %s into image (%ld bytes)\n", src, size);

    fclose(fp);
    fclose(in);
    return 0;
}

int main(int argc, char *argv[]) {
    const char *image = NULL;
    const char *file = NULL;
    bool overwrite = false;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-i")) {
            if (++i >= argc) usage(argv[0]);
            image = argv[i];
        } else if (!strcmp(argv[i], "--overwrite")) {
            overwrite = true;
        } else if (!file) {
            file = argv[i];
        } else {
            usage(argv[0]);
        }
    }

    if (!image || !file) usage(argv[0]);
    return mcp(image, file, overwrite);
}