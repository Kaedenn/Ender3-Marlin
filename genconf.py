#!/usr/bin/env python3

"""
Generate Configuration.M
"""

import argparse
import logging
import os
import subprocess

logging.basicConfig(format="%(module)s:%(lineno)s: %(levelname)s: %(message)s",
                    level=logging.INFO)
logger = logging.getLogger(__name__)

PACKAGES = "/home/kaedenn/.platformio/packages"
# pylint: disable=line-too-long
CLARGS = [
  "-c",
  "-fno-sized-deallocation",
  "-Wno-register",
  "-std=gnu++14",
  "-fno-threadsafe-statics",
  "-fno-rtti",
  "-fno-exceptions",
  "-fno-use-cxa-atexit",
  "-g3",
  "-fmax-errors=5",
  "-funwind-tables",
  "-Os",
  "-mcpu=cortex-m3",
  "-mthumb",
  "-ffunction-sections",
  "-fdata-sections",
  "-Wall",
  "-nostdlib",
  "--param", "max-inline-insns-single=500",
  "-DPLATFORMIO=60200",
  "-DSTM32F103xE",
  "-DSTM32F1",
  "-D__MARLIN_FIRMWARE__",
  "-DNDEBUG",
  "-DHAL_STM32",
  "-DPLATFORM_M997_SUPPORT",
  "-DTIM_IRQ_PRIO=13",
  "-DADC_RESOLUTION=12",
  "-DMCU_STM32F103RE",
  "-DHAL_SD_MODULE_ENABLED",
  "-DSS_TIMER=4",
  "-DTIMER_SERVO=TIM5",
  "-DENABLE_HWSERIAL3",
  "-DTRANSFER_CLOCK_DIV=8",
  "-DBOARD_F_CPU=72000000L",
  "-DSERIAL_RX_BUFFER_SIZE=128",
  "-DSERIAL_TX_BUFFER_SIZE=64",
  "-DUSART_RX_BUF_SIZE=128",
  "-DUSART_TX_BUF_SIZE=64",
  "-DSTM32F1xx",
  "-DARDUINO=10808",
  "-DARDUINO_ARCH_STM32",
  "-DARDUINO_GENERICSTM32F103RE",
  "-DBOARD_NAME=\"GENERICSTM32F103RE\"",
  "-DHAL_UART_MODULE_ENABLED",
  "-DHAL_PCD_MODULE_ENABLED",
  "-IMarlin",
  "-I.",
  f"-I{PACKAGES}/framework-arduinoststm32/libraries/Servo/src",
  f"-I{PACKAGES}/framework-arduinoststm32/libraries/SoftwareSerial/src",
  f"-I{PACKAGES}/framework-arduinoststm32/libraries/EEPROM/src",
  f"-I{PACKAGES}/framework-arduinoststm32/libraries/IWatchdog/src",
  "-I.pio/libdeps/STM32F103RE_creality/U8glib-HAL/src",
  f"-I{PACKAGES}/framework-arduinoststm32/libraries/SPI/src",
  f"-I{PACKAGES}/framework-arduinoststm32/libraries/Wire/src",
  f"-I{PACKAGES}/framework-arduinoststm32/cores/arduino/avr",
  f"-I{PACKAGES}/framework-arduinoststm32/cores/arduino/stm32",
  f"-I{PACKAGES}/framework-arduinoststm32/cores/arduino/stm32/LL",
  f"-I{PACKAGES}/framework-arduinoststm32/cores/arduino/stm32/usb",
  f"-I{PACKAGES}/framework-arduinoststm32/cores/arduino/stm32/OpenAMP",
  f"-I{PACKAGES}/framework-arduinoststm32/cores/arduino/stm32/usb/hid",
  f"-I{PACKAGES}/framework-arduinoststm32/cores/arduino/stm32/usb/cdc",
  f"-I{PACKAGES}/framework-arduinoststm32/system/Drivers/STM32F1xx_HAL_Driver/Inc",
  f"-I{PACKAGES}/framework-arduinoststm32/system/Drivers/STM32F1xx_HAL_Driver/Src",
  f"-I{PACKAGES}/framework-arduinoststm32/system/STM32F1xx",
  f"-I{PACKAGES}/framework-arduinoststm32/system/Middlewares/ST/STM32_USB_Device_Library/Core/Inc",
  f"-I{PACKAGES}/framework-arduinoststm32/system/Middlewares/ST/STM32_USB_Device_Library/Core/Src",
  f"-I{PACKAGES}/framework-arduinoststm32/system/Middlewares/OpenAMP",
  f"-I{PACKAGES}/framework-arduinoststm32/system/Middlewares/OpenAMP/open-amp/lib/include",
  f"-I{PACKAGES}/framework-arduinoststm32/system/Middlewares/OpenAMP/libmetal/lib/include",
  f"-I{PACKAGES}/framework-arduinoststm32/system/Middlewares/OpenAMP/virtual_driver",
  f"-I{PACKAGES}/framework-cmsis/CMSIS/Core/Include",
  f"-I{PACKAGES}/framework-arduinoststm32/system/Drivers/CMSIS/Device/ST/STM32F1xx/Include",
  f"-I{PACKAGES}/framework-arduinoststm32/system/Drivers/CMSIS/Device/ST/STM32F1xx/Source/Templates/gcc",
  f"-I{PACKAGES}/framework-cmsis/CMSIS/DSP/Include",
  f"-I{PACKAGES}/framework-arduinoststm32/cores/arduino",
  "-Ibuildroot/share/PlatformIO/variants/MARLIN_F103Rx",
  "-E", "-dM",
]
# pylint: enable=line-too-long

