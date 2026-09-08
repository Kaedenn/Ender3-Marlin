#!/usr/bin/env python3

"""
Extract the embedded configuration from a firmware image.

This requires CONFIGURATION_EMBEDDING to work.

This script will either dump the .zip archive, or it will extract the
JSON file within the .zip archive, depending on whether or not
-x,--extract was supplied.
"""

import argparse
import logging
from io import BytesIO
import os
import sys
import struct
from zipfile import ZipFile

logging.basicConfig(format="%(module)s:%(lineno)s: %(levelname)s: %(message)s",
                    level=logging.INFO)
logger = logging.getLogger(__name__)

def read_mczip(binpath):
  "Obtain the contents of mc.zip from the given binary"
  with open(binpath, "rb") as fobj:
    data = fobj.read()

  # ZIP local-file-header signature
  start = data.find(b"PK\x03\x04")
  if start < 0:
    raise ValueError("Zip start signature not found")

  # End-of-central-directory signature.
  #
  # Search backwards so that an accidental PK\x05\x06 sequence in file
  # contents doesn't fool us.
  eocd = data.rfind(b"PK\x05\x06", start)
  if eocd < 0:
    raise ValueError("ZIP end-of-central-directory record not found")

  # EOCD is 22 bytes minimum. Bytes 20-21 contain the comment length.
  if eocd + 22 > len(data):
    raise ValueError("Truncated ZIP end-of-central-directory record")

  comment_len = struct.unpack_from("<H", data, eocd + 20)[0]
  end = eocd + 22 + comment_len

  if end > len(data):
    raise ValueError("ZIP end extends beyond firmware image")

  return data[start:end]

def main():
  ap = argparse.ArgumentParser(description="Extract embedded configuration")
  ap.add_argument("bin", help="path to the firmware binary")
  ap.add_argument("-o", "--output",
      help="write to file (default: mc.zip or marlin_config.json)")
  ap.add_argument("-x", "--extract", action="store_true",
      help="extract firmware JSON to the -o,--output location")
  ap.add_argument("-v", "--verbose", action="store_true", help="verbose output")
  args = ap.parse_args()
  if args.verbose:
    logger.setLevel(logging.DEBUG)

  try:
    data = read_mczip(args.bin)
  except ValueError as err:
    logger.error(err)
    raise SystemExit(1) from err

  output = args.output
  if args.extract:
    if not output:
      output = "mc.zip"
    logger.info("Extracting %s to %s", args.bin, output)
    with open(output, "wb") as fobj:
      fobj.write(data)
  else:
    if not output:
      output = "marlin_config.json"
    with ZipFile(BytesIO(data), "r") as zfobj:
      with zfobj.open("marlin_config.json", "r") as jsonfobj:
        logger.info("Extracting marlin_config.json from %s to %s", args.bin, output)
        with open(output, "wt", encoding="UTF-8") as fobj:
          fobj.write(jsonfobj.read().decode())

if __name__ == "__main__":
  main()

# vim: set ts=2 sts=2 sw=2:
