dd if=/dev/zero of=floppy.img bs=1k count=4096
mformat -i floppy.img
mcp -i floppy.img hello.txt
mdir -i floppy.img