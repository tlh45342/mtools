// minfo.c - Minimal info tool for FAT12/16/32 "super-floppy" images
// Compile: gcc -Wall -O2 -o minfo.exe minfo.c   (Windows/MinGW) or minfo (POSIX)

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>

#define SECTOR_SIZE_MIN 512

static inline uint16_t rd_le16(const uint8_t *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}
static inline uint32_t rd_le32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

typedef struct {
    // Standard BPB
    uint16_t BytsPerSec;   // 11
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
    // Extended (FAT12/16)
    uint8_t  BS_DrvNum;    // 36
    uint8_t  BS_Reserved1; // 37
    uint8_t  BS_BootSig;   // 38 (0x29 if VolID/Lab/Type present)
    uint32_t BS_VolID;     // 39..42
    char     BS_VolLab[12];// 43..53 (11 chars + NUL)
    char     BS_FilSysType[9]; // 54..61 (8 chars + NUL)
    // FAT32 extras (if present)
    uint32_t FATSz32;      // 36 (overlaps FAT12/16 ext area in FAT32 layout)
    uint32_t RootClus;     // 44
    uint16_t FSInfo;       // 48
    uint16_t BkBootSec;    // 50
    int isFat32Layout;     // internal: 1 if RootEntCnt==0
} BPB;

static int read_boot_sector(FILE *fp, uint8_t *buf, size_t sz) {
    if (fseek(fp, 0, SEEK_SET) != 0) return -1;
    if (fread(buf, 1, sz, fp) != sz) return -1;
    return 0;
}

static int parse_bpb(const uint8_t *b, BPB *o) {
    memset(o, 0, sizeof(*o));
    o->BytsPerSec = rd_le16(&b[11]);
    o->SecPerClus = b[13];
    o->RsvdSecCnt = rd_le16(&b[14]);
    o->NumFATs    = b[16];
    o->RootEntCnt = rd_le16(&b[17]);
    o->TotSec16   = rd_le16(&b[19]);
    o->Media      = b[21];
    o->FATSz16    = rd_le16(&b[22]);
    o->SecPerTrk  = rd_le16(&b[24]);
    o->NumHeads   = rd_le16(&b[26]);
    o->HiddSec    = rd_le32(&b[28]);
    o->TotSec32   = rd_le32(&b[32]);

    // Extended FAT12/16 (only valid if not FAT32)
    o->BS_DrvNum    = b[36];
    o->BS_Reserved1 = b[37];
    o->BS_BootSig   = b[38];
    o->BS_VolID     = rd_le32(&b[39]);
    memcpy(o->BS_VolLab, &b[43], 11); o->BS_VolLab[11] = '\0';
    memcpy(o->BS_FilSysType, &b[54], 8); o->BS_FilSysType[8] = '\0';

    // Heuristic for FAT32 layout: RootEntCnt==0 and FATSz16==0
    o->isFat32Layout = (o->RootEntCnt == 0 && o->FATSz16 == 0);
    if (o->isFat32Layout) {
        o->FATSz32   = rd_le32(&b[36]);
        o->RootClus  = rd_le32(&b[44]);
        o->FSInfo    = rd_le16(&b[48]);
        o->BkBootSec = rd_le16(&b[50]);
    }
    return 0;
}

static uint32_t total_sectors(const BPB *b) {
    return (b->TotSec16 != 0) ? b->TotSec16 : b->TotSec32;
}

static void trim_right(char *s) {
    for (int i = (int)strlen(s) - 1; i >= 0 && s[i] == ' '; --i) s[i] = '\0';
}

static const char* fat_type_from_clusters(uint32_t clusters) {
    if (clusters < 4085)    return "FAT12";
    if (clusters < 65525)   return "FAT16";
    return "FAT32";
}

static void usage(void) {
    fprintf(stderr, "Usage: minfo -i <image.img> [::]\n");
}

