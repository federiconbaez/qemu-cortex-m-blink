# qemu-cortex-m-blink

Firmware bare-metal para Cortex-M3 que hace parpadear "SOS" en Morse sobre el LED (PF0) del LM3S6965, desde la interrupción de SysTick, en `qemu-system-arm -M lm3s6965evb`. Se verifica a sí mismo y sale de QEMU por semihosting con el número de fallos como código de salida.

Bare-metal Cortex-M3 firmware that blinks "SOS" in Morse on the LM3S6965 LED (PF0) from the SysTick interrupt, under `qemu-system-arm -M lm3s6965evb`. It checks itself and leaves QEMU through semihosting with the failure count as the exit code.

```
$ make && make qemu-test
qemu-cortex-m-blink
ok   .data copied from flash
...
ok   hardware edges match the software replay
edge 0 1
edge 2 0
...
stack high water 156 of 4096 bytes
PASS
hardfault pc=0x000002f2 cfsr=0x00010000 hfsr=0x40000000
qemu-test: all checks passed
```

## Español

**Qué hay**

- Sin libc ni CMSIS: tabla de vectores, `Reset_Handler` (copia `.data`, pone `.bss` en cero, pinta la pila), script de linker y los registros justos.
- `src/morse.c`: compila texto a pasos encendido/apagado con los tiempos ITU (punto 1, raya 3, separaciones 1/3/7) y un reproductor que avanza un tick por vez. Es freestanding: el mismo archivo corre en los tests del host y en el Cortex-M3.
- SysTick a 1 kHz; cada interrupción avanza el reproductor, escribe PF0 con la escritura enmascarada del GPIO y **lee el pin de vuelta** del registro. Los flancos se registran a partir de esa lectura, no del estado del software.
- Salida por UART0 (PL011) y fin por semihosting `SYS_EXIT_EXTENDED`: QEMU termina con el código que devuelve `main`.
- Una segunda imagen (`-DFAULT_TEST`) ejecuta `udf`; el `HardFault_Handler` elige MSP/PSP, imprime el PC apilado, CFSR y HFSR, y sale con código 3.

**Pruebas**

- Host (gcc y clang, ASan + UBSan): tabla ITU completa contra una copia independiente y sin patrones repetidos; "PARIS " dura exactamente 50 unidades (la palabra estándar); espacios iniciales, repetidos y finales; carácter inválido con su índice; todas las capacidades insuficientes devuelven `ENOSPC`; la línea de tiempo de "SOS " a 2 ticks por unidad contra flancos derivados a mano.
- En QEMU, 15 chequeos: `.data`/`.bss`; escritura y lectura enmascaradas de GPIO y pin de entrada que ignora escrituras; SysTick con `wfi` (una despertada por tick); 2 vueltas de "SOS " = 136 ticks y 36 flancos; flancos del hardware iguales a la reproducción por software; LED apagado al final; pila usada por debajo de la mitad.
- El script compara los flancos de la UART con `tests/golden_edges.txt` (escrito a mano), exige código 0 y `PASS`, y para la imagen de falla exige código 3, `cfsr=0x00010000` (UNDEFINSTR), `hfsr=0x40000000` (FORCED) y que el PC reportado sea la dirección del `udf` según `objdump`.
- Mutaciones de control: 3 ticks por unidad en vez de 2 → falla el golden; manejar PF1 en vez de PF0 → fallan 2 chequeos.

**Medido** (arm-none-eabi-gcc 14.2, `-Os`, QEMU 10.0): `.text` 2852 B, `.data` 4 B, `.bss` 504 B; pico de pila 156 B de los 4096 reservados.

**Límites**: QEMU modela el GPIO y SysTick, no la temporización eléctrica; la frecuencia de CPU está fijada en 12 MHz nominal y ningún chequeo depende del tiempo real. No se probó en una placa física.

## English

**What is here**

- No libc, no CMSIS: vector table, `Reset_Handler` (copies `.data`, zeroes `.bss`, paints the stack), linker script and just the registers needed.
- `src/morse.c`: compiles text into on/off steps with ITU timing (dot 1, dash 3, gaps 1/3/7) and a player that advances one tick at a time. It is freestanding: the same file runs in the host tests and on the Cortex-M3.
- SysTick at 1 kHz; each interrupt advances the player, writes PF0 through the GPIO masked write and **reads the pin back** from the register. Edges are recorded from that readback, not from software state.
- Output on UART0 (PL011) and exit through semihosting `SYS_EXIT_EXTENDED`: QEMU exits with the code `main` returns.
- A second image (`-DFAULT_TEST`) executes `udf`; `HardFault_Handler` picks MSP/PSP, prints the stacked PC, CFSR and HFSR, and exits with code 3.

**Tests**

- Host (gcc and clang, ASan + UBSan): full ITU table against an independent copy and with no duplicate patterns; "PARIS " is exactly 50 units (the standard word); leading, repeated and trailing spaces; invalid character with its index; every short capacity returns `ENOSPC`; the "SOS " timeline at 2 ticks per unit against hand-derived edges.
- In QEMU, 15 checks: `.data`/`.bss`; GPIO masked write and read and an input pin that ignores writes; SysTick with `wfi` (one wakeup per tick); 2 loops of "SOS " = 136 ticks and 36 edges; hardware edges equal to the software replay; LED off at the end; stack use below half.
- The script diffs the UART edges against `tests/golden_edges.txt` (written by hand), requires exit 0 and `PASS`, and for the fault image requires exit 3, `cfsr=0x00010000` (UNDEFINSTR), `hfsr=0x40000000` (FORCED) and a reported PC equal to the `udf` address per `objdump`.
- Control mutations: 3 ticks per unit instead of 2 → the golden fails; driving PF1 instead of PF0 → 2 checks fail.

**Measured** (arm-none-eabi-gcc 14.2, `-Os`, QEMU 10.0): `.text` 2852 B, `.data` 4 B, `.bss` 504 B; stack peak 156 B of the 4096 reserved.

**Limits**: QEMU models GPIO and SysTick, not electrical timing; the CPU clock is a nominal 12 MHz and no check depends on wall time. Not tried on a physical board.

## Build

```
sudo apt-get install gcc-arm-none-eabi qemu-system-arm
make host-test && make && make qemu-test
```

## License

Apache-2.0
