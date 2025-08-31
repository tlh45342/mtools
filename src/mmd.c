// src/mmd.c
// Minimal "mtools-like" mmd: create a directory in the root of a FAT12/16 image.
// Limitations:
//  - FAT12/16 only (root creation); FAT32 and nested paths TODO
//  - 8.3 names only (no LFN)
//  - Timestamps are zeroed
// Build: cc -Wall -Wextra -O2 src/mmd.c -o build/mmd
// Usage: mmd -i IMAGE NEWDIR    (NEWDIR must be 8.3)

#define _FILE_OFFSET_BITS 64
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#pragma pack(push,1)
typedef struct {
    uint8_t  jmp[3];
    char     oem[8];
    uint16_t bytes_per_sector;   // offset 11
    uint8_t  sectors_per_cluster;// 13
    uint16_t reserved_sectors;   // 14
    uint8_t  num_fats;           // 16
    uint16_t root_entries;       // 17 (FAT12/16)
    uint16_t total_sectors16;    // 19
    uint8_t  media;              // 21
    uint16_t sectors_per_fat16;  // 22
    uint16_t sectors_per_track;  // 24
    uint16_t num_heads;          // 26
    uint32_t hidden_sectors;     // 28
    uint32_t total_sectors32;    // 32
    // FAT32 fields omitted (we only support FAT12/16 here)
} BPB1216;

typedef struct {
    uint8_t  name[8];
    uint8_t  ext[3];
    uint8_t  attr;
    uint8_t  ntres;
    uint8_t  crtTimeTenth;
    uint16_t crtTime;
    uint16_t crtDate;
    uint16_t lstAccDate;
    uint16_t fstClusHI;   // FAT32 only; zero on FAT12/16
    uint16_t wrtTime;
    uint16_t wrtDate;
    uint16_t fstClusLO;
    uint32_t fileSize;
} DirEnt;
#pragma pack(pop)

// Attributes
enum { ATTR_READONLY=0x01, ATTR_HIDDEN=0x02, ATTR_SYSTEM=0x04, ATTR_VOLUME=0x08,
       ATTR_DIR=0x10,      ATTR_ARCHIVE=0x20 };

// Geometry helper
typedef struct {
    BPB1216 bpb;
    uint32_t bytes_per_sector;
    uint32_t root_dir_sectors;
    uint32_t fat_size_sectors;     // per FAT (FAT12/16)
    uint32_t first_fat_lba;
    uint32_t first_root_lba;       // FAT12/16 fixed root
    uint32_t first_data_lba;       // first cluster (#2)
    uint32_t total_sectors;
    uint32_t total_clusters;
    int      fat_bits;             // 12 or 16 (32 not supported here)
} FatGeom;

// --- I/O helpers ---
static int read_bpb(FILE *f, FatGeom *g) {
    if (fseeko(f, 0, SEEK_SET) != 0) return -1;
    if (fread(&g->bpb, 1, sizeof(BPB1216), f) != sizeof(BPB1216)) return -1;

    g->bytes_per_sector = g->bpb.bytes_per_sector;
    if (g->bytes_per_sector == 0 || (g->bytes_per_sector & (g->bytes_per_sector-1)))
        return -1;

    uint32_t totsec = g->bpb.total_sectors16 ? g->bpb.total_sectors16 : g->bpb.total_sectors32;
    g->total_sectors = totsec;

    g->fat_size_sectors = g->bpb.sectors_per_fat16;
    if (g->fat_size_sectors == 0) {
        // Likely FAT32 (needs BPB_FATSz32, not handled here)
        return -2;
    }

    // Root dir sectors (FAT12/16)
    g->root_dir_sectors = ((g->bpb.root_entries * 32u) + (g->bpb.bytes_per_sector - 1u))
                          / g->bpb.bytes_per_sector;
    g->first_fat_lba  = g->bpb.reserved_sectors;
    g->first_root_lba = g->first_fat_lba + (g->bpb.num_fats * g->fat_size_sectors);
    g->first_data_lba = g->first_root_lba + g->root_dir_sectors;

    uint32_t data_sectors = totsec - (g->bpb.reserved_sectors +
                                      (g->bpb.num_fats * g->fat_size_sectors) +
                                      g->root_dir_sectors);
    if (g->bpb.sectors_per_cluster == 0) return -1;
    g->total_clusters = data_sectors / g->bpb.sectors_per_cluster;

    // Decide FAT type by cluster count
    if (g->total_clusters < 4085)        g->fat_bits = 12;
    else if (g->total_clusters < 65525)  g->fat_bits = 16;
    else                                 g->fat_bits = 32; // not handled

    if (g->fat_bits == 32) return -2;
    return 0;
}

