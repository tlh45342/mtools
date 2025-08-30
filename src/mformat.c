#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/stat.h>

#define VERSION "0.0.3"
#define SECTOR_SIZE 512
#define DEFAULT_IMAGE_SIZE (1474560)  // 1.44MB

typedef struct {
    uint16_t bytesPerSector;
    uint8_t sectorsPerCluster;
    uint16_t reservedSectors;
    uint8_t numFATs;
    uint16_t rootEntryCount;
    uint16_t rootDirSectors;
    uint16_t totalSectors;
    uint16_t sectorsPerFAT;
    uint16_t fatStart;
    uint16_t rootStart;
    uint16_t dataStart;
} FatLayout;

// Computes layout fields based on image size
FatLayout compute_layout_from_size(size_t image_size) {
    FatLayout layout = {0};

    layout.bytesPerSector = SECTOR_SIZE;
    layout.sectorsPerCluster = 1;
    layout.reservedSectors = 1;
    layout.numFATs = 2;
    layout.rootEntryCount = 224;

    layout.rootDirSectors = ((layout.rootEntryCount * 32) + (SECTOR_SIZE - 1)) / SECTOR_SIZE;
    layout.totalSectors = (uint16_t)(image_size / SECTOR_SIZE);
    layout.sectorsPerFAT = 9;  // standard for FAT12, 1.44MB

    layout.fatStart = layout.reservedSectors;
    layout.rootStart = layout.fatStart + layout.numFATs * layout.sectorsPerFAT;
    layout.dataStart = layout.rootStart + layout.rootDirSectors;

    return layout;
}

void usage(const char *progname) {
    fprintf(stderr, "Usage: %s -i <image>\n", progname);
    exit(1);
}

int main(int argc, char *argv[]) {
    const char *image = NULL;
    size_t image_size = 0;

    // Parse args
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-i")) {
            if (++i >= argc) usage(argv[0]);
            image = argv[i];
        } else if (!strcmp(argv[i], "--version")) {
            printf("mformat version %s\n", VERSION);
            return 0;
        } else {
            usage(argv[0]);
        }
    }

    if (!image) usage(argv[0]);

    // Open or create image
    FILE *fp = fopen(image, "r+b");
    if (fp) {
        struct stat st;
        if (stat(image, &st) != 0) {
            perror("stat");
            fclose(fp);
            return 1;
        }
        image_size = st.st_size;
    } else {
        fp = fopen(image, "w+b");
        if (!fp) {
            perror("fopen");
            return 1;
        }
        image_size = DEFAULT_IMAGE_SIZE;
        if (fseek(fp, image_size - 1, SEEK_SET) != 0 || fputc(0, fp) == EOF) {
            perror("fseek/fputc");
            fclose(fp);
            return 1;
        }
    }

    // Compute layout
    FatLayout layout = compute_layout_from_size(image_size);

    // Boot sector
    uint8_t boot[SECTOR_SIZE] = {0};
    boot[0x00] = 0xEB;
    boot[0x01] = 0x3C;
    boot[0x02] = 0x90;
    memcpy(&boot[0x03], "MSDOS5.0", 8);
    boot[0x0B] = layout.bytesPerSector & 0xFF;
    boot[0x0C] = layout.bytesPerSector >> 8;
    boot[0x0D] = layout.sectorsPerCluster;
    boot[0x0E] = layout.reservedSectors & 0xFF;
    boot[0x0F] = layout.reservedSectors >> 8;
    boot[0x10] = layout.numFATs;
    boot[0x11] = layout.rootEntryCount & 0xFF;
    boot[0x12] = layout.rootEntryCount >> 8;
	boot[0x13] = layout.totalSectors & 0xFF;
	boot[0x14] = (layout.totalSectors >> 8) & 0xFF;
    boot[0x15] = 0xF0;  // media descriptor
    boot[0x16] = layout.sectorsPerFAT & 0xFF;
    boot[0x17] = layout.sectorsPerFAT >> 8;
    boot[0x18] = 0x12;  // sectors per track (dummy)
    boot[0x19] = 0x02;  // number of heads (dummy)
    boot[0x1FE] = 0x55;
    boot[0x1FF] = 0xAA;

    fseek(fp, 0, SEEK_SET);
    fwrite(boot, 1, SECTOR_SIZE, fp);

    // FATs
    uint8_t fat[SECTOR_SIZE] = {0};
    fat[0] = 0xF0;
    fat[1] = 0xFF;
    fat[2] = 0xFF;

    for (int i = 0; i < layout.numFATs; i++) {
        fseek(fp, layout.fatStart * SECTOR_SIZE + i * layout.sectorsPerFAT * SECTOR_SIZE, SEEK_SET);
        fwrite(fat, 1, SECTOR_SIZE, fp);
    }

    // Root directory
    fseek(fp, layout.rootStart * SECTOR_SIZE, SEEK_SET);
    uint8_t empty[SECTOR_SIZE] = {0};
    for (int i = 0; i < layout.rootDirSectors; i++) {
        fwrite(empty, 1, SECTOR_SIZE, fp);
    }

    fclose(fp);
    printf("Formatted FAT12 image: %s\n", image);
    return 0;
}