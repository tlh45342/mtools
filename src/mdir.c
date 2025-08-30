// mdir.c - Minimal directory lister for FAT12/FAT16 "super-floppy" images
// Compile: gcc -Wall -Wextra -O2 -o mdir.exe src/mdir.c   (Windows/MinGW) or mdir (POSIX)

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>

#define SECTOR_SIZE 512

// ---- Version/branding ----
#define PROGRAM_NAME  "mdir"
#define PACKAGE_NAME  "mtools"
#define VERSION_STR   "0.0.2"
#define COPYRIGHT_YR  "2025"
#define AUTHOR_NAME   "Thomas Hamilton"

static void print_version(void) {
    printf("%s (%s) (%s)\n", PROGRAM_NAME, PACKAGE_NAME, VERSION_STR);
    printf("Copyright %s - %s\n", COPYRIGHT_YR, AUTHOR_NAME);
    printf("Copyright (C) 2021 Free Software Foundation, Inc.\n");
    printf("License GPLv3+: GNU GPL version 3 or later <https://gnu.org/licenses/gpl.html>.\n");
    printf("This is free software: you are free to change and redistribute it.\n");
    printf("There is NO WARRANTY, to the extent permitted by law.\n");
}

static inline uint16_t rd_le16(const uint8_t *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}
static inline uint32_t rd_le32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

typedef struct {
    uint16_t BytsPerSec;   // offset 11
    uint8_t  SecPerClus;   // 13
    uint16_t RsvdSecCnt;   // 14
    uint8_t  NumFATs;      // 16
    uint16_t RootEntCnt;   // 17
    uint16_t TotSec16;     // 19
    uint8_t  Media;        // 21
    uint16_t FATSz16;      // 22
    uint16_t SecPerTrk;    // 24
    uint16_t NumHeads;     // 26
    uint32_t HiddSec;      // 28
    uint32_t TotSec32;     // 32
} BPB;

typedef struct {
    int show_all; // include hidden/system
} Opts;

static void usage(void) {
    fprintf(stderr, "Usage: %s -i <image.img> [::] [-a] [--version]\n", PROGRAM_NAME);
}

// Decode DOS date/time
static void decode_dos_datetime(uint16_t dosDate, uint16_t dosTime,
                                int *Y, int *M, int *D, int *h, int *m, int *s) {
    *Y = 1980 + ((dosDate >> 9) & 0x7F);
    *M = (dosDate >> 5) & 0x0F;
    *D = dosDate & 0x1F;
    *h = (dosTime >> 11) & 0x1F;
    *m = (dosTime >> 5) & 0x3F;
    *s = (dosTime & 0x1F) * 2;
}

// Make 8.3 name from directory entry (skip LFN)
static void make_83_name(const uint8_t *ent, char out[13]) {
    char base[9], ext[4];
    memcpy(base, ent + 0, 8);
    memcpy(ext,  ent + 8, 3);
    // trim trailing spaces
    int i;
    for (i = 7; i >= 0 && base[i] == ' '; --i) {}
    base[i + 1] = '\0';
    for (i = 2; i >= 0 && ext[i] == ' '; --i) {}
    ext[i + 1] = '\0';
    // upper-case for display (classic)
    for (i = 0; base[i]; ++i) base[i] = (char)toupper((unsigned char)base[i]);
    for (i = 0; ext[i];  ++i) ext[i]  = (char)toupper((unsigned char)ext[i]);
    if (ext[0])
        snprintf(out, 13, "%s.%s", base, ext);
    else
        snprintf(out, 13, "%s", base);
}

// Attribute string "RHSVDA" (V=Volume, D=Directory, A=Archive)
static void attr_string(uint8_t a, char out[7]) {
    out[0] = (a & 0x01) ? 'R' : '-';
    out[1] = (a & 0x02) ? 'H' : '-';
    out[2] = (a & 0x04) ? 'S' : '-';
    out[3] = (a & 0x08) ? 'V' : '-';
    out[4] = (a & 0x10) ? 'D' : '-';
    out[5] = (a & 0x20) ? 'A' : '-';
    out[6] = '\0';
}

static int read_bpb(FILE *fp, BPB *bpb, uint8_t *boot) {
    if (fseek(fp, 0, SEEK_SET) != 0) return -1;
    if (fread(boot, 1, SECTOR_SIZE, fp) != SECTOR_SIZE) return -1;
    // minimal validation
    if (!(boot[510] == 0x55 && boot[511] == 0xAA)) {
        fprintf(stderr, "Warning: boot sector signature 0x55AA not found.\n");
    }
    // fill BPB
    bpb->BytsPerSec = rd_le16(&boot[11]);
    bpb->SecPerClus = boot[13];
    bpb->RsvdSecCnt = rd_le16(&boot[14]);
    bpb->NumFATs    = boot[16];
    bpb->RootEntCnt = rd_le16(&boot[17]);
    bpb->TotSec16   = rd_le16(&boot[19]);
    bpb->Media      = boot[21];
    bpb->FATSz16    = rd_le16(&boot[22]);
    bpb->SecPerTrk  = rd_le16(&boot[24]);
    bpb->NumHeads   = rd_le16(&boot[26]);
    bpb->HiddSec    = rd_le32(&boot[28]);
    bpb->TotSec32   = rd_le32(&boot[32]);

    if (bpb->BytsPerSec == 0 || bpb->BytsPerSec % 512 != 0) {
        fprintf(stderr, "Unsupported bytes/sector: %u\n", bpb->BytsPerSec);
        return -1;
    }
    return 0;
}