static int read_sector(FILE *f, const FatGeom *g, uint32_t lba, void *buf) {
    off_t off = (off_t)lba * g->bytes_per_sector;
    if (fseeko(f, off, SEEK_SET) != 0) return -1;
    size_t n = fread(buf, 1, g->bytes_per_sector, f);
    return (n == g->bytes_per_sector) ? 0 : -1;
}
static int write_sector(FILE *f, const FatGeom *g, uint32_t lba, const void *buf) {
    off_t off = (off_t)lba * g->bytes_per_sector;
    if (fseeko(f, off, SEEK_SET) != 0) return -1;
    size_t n = fwrite(buf, 1, g->bytes_per_sector, f);
    return (n == g->bytes_per_sector) ? 0 : -1;
}

// --- FAT access (12/16) ---
static int fat_get(FILE *f, const FatGeom *g, uint32_t clus, uint32_t *val) {
    if (clus < 2 || clus >= g->total_clusters + 2) return -1;

    uint8_t secbuf[4096], secbuf2[4096];
    if (g->bytes_per_sector > sizeof(secbuf)) return -1;

    if (g->fat_bits == 12) {
        // 1.5 bytes per entry
        uint32_t byte_offset = (clus * 3) / 2;
        uint32_t lba = g->first_fat_lba + (byte_offset / g->bytes_per_sector);
        uint32_t off = byte_offset % g->bytes_per_sector;

        if (read_sector(f, g, lba, secbuf) != 0) return -1;

        uint16_t pair;
        if (off + 1 < g->bytes_per_sector) {
            pair = (uint16_t)secbuf[off] | ((uint16_t)secbuf[off+1] << 8);
        } else {
            // straddles sectors
            if (read_sector(f, g, lba + 1, secbuf2) != 0) return -1;
            pair = (uint16_t)secbuf[off] | ((uint16_t)secbuf2[0] << 8);
        }

        if (clus & 1) *val = (pair >> 4) & 0x0FFF;
        else          *val = pair & 0x0FFF;
        return 0;
    } else {
        // FAT16: 2 bytes per entry
        uint32_t fat_offset = clus * 2;
        uint32_t lba = g->first_fat_lba + (fat_offset / g->bytes_per_sector);
        uint32_t off = fat_offset % g->bytes_per_sector;
        if (read_sector(f, g, lba, secbuf) != 0) return -1;
        *val = (uint16_t)secbuf[off] | ((uint16_t)secbuf[off+1] << 8);
        return 0;
    }
}

