
Here’s the classic set of **mtools** utilities, grouped by what they do (with the canonical one‑line descriptions):

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
- `mpartition` – create an MS‑DOS partition
- `mmount` – mount an MS‑DOS disk (where supported)
- `mzip` – Zip/Jaz disk specific commands
- `mclasserase` – erase a memory card
- `mbadblocks` – test a floppy and mark bad blocks. [GNU](https://www.gnu.org/s/mtools/manual/html_node/Commands.html)

**Info / diagnostics / helpers**

- `minfo` – show filesystem information (BPB, geometry, etc.)
- `mdu` – report space used by a directory tree
- `mshowfat` – show the FAT map of a file
- `mkmanifest` – list short‑name equivalents for long filenames
- `mtoolstest` – test/display configuration. [GNU](https://www.gnu.org/s/mtools/manual/html_node/Commands.html)

**Daemon / special**

- `floppyd` – floppy‑access daemon (for remote/X setups)
- `floppyd_installtest` – check for `floppyd`
- `mcat` – `cat` via `floppyd` (mostly useful with the daemon). [GNU](https://www.gnu.org/s/mtools/manual/html_node/Commands.html)


mformat.exe -i floppy.img ::
  - by default will create 1M image file.

mdir -i floppy.img ::
minfo -i floopy.img ::

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
