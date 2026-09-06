/**
 * Marlin 3D Printer Firmware
 * Copyright (c) 2020 MarlinFirmware [https://github.com/MarlinFirmware/Marlin]
 *
 * Based on Sprinter and grbl.
 * Copyright (c) 2011 Camiel Gubbels / Erik van der Zalm
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#include "../../inc/MarlinConfig.h"

#if ENABLED(KAE_SD_BINARY_BASE64_DOWNLOAD)

#include "../gcode.h"
#include "../../sd/cardreader.h"
#include "../../MarlinCore.h"

static int16_t kae_base64_encode(const uint8_t*, uint16_t, char*, uint16_t);

void GcodeSuite::M6400() {
  if (!card.isMounted()) {
    SERIAL_ECHO_MSG(STR_NO_MEDIA);
    return;
  }

  const char* const filename = parser.string_arg;
  if (!filename || !*filename) {
    SERIAL_ERROR_MSG(STR_B64_ERR_NOFILE);
    return;
  }

  if (IS_SD_PRINTING()) {
    SERIAL_ERROR_MSG(STR_B64_ERR_PRINTING);
    return;
  }

  MediaFile* diveDir;
  const char* const fname = card.diveToFile(true, diveDir, filename);
  if (!fname) {
    SERIAL_ERROR_START();
    SERIAL_ECHOPGM(STR_B64_ERR_OPEN_FAIL);
    SERIAL_CHAR(' ');
    SERIAL_ECHO("diveToFile(");
    SERIAL_ECHO(filename);
    SERIAL_ECHOLN(") failed");
    return;
  }

  MediaFile file;
  if (!file.open(diveDir, fname, O_READ)) {
    SERIAL_ERROR_START();
    SERIAL_ECHOPGM(STR_B64_ERR_OPEN_FAIL);
    SERIAL_CHAR(' ');
    SERIAL_ECHOLN(fname);
    return;
  }

  SERIAL_ECHOPGM(STR_B64_BEGIN);
  SERIAL_CHAR(' ');
  SERIAL_ECHO(filename);
  SERIAL_CHAR(' ');
  SERIAL_ECHOLN(file.fileSize());

  uint8_t input[48];
  char output[65];
  bool failure = false;

  uint16_t input_length = 0;

  while (true) {
    const int16_t bytes_read = file.read(
        input + input_length,
        sizeof(input) - input_length);

    if (bytes_read < 0) {
      SERIAL_ERROR_MSG(STR_B64_ERR_READ_FAIL, bytes_read);
      failure = true;
      break;
    }

    if (bytes_read == 0) {
      // EOF. Encode any final partial block.
      if (input_length > 0) {
        const int16_t output_length = kae_base64_encode(
            input,
            input_length,
            output,
            sizeof(output) - 1);

        if (output_length < 0) {
          SERIAL_ERROR_MSG(STR_B64_ERR_OVERRUN);
          failure = true;
          break;
        }

        output[output_length] = '\0';
        SERIAL_ECHOPGM(STR_B64_DATA);
        SERIAL_CHAR(' ');
        SERIAL_ECHOLN(output);
      }

      break;
    }

    input_length += bytes_read;

    if (input_length == sizeof(input)) {
      const int16_t output_length = kae_base64_encode(
          input,
          input_length,
          output,
          sizeof(output) - 1);

      if (output_length < 0) {
        SERIAL_ERROR_MSG(STR_B64_ERR_OVERRUN);
        failure = true;
        break;
      }

      output[output_length] = '\0';
      SERIAL_ECHOPGM(STR_B64_DATA);
      SERIAL_CHAR(' ');
      SERIAL_ECHOLN(output);

      input_length = 0;
    }

    idle();
  }

  if (failure) {
    SERIAL_ECHOLNPGM(STR_B64_FAILURE);
  } else {
    SERIAL_ECHOLNPGM(STR_B64_END);
  }

  file.close();
}

static int16_t kae_base64_encode(
    const uint8_t *raw,
    uint16_t nraw,
    char *output,
    uint16_t nout)
{
  static constexpr char base64[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789+/";

  if ((!raw && nraw) || !output)
    return -1;

  // Base64 requires 4 output bytes for every 3 input bytes,
  // rounding the input length up to the next group of 3.
  const uint32_t required = 4UL * ((nraw + 2UL) / 3UL);

  // Check the complete required size before writing anything.
  // This guarantees that no partial output and no out-of-bounds
  // write can occur.
  if (required > nout || required > INT16_MAX)
    return -1;

  uint16_t i = 0;
  uint16_t o = 0;

  while (nraw - i >= 3) {
    const uint8_t a = raw[i++];
    const uint8_t b = raw[i++];
    const uint8_t c = raw[i++];

    output[o++] = base64[a >> 2];
    output[o++] = base64[((a & 0x03) << 4) | (b >> 4)];
    output[o++] = base64[((b & 0x0F) << 2) | (c >> 6)];
    output[o++] = base64[c & 0x3F];
  }

  const uint16_t remaining = nraw - i;

  if (remaining == 1) {
    const uint8_t a = raw[i];

    output[o++] = base64[a >> 2];
    output[o++] = base64[(a & 0x03) << 4];
    output[o++] = '=';
    output[o++] = '=';
  }
  else if (remaining == 2) {
    const uint8_t a = raw[i++];
    const uint8_t b = raw[i];

    output[o++] = base64[a >> 2];
    output[o++] = base64[((a & 0x03) << 4) | (b >> 4)];
    output[o++] = base64[(b & 0x0F) << 2];
    output[o++] = '=';
  }

  return (int16_t)o;
}

#endif