static int fat_set(FILE *f, const FatGeom *g, uint32_t clus, uint32_t value) {
    if (clus < 2 || clus >= g->total_clusters + 2) return -1;

    uint8_t secbuf[4096], secbuf2[4096];
    if (g->bytes_per_sector > sizeof(secbuf)) return -1;

    if (g->fat_bits == 12) {
        uint32_t byte_offset = (clus * 3) / 2;
        uint32_t base_lba = (byte_offset / g->bytes_per_sector);
        uint32_t off      = byte_offset % g->bytes_per_sector;

        for (uint8_t fi = 0; fi < g->bpb.num_fats; ++fi) {
            uint32_t lba = g->first_fat_lba + fi * g->fat_size_sectors + base_lba;

            if (read_sector(f, g, lba, secbuf) != 0) return -1;

            uint16_t pair;
            int crosses = (off + 1 >= g->bytes_per_sector);
            if (!crosses) {
                pair = (uint16_t)secbuf[off] | ((uint16_t)secbuf[off+1] << 8);
            } else {
                if (read_sector(f, g, lba + 1, secbuf2) != 0) return -1;
                pair = (uint16_t)secbuf[off] | ((uint16_t)secbuf2[0] << 8);
            }

            if (clus & 1) { // high 12 bits
                pair = (pair & 0x000F) | ((value & 0x0FFF) << 4);
            } else {        // low 12 bits
                pair = (pair & 0xF000) | (value & 0x0FFF);
            }

            // write back
            secbuf[off] = pair & 0xFF;
            if (!crosses) {
                secbuf[off+1] = (pair >> 8) & 0xFF;
                if (write_sector(f, g, lba, secbuf) != 0) return -1;
            } else {
                if (write_sector(f, g, lba, secbuf) != 0) return -1;
                secbuf2[0] = (pair >> 8) & 0xFF;
                if (write_sector(f, g, lba + 1, secbuf2) != 0) return -1;
            }
        }
        return 0;
    } else {
        // FAT16
        uint32_t fat_offset = clus * 2;
        uint32_t base_lba   = (fat_offset / g->bytes_per_sector);
        uint32_t off        = fat_offset % g->bytes_per_sector;

        for (uint8_t fi = 0; fi < g->bpb.num_fats; ++fi) {
            uint32_t lba = g->first_fat_lba + fi * g->fat_size_sectors + base_lba;
            if (read_sector(f, g, lba, secbuf) != 0) return -1;
            secbuf[off]     = value & 0xFF;
            secbuf[off + 1] = (value >> 8) & 0xFF;
            if (write_sector(f, g, lba, secbuf) != 0) return -1;
        }
        return 0;
    }
}

static uint32_t eoc_value(const FatGeom *g) {
    return (g->fat_bits == 12) ? 0x0FFF : 0xFFFF;
}
static int is_free_fat_val(const FatGeom *g, uint32_t v) {
    return (g->fat_bits == 12) ? (v == 0x000) : (v == 0x0000);
}

static int alloc_free_cluster(FILE *f, const FatGeom *g, uint32_t *clus_out) {
    // Simple first-fit scan
    for (uint32_t c = 2; c < g->total_clusters + 2; ++c) {
        uint32_t v;
        if (fat_get(f, g, c, &v) != 0) return -1;
        if (is_free_fat_val(g, v)) {
            if (fat_set(f, g, c, eoc_value(g)) != 0) return -1;
            *clus_out = c;
            return 0;
        }
    }
    return -1; // no space
}

static uint32_t cluster_to_lba(const FatGeom *g, uint32_t clus) {
    return g->first_data_lba + (clus - 2) * g->bpb.sectors_per_cluster;
}

// --- 8.3 packing (very basic). Returns 0 on success, -1 if invalid ---
static int pack_83(const char *in, uint8_t name[8], uint8_t ext[3]) {
    const char *dot = strchr(in, '.');
    if (dot && strchr(dot + 1, '.')) return -1; // multiple dots not allowed

    size_t blen = dot ? (size_t)(dot - in) : strlen(in);
    size_t elen = dot ? strlen(dot + 1) : 0;
    if (blen == 0 || blen > 8 || elen > 3) return -1;

    for (size_t i=0;i<8;i++) name[i] = ' ';
    for (size_t i=0;i<3;i++) ext[i]  = ' ';

    for (size_t i=0;i<blen;i++) {
        unsigned char c = (unsigned char)in[i];
        if (c==' ' || c=='+' || c==',' || c==';' || c==':' || c=='=' || c=='[' || c==']')
            return -1;
        name[i] = (uint8_t)toupper(c);
    }
    for (size_t i=0;i<elen;i++) {
        unsigned char c = (unsigned char)dot[1+i];
        if (c==' ' || c=='+' || c==',' || c==';' || c==':' || c=='=' || c=='[' || c==']')
            return -1;
        ext[i] = (uint8_t)toupper(c);
    }
    return 0;
}