int main(int argc, char **argv) {
    const char *image = NULL;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-i") == 0 && i + 1 < argc) {
            image = argv[++i];
        } else if (strcmp(argv[i], "::") == 0) {
            continue;
        } else {
            usage();
            return 1;
        }
    }
    if (!image) { usage(); return 1; }

    FILE *fp = fopen(image, "rb");
    if (!fp) { perror("open image"); return 1; }

    uint8_t bs[SECTOR_SIZE_MIN];
    if (read_boot_sector(fp, bs, sizeof(bs)) != 0) {
        fprintf(stderr, "Failed to read boot sector\n");
        fclose(fp);
        return 1;
    }

    if (!(bs[510] == 0x55 && bs[511] == 0xAA)) {
        fprintf(stderr, "Warning: boot sector signature 0x55AA not found.\n");
    }

    BPB b; parse_bpb(bs, &b);
    uint32_t totSec = total_sectors(&b);

    // Basic validation
    int warn = 0;
    if (b.BytsPerSec < 512 || (b.BytsPerSec & (b.BytsPerSec - 1)) != 0) {
        fprintf(stderr, "Warning: unusual Bytes/sector = %u\n", b.BytsPerSec); warn = 1;
    }
    if (b.SecPerClus == 0 || (b.SecPerClus & (b.SecPerClus - 1)) != 0) {
        fprintf(stderr, "Warning: SecPerClus should be a power of two (got %u)\n", b.SecPerClus); warn = 1;
    }
    if (b.NumFATs == 0) {
        fprintf(stderr, "Warning: NumFATs=0\n"); warn = 1;
    }

    // Derived layout
    uint32_t fatsz = b.isFat32Layout ? b.FATSz32 : b.FATSz16;
    if (fatsz == 0) {
        fprintf(stderr, "Warning: FAT size is zero; layout may be invalid.\n"); warn = 1;
    }

    uint32_t rootDirSectors = (!b.isFat32Layout)
        ? ((uint32_t)b.RootEntCnt * 32 + (b.BytsPerSec - 1)) / b.BytsPerSec
        : 0;

    uint32_t firstFATSector   = b.RsvdSecCnt;
    uint32_t firstRootSector  = b.RsvdSecCnt + (b.NumFATs * fatsz);
    uint32_t firstDataSector  = firstRootSector + rootDirSectors;

    uint32_t dataSectors = (totSec > firstDataSector) ? (totSec - firstDataSector) : 0;
    uint32_t clusters    = (b.SecPerClus ? dataSectors / b.SecPerClus : 0);

    // Print summary
    printf("Boot Sector Summary (from %s)\n", image);
    printf(" OEM Name        : \"%.8s\"\n", &bs[3]);
    printf(" Bytes/sector    : %u\n", b.BytsPerSec);
    printf(" Sec/cluster     : %u\n", b.SecPerClus);
    printf(" Reserved sectors: %u\n", b.RsvdSecCnt);
    printf(" Number of FATs  : %u\n", b.NumFATs);
    printf(" Root entries    : %u\n", b.RootEntCnt);
    printf(" Total sectors   : %u\n", totSec);
    printf(" Media           : 0x%02X\n", b.Media);
    printf(" FAT size (sec)  : %u\n", fatsz);
    printf(" Sec/track       : %u\n", b.SecPerTrk);
    printf(" Heads           : %u\n", b.NumHeads);
    printf(" Hidden sectors  : %u\n", b.HiddSec);
    if (!b.isFat32Layout) {
        printf(" Ext BootSig     : 0x%02X\n", b.BS_BootSig);
        if (b.BS_BootSig == 0x29) {
            char lab[12]; strncpy(lab, b.BS_VolLab, 12); trim_right(lab);
            char fst[9];  strncpy(fst, b.BS_FilSysType, 9); trim_right(fst);
            printf(" Volume ID       : %08X\n", b.BS_VolID);
            printf(" Volume Label    : \"%s\"\n", lab);
            printf(" File system tag : \"%s\"\n", fst);
        }
    } else {
        printf(" FAT32 FATSz32   : %u\n", b.FATSz32);
        printf(" FAT32 RootClus  : %u\n", b.RootClus);
        printf(" FAT32 FSInfo    : %u\n", b.FSInfo);
        printf(" FAT32 BkBootSec : %u\n", b.BkBootSec);
    }

    printf("\nDerived Layout\n");
    printf(" First FAT sector: %u\n", firstFATSector);
    printf(" Root dir start  : %u (sectors)\n", firstRootSector);
    if (!b.isFat32Layout)
        printf(" Root dir size   : %u sectors\n", rootDirSectors);
    else
        printf(" Root dir size   : (dynamic; FAT32, via cluster chain)\n");
    printf(" First data sect : %u\n", firstDataSector);
    printf(" Data sectors    : %u\n", dataSectors);
    printf(" Cluster count   : %u\n", clusters);
    printf(" Guessed FAT type: %s\n", fat_type_from_clusters(clusters));

    if (warn) {
        printf("\nNotes: One or more suspicious values detected (see warnings above).\n");
    }

    fclose(fp);
    return 0;
}
