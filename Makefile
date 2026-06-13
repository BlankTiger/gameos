build: src
	mkdir -p build
	i686-elf-as src/boot.s -o build/boot.o
	i686-elf-g++ -c src/kernel.cc -o build/kernel.o -ffreestanding -O2 -Wall -Wextra -fno-exceptions -fno-rtti
	i686-elf-g++ -T src/linker.ld -o build/gameos -ffreestanding -O2 -nostdlib build/boot.o build/kernel.o -lgcc
	@grub-file --is-x86-multiboot build/gameos || echo "Something went wrong and kernel is not multiboot compliant."

iso: build
	mkdir -p build/isodir/boot/grub
	cp build/gameos build/isodir/boot/gameos
	cp grub.cfg build/isodir/boot/grub/grub.cfg
	grub-mkrescue -o build/gameos.iso build/isodir