// --- Root directory scanning (FAT12/16) ---
static int find_free_root_slot(FILE *f, const FatGeom *g, uint32_t *slot_index) {
    // Root directory is contiguous for FAT12/16
    const uint32_t ents_per_sector = g->bytes_per_sector / 32;
    uint8_t sec[4096];
    for (uint32_t i=0;i<(g->root_dir_sectors);++i) {
        if (read_sector(f, g, g->first_root_lba + i, sec) != 0) return -1;
        for (uint32_t e=0;e<ents_per_sector; ++e) {
            uint8_t *p = sec + e*32;
            if (p[0] == 0x00 || p[0] == 0xE5) {
                *slot_index = i*ents_per_sector + e;
                return 0;
            }
        }
    }
    return -1; // full
}

static int check_exists_root(FILE *f, const FatGeom *g, const uint8_t name[8], const uint8_t ext[3]) {
    const uint32_t ents_per_sector = g->bytes_per_sector / 32;
    uint8_t sec[4096];
    for (uint32_t i=0;i<g->root_dir_sectors;++i) {
        if (read_sector(f, g, g->first_root_lba + i, sec) != 0) return -1;
        for (uint32_t e=0;e<ents_per_sector;++e) {
            DirEnt *de = (DirEnt*)(sec + e*32);
            if (de->name[0]==0x00 || de->name[0]==0xE5) continue;
            if (memcmp(de->name, name, 8)==0 && memcmp(de->ext, ext, 3)==0) return 1; // exists
        }
    }
    return 0;
}

static int write_root_entry(FILE *f, const FatGeom *g, uint32_t slot_index, const DirEnt *de) {
    const uint32_t ents_per_sector = g->bytes_per_sector / 32;
    uint32_t sec_index = slot_index / ents_per_sector;
    uint32_t off_in_sec = (slot_index % ents_per_sector) * 32;
    uint8_t sec[4096];
    if (read_sector(f, g, g->first_root_lba + sec_index, sec) != 0) return -1;
    memcpy(sec + off_in_sec, de, sizeof(DirEnt));
    if (write_sector(f, g, g->first_root_lba + sec_index, sec) != 0) return -1;
    return 0;
}

// --- Directory cluster init ---
static int zero_cluster(FILE *f, const FatGeom *g, uint32_t clus) {
    uint32_t lba = cluster_to_lba(g, clus);
    uint8_t *z = calloc(1, g->bytes_per_sector);
    if (!z) return -1;
    for (uint8_t s=0; s<g->bpb.sectors_per_cluster; ++s) {
        if (write_sector(f, g, lba + s, z) != 0) { free(z); return -1; }
    }
    free(z);
    return 0;
}

static int init_dot_entries(FILE *f, const FatGeom *g, uint32_t clus, uint16_t parentClus) {
    uint32_t lba = cluster_to_lba(g, clus);
    uint8_t sec[4096];
    memset(sec, 0, sizeof(sec)); // we will only write one sector here (cluster already zeroed)

    DirEnt dot = {0}, dotdot = {0};
    memset(dot.name, ' ', 8); memset(dot.ext, ' ', 3);
    dot.name[0] = '.';
    dot.attr = ATTR_DIR;
    dot.fstClusHI = 0;
    dot.fstClusLO = (uint16_t)clus;
    dot.fileSize  = 0;

    memset(dotdot.name, ' ', 8); memset(dotdot.ext, ' ', 3);
    dotdot.name[0] = '.'; dotdot.name[1] = '.';
    dotdot.attr = ATTR_DIR;
    dotdot.fstClusHI = 0;
    dotdot.fstClusLO = parentClus; // 0 for root on FAT12/16
    dotdot.fileSize  = 0;

    memcpy(sec + 0, &dot, sizeof(DirEnt));
    memcpy(sec + 32, &dotdot, sizeof(DirEnt));
    if (write_sector(f, g, lba, sec) != 0) return -1;

    return 0;
}