int main(int argc, char **argv) {
    const char *image = NULL;
    Opts opt = {0};

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--version") == 0 || strcmp(argv[i], "-V") == 0) {
            print_version();
            return 0;
        } else if (strcmp(argv[i], "-i") == 0 && i + 1 < argc) {
            image = argv[++i];
        } else if (strcmp(argv[i], "-a") == 0) {
            opt.show_all = 1;
        } else if (strcmp(argv[i], "::") == 0) {
            continue; // accept mtools-style
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            usage();
            return 0;
        } else {
            usage();
            return 1;
        }
    }
    if (!image) {
        usage();
        return 1;
    }

    FILE *fp = fopen(image, "rb");
    if (!fp) {
        perror("open image");
        return 1;
    }

    uint8_t boot[SECTOR_SIZE];
    BPB bpb;
    if (read_bpb(fp, &bpb, boot) != 0) {
        fclose(fp);
        return 1;
    }

    if (bpb.RootEntCnt == 0) {
        fprintf(stderr, "This looks like FAT32 (RootEntCnt==0). v1 lists only FAT12/16 root.\n");
        fclose(fp);
        return 1;
    }

    // Compute root directory position and size
    uint32_t rootDirSectors = ((uint32_t)bpb.RootEntCnt * 32 + (bpb.BytsPerSec - 1)) / bpb.BytsPerSec;
    if (bpb.FATSz16 == 0) {
        fprintf(stderr, "Unexpected FATSz16=0 for FAT12/16.\n");
        fclose(fp);
        return 1;
    }
    uint32_t firstRootSector = (uint32_t)bpb.RsvdSecCnt + ((uint32_t)bpb.NumFATs * bpb.FATSz16);
    uint64_t rootOffset = (uint64_t)firstRootSector * bpb.BytsPerSec;
    uint64_t rootSize   = (uint64_t)rootDirSectors * bpb.BytsPerSec;

    // Read root directory
    uint8_t *root = (uint8_t*)malloc((size_t)rootSize);
    if (!root) {
        fprintf(stderr, "Out of memory reading root dir\n");
        fclose(fp);
        return 1;
    }
    if (fseek(fp, (long)rootOffset, SEEK_SET) != 0 ||
        fread(root, 1, (size_t)rootSize, fp) != rootSize) {
            fprintf(stderr, "Failed to read root directory\n");
            free(root);
            fclose(fp);
            return 1;
    }

    printf(" Volume in drive ::  ");
    // Try to show volume label from boot sector first
    // (BS_VolLab at offset 43..53 if BS_BootSig==0x29)
    if (boot[38] == 0x29) {
        char label[12]; memcpy(label, &boot[43], 11); label[11] = '\0';
        for (int i = 10; i >= 0 && label[i] == ' '; --i) label[i] = '\0';
        if (label[0]) printf("%s\n\n", label);
        else          printf("NO LABEL\n\n");
    } else {
        printf("\n\n");
    }

    printf("  Size      Date       Time   Attr  Name\n");
    printf("--------  ----------  ------- ------ ------------\n");

    // Iterate 32-byte directory entries
    size_t entries = (size_t)bpb.RootEntCnt;
    for (size_t i = 0; i < entries; ++i) {
        const uint8_t *ent = root + i * 32;
        uint8_t first = ent[0];

        if (first == 0x00) break;        // end of directory
        if (first == 0xE5) continue;     // deleted
        uint8_t attr = ent[11];
        if (attr == 0x0F) continue;      // LFN entry (skip v1)

        // Filter hidden/system unless -a
        if (!opt.show_all && (attr & (0x02 | 0x04))) {
            continue;
        }

        // Volume label line (optional to show)
        if (attr & 0x08) {
            char lbl[12]; memcpy(lbl, ent, 11); lbl[11] = '\0';
            for (int j = 10; j >= 0 && lbl[j] == ' '; --j) lbl[j] = '\0';
            printf("          <VOL LABEL>        ------ %s\n", lbl[0] ? lbl : "(blank)");
            continue;
        }

        char name[13];
        make_83_name(ent, name);

        uint32_t size = rd_le32(&ent[28]);
        uint16_t time = rd_le16(&ent[22]);
        uint16_t date = rd_le16(&ent[24]);

        int Y, M, D, h, m, s;
        decode_dos_datetime(date, time, &Y, &M, &D, &h, &m, &s);

        char a[7]; attr_string(attr, a);

        printf("%8u  %04d-%02d-%02d  %02d:%02d  %s  %s\n",
               size, Y, M, D, h, m, a, name);
    }

    free(root);
    fclose(fp);
    return 0;
}