def gen_command(srcfile, destfile, *extra_args):
  "Generate a final command suitable for compiling srcfile into destfile"
  args = ["/home/kaedenn/.platformio/packages/toolchain-gccarmnoneeabi/bin/arm-none-eabi-g++"]
  args.extend(CLARGS)
  args.append(srcfile)
  args.extend(("-o", destfile))
  if extra_args:
    args.extend(extra_args)
  if len(args) != len(set(args)):
    dupes = {arg for arg in args if args.count(arg) > 1}
    logger.warning("%d duplicate argument(s): %s",
        len(args) - len(set(args)),
        dupes)
  return args

def matches_any(srcline, dstlines, invert=False):
  "Does the source line match any of the lines in dstlines?"
  for line in dstlines:
    if invert ^ line.startswith(srcline):
      return True
  return False

def sanitize_macro(line):
  "Remove any comments from the line"
  line = line.strip()
  if "//" in line:
    line = line[:line.index("//")].strip()
  if "/*" in line:
    line = line[:line.index("/*")].strip()
  return line

def count_lines(fname):
  "Count the number of lines in fname"
  with open(fname, "rt", encoding="UTF-8") as fobj:
    return len(fobj.read().splitlines())

def compile_program(srcfile, destfile, *clargs):
  "Compile srcfile to destfile"
  command = gen_command(srcfile, destfile, *clargs)
  logger.info("Compiling %s to %s using %s", srcfile, destfile, command[0])
  subprocess.check_call(command)
  logger.info("Wrote %d lines to %s", count_lines(destfile), destfile)

def deduplicate(sequence):
  "Remove duplicates while preserving order"
  results = []
  seen = set()
  for entry in sequence:
    if entry in seen:
      continue
    seen.add(entry)
    results.append(entry)
  return results

def main():
  ap = argparse.ArgumentParser()
  ap.add_argument("-o", "--output", metavar="PATH", default="Configuration.M",
      help="write macros to %(metavar)s")
  ap.add_argument("-k", "--keep", action="store_true",
      help="do not purge temporary files")
  ap.add_argument("-g", "--gen-debug", action="store_true",
      help="generate files to assist in debugging")
  ap.add_argument("-v", "--verbose", action="store_true", help="verbose output")
  args = ap.parse_args()
  if args.verbose:
    logger.setLevel(logging.DEBUG)

  srcfile = "/tmp/genconf.c"
  basefile = "/tmp/genconf-base.E"
  destfile = "/tmp/genconf.E"
  with open(srcfile, "wt", encoding="UTF-8") as fobj:
    fobj.write(os.linesep)
  compile_program(srcfile, basefile)
  with open(srcfile, "wt", encoding="UTF-8") as fobj:
    fobj.write('#include "Marlin/src/inc/MarlinConfig.h"\n')
  compile_program(srcfile, destfile)

  sanitized_base = []
  sanitized_dest = []
  sanitized_ref = []
  with open(basefile, "rt", encoding="UTF-8") as fobj:
    for line in fobj.read().splitlines():
      sanitized_base.append(sanitize_macro(line))
  with open(destfile, "rt", encoding="UTF-8") as fobj:
    for line in fobj.read().splitlines():
      sanitized_dest.append(sanitize_macro(line))
  for conffile in ["Marlin/Configuration.h", "Marlin/Configuration_adv.h"]:
    with open(conffile, "rt", encoding="UTF-8") as fobj:
      for line in fobj.read().splitlines():
        if line.lstrip().startswith("#define"):
          sanitized_ref.append(sanitize_macro(line))
  sanitized_ref = deduplicate(sanitized_ref)

  if args.gen_debug:
    with open("genconf-base-debug.E", "wt", encoding="UTF-8") as fobj:
      fobj.write(os.linesep.join(sanitized_base))
      fobj.write(os.linesep)
    with open("genconf-debug.E", "wt", encoding="UTF-8") as fobj:
      fobj.write(os.linesep.join(sanitized_dest))
      fobj.write(os.linesep)
    with open("genconf-ref-debug.E", "wt", encoding="UTF-8") as fobj:
      fobj.write(os.linesep.join(sanitized_ref))
      fobj.write(os.linesep)
    logger.info("Generated genconf-base-debug.E, genconf-debug.E, and genconf-ref-debug.E")

  logger.debug("Removing macros not defined via configuration headers...")
  config_macros = []
  for line in sanitized_ref:
    if any(line in srcline for srcline in sanitized_dest):
      config_macros.append(line)

  logger.info("Writing %d lines to %s", len(config_macros), args.output)
  with open(args.output, "wt", encoding="UTF-8") as fobj:
    fobj.write(os.linesep.join(config_macros))
    fobj.write(os.linesep)

  if not args.keep:
    logger.info("Removing intermediate files")
    os.unlink(srcfile)
    os.unlink(basefile)
    os.unlink(destfile)

if __name__ == "__main__":
  main()

# vim: set ts=2 sts=2 sw=2:
