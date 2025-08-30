**File & directory operations**

- `mdir` – display an MS‑DOS directory
- `mcopy` – copy files to/from MS‑DOS
- `mdel` – delete a file
- `mdeltree` – recursively delete a directory
- `mmd` – make a subdirectory
- `mrd` – remove a subdirectory
- `mren` – rename a file
- `mmove` – move/rename a file or directory
- `mattrib` – change attribute flags (R/H/S/A)
- `mtype` – print a file’s contents
- `mshortname` – show a file’s 8.3 short name. [GNU](https://www.gnu.org/s/mtools/manual/html_node/Commands.html)

**Disk / filesystem utilities**

- `mformat` – create an MS‑DOS (FAT) filesystem on a disk/image
- `mlabel` – set the volume label



mformat.exe -i floppy.img ::

  - by default will create 1M image file.

mdir -i floppy.img ::
minfo -i floopy.img ::
mdel -i floppy.img file.txt
mcp -i floppy.img hello.txt
mdir -i floppy.img ::



### 🛠️ Sample usage

```
mdel -i floppy.img FOO.TXT
```

Note:

- This implementation assumes FAT12/FAT16 where the root directory is in a fixed location.
- It does **not** currently support subdirectories or FAT32 root in data clusters.
- File name matching is simplistic. A more robust approach would normalize the 8.3 format more carefully.

1. 

------

## ✅ Example 1: Create a 4MB FAT12/16 disk image

```
dd if=/dev/zero of=floppy.img bs=1k count=4096
```

Then:

```
mformat -i floppy.img
```

> Assumes the default is FAT12 or FAT16 based on image size and implementation logic.

------

## 
