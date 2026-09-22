# Steps to compile Marlin

## make:
`pio run -e STM32F103RE_creality`

## make clean:
`pio run -t clean -e STM32F103RE_creality`

## make test:
```
pio run -e kae_simulator
.pio/build/kae_simulator/MarlinSimulator
```
