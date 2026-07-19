# Development

    nix develop -c fish
    cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=cmake/Toolchain-i686-elf.cmake && cmake --build build

# Running

    qemu-system-i386 -serial stdio -cdrom build/gameos.iso

# Terminal interface (switched by using a different namespace)

Must implement all forward declared functions in `kstd/term.hh`. Management of
the current row, column, scrolling should be handled internally.

# TODO:

8-bit slowly drifting

# Abbreviations used

## Serial / UART registers / signals

- IER - Interrupt Enable Register (the UART register that masks which events generate interrupts)
- FCR - FIFO Control Register (enables and configures the UART's send/receive FIFO buffers)
- LCR - Line Control Register (sets data bits, stop bits, parity, and the DLAB mode switch)
- MCR - Modem Control Register (drives the DTR/RTS/OUT2 handshake lines)
- LSR - Line Status Register (reports transmit-ready, data-ready, and error conditions)
- DLAB - Divisor Latch Access Bit (LCR bit 7; repurposes two UART registers into the baud-rate divisor)
- THR - Transmitter Holding Register (holds the next byte waiting to be shifted out)
- THRE - Transmitter Holding Register Empty (LSR bit meaning the UART is ready for another byte)
- DTR - Data Terminal Ready (MCR handshake line signaling the local end is ready to communicate)
- RTS - Request To Send (MCR handshake line signaling the local end wants to transmit)
- FIFO - First In, First Out (the UART's internal send/receive buffer queues)
