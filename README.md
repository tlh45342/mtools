**Purpose**

This is a replacment? for some mtools functions as MTOOLS for Windows does not seem to be repliably applicable. I think some of this stuff has existed at some time, but the facts are there are some CYGWIN64 items that I think have existed in the past but can't be relied on currently.

I am not even going to touch "config" for Windows as I don't think that is mature - or has been broken; or it's probably just me and I am invoking it wrong.

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

mformat -i flooopy.img ::
mdir -i floppy.img ::
minfo -i floopy.img ::
mdel -i floppy.img file.txt
mcp -i floppy.img hello.txt
mdir -i floppy.img ::

## INSTALLATION

```bash
cd /opt
git clone https://github.com/tlh45342/mtools.git
cd mtools
make ; make install
```

## WARNING

This has NOT been tested hardly at all.  Use at your own risk.  Do NOT use this on a production system.
