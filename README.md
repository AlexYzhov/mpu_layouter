# mpu_layouter
ARMv7-M MPU region layouter for given memory blocks

- This tool generates layouts of mpu region settings for given memory blocks
  - this program is only a 1 pass greedy implementation for now
  - for more compact layout, function `merge_region()` should be completed
- the print style of describing memory blocks in this program is familiar with practice language of lauterbach
  - `[base_addr--size]` or `[base_addr++end_addr]`

---

first of all, memory blocks can be modifed by changing the implementation of C macro `MEMORIES` in `main.c`:

```
#ifndef MEMORIES
#define MEMORIES {\
    {"rom", 0x00000000, KB(1024)},\
    {"sram", 0x20000000, KB(384)},\
    {"dram", 0x60000000, KB(1000)},\
}
#endif
```

build program without debug logs (RELEASE):

```
gcc main.c -o main -DNDEBUG # no debug logs
```

build program with debug logs (DEBUG):

```
gcc main.c -o main # with debug logs
```

execute & report (if layout is successfully generated):

```
./main
[0x00000000--0x00100000] rom[0]: [0x00000000--0x00080000, 0x00]
[0x00000000--0x00100000] rom[1]: [0x00080000--0x00080000, 0x00]

[0x20000000--0x00060000] sram[0]: [0x20000000--0x00040000, 0x00]
[0x20000000--0x00060000] sram[1]: [0x20040000--0x00020000, 0x00]

[0x60000000--0x000fa000] dram[0]: [0x60000000--0x00080000, 0x00]
[0x60000000--0x000fa000] dram[1]: [0x60080000--0x00040000, 0x00]
[0x60000000--0x000fa000] dram[2]: [0x600c0000--0x00020000, 0x00]
[0x60000000--0x000fa000] dram[3]: [0x600e0000--0x00010000, 0x00]
[0x60000000--0x000fa000] dram[4]: [0x600f0000--0x00008000, 0x00]
[0x60000000--0x000fa000] dram[5]: [0x600f8000--0x00002000, 0x00]
```

execute & report (if layout is failed to generate):

```
./main
[0x00000005--0x00100000] rom: failed to generate mpu layout!!
[0x2000007d--0x00060000] sram: failed to generate mpu layout!!
[0x60000301--0x000fa000] dram: failed to generate mpu layout!!
```