// --- CLI ---
static void usage(const char *prog) {
    fprintf(stderr,
        "Usage: %s -i IMAGE NEWDIR\n"
        "  -i IMAGE   FAT12/16 disk image file to modify\n"
        "  NEWDIR     8.3 name of directory to create in root\n",
        prog);
}

int main(int argc, char **argv) {
    const char *img = NULL;
    const char *newdir = NULL;

    // minimal option parsing: only -i IMAGE
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-i") == 0) {
            if (i + 1 >= argc) { usage(argv[0]); return 2; }
            img = argv[++i];
        } else if (!newdir) {
            newdir = argv[i];
        } else {
            usage(argv[0]);
            return 2;
        }
    }

    if (!img || !newdir) {
        usage(argv[0]);
        return 2;
    }

    uint8_t name[8], ext[3];
    if (pack_83(newdir, name, ext) != 0) {
        fprintf(stderr, "Error: NEWDIR must be 8.3 without illegal characters\n");
        return 2;
    }

    FILE *f = fopen(img, "r+b");
    if (!f) {
        fprintf(stderr, "Cannot open %s: %s\n", img, strerror(errno));
        return 1;
    }

    FatGeom g;
    int rb = read_bpb(f, &g);
    if (rb == -2) {
        fprintf(stderr, "FAT32 not supported by this minimal mmd (yet).\n");
        fclose(f);
        return 1;
    }
    if (rb != 0) {
        fprintf(stderr, "Failed to read BPB / unsupported image.\n");
        fclose(f);
        return 1;
    }

    // Ensure directory not already present in root
    int ex = check_exists_root(f, &g, name, ext);
    if (ex < 0) { fprintf(stderr, "Read error scanning root.\n"); fclose(f); return 1; }
    if (ex > 0) { fprintf(stderr, "Directory already exists.\n"); fclose(f); return 1; }

    // Find free slot in root
    uint32_t slot;
    if (find_free_root_slot(f, &g, &slot) != 0) {
        fprintf(stderr, "Root directory is full.\n");
        fclose(f);
        return 1;
    }

    // Allocate one cluster for the new directory
    uint32_t clus;
    if (alloc_free_cluster(f, &g, &clus) != 0) {
        fprintf(stderr, "No free clusters available.\n");
        fclose(f);
        return 1;
    }

    // Zero it and write dot entries
    if (zero_cluster(f, &g, clus) != 0) {
        fprintf(stderr, "Failed zeroing new dir cluster.\n");
        fclose(f);
        return 1;
    }
    if (init_dot_entries(f, &g, clus, 0 /*root parent*/) != 0) {
        fprintf(stderr, "Failed writing . and .. entries.\n");
        fclose(f);
        return 1;
    }

    // Compose directory entry
    DirEnt de = {0};
    memcpy(de.name, name, 8);
    memcpy(de.ext, ext, 3);
    de.attr = ATTR_DIR;
    de.fstClusHI = 0;
    de.fstClusLO = (uint16_t)clus;
    de.fileSize = 0;

    if (write_root_entry(f, &g, slot, &de) != 0) {
        fprintf(stderr, "Failed to write root dir entry.\n");
        fclose(f);
        return 1;
    }

    fclose(f);
    printf("Created directory %s (cluster %u)\n", newdir, clus);
    return 0;
}
