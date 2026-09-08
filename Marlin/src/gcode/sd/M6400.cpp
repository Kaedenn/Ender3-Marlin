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

#define MAX_FILE_LENGTH 80

struct M6400Config {
  bool prefixed;        // R: Omit prefix
  enum {                // A: Output ASCII
    TEXT,
    BASE64
  } mode;
  char filename[MAX_FILE_LENGTH];
};

static bool emit_text(const uint8_t*, const int16_t, const M6400Config&);

static bool parse_M6400_args(GCodeParser &parser, M6400Config &config);

static int16_t kae_base64_encode(const uint8_t*, uint16_t, char*, uint16_t);

static char *createFilename(char * const buffer, const dir_t &p);

void GcodeSuite::M6400() {
  if (!card.isMounted()) {
    SERIAL_ECHO_MSG(STR_NO_MEDIA);
    return;
  }

  if (IS_SD_PRINTING()) {
    SERIAL_ERROR_MSG(STR_B64_ERR_PRINTING);
    return;
  }

  M6400Config config = {0};
  config.prefixed = true;
  config.mode = M6400Config::BASE64;
  if (!parser.string_arg || !*parser.string_arg) {
    SERIAL_ERROR_MSG(STR_B64_ERR_NOFILE);
    return;
  }
  if (!parse_M6400_args(parser, config)) {
    SERIAL_ERROR_MSG(STR_B64_ERR_PARSE_FAIL);
    return;
  }

  MediaFile* diveDir;
  const char* const realname = card.diveToFile(true, diveDir, config.filename);
  if (!realname) {
    SERIAL_ERROR_START();
    SERIAL_ECHOPGM(STR_B64_ERR_OPEN_FAIL);
    SERIAL_CHAR(' ');
    SERIAL_ECHO("diveToFile(");
    SERIAL_ECHO(config.filename);
    SERIAL_ECHOLN(") failed");
    return;
  }

  MediaFile file;
  if (!file.open(diveDir, realname, O_READ)) {
    SERIAL_ERROR_START();
    SERIAL_ECHOPGM(STR_B64_ERR_OPEN_FAIL);
    SERIAL_CHAR(' ');
    SERIAL_ECHOLN(realname);
    return;
  }

  SERIAL_ECHOPGM(STR_B64_BEGIN);
  SERIAL_CHAR(' ');
  SERIAL_ECHO(config.filename);
  SERIAL_CHAR(' ');
  SERIAL_ECHOLN(file.fileSize());

  uint8_t input[48];
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

    input_length += bytes_read;

    if (input_length == sizeof(input) || (bytes_read == 0 && input_length > 0)) {
      if (!emit_text(input, input_length, config)) {
        failure = true;
        break;
      }
      input_length = 0;
    }

    if (bytes_read == 0) {
      break;
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

void GcodeSuite::M6401() {
  if (!card.isMounted()) {
    SERIAL_ECHO_MSG(STR_NO_MEDIA);
    return;
  }

  if (IS_SD_PRINTING()) {
    SERIAL_ERROR_MSG(STR_B64_ERR_PRINTING);
    return;
  }

  MediaFile parent = CardReader::getroot();
  SERIAL_ECHOLNPGM(STR_BEGIN_FILE_LIST);
  dir_t p;
  char longFilename[LONG_FILENAME_LENGTH] = {0};
  char shortFilename[FILENAME_LENGTH] = {0};
  while (parent.readDir(&p, longFilename)) {
    SERIAL_ECHO(createFilename(shortFilename, p));
    SERIAL_CHAR(' ');
    if (longFilename[0]) {
      SERIAL_ECHO(longFilename);
      SERIAL_CHAR(' ');
    }
    SERIAL_ECHO("Attributes: 0x");
    SERIAL_ECHO(p.attributes);
    SERIAL_CHAR(' ');

    if (p.name[0] == DIR_NAME_0xE5)
      SERIAL_ECHO("DIR_NAME_0xE5 ");
    if (p.name[0] == DIR_NAME_DELETED)
      SERIAL_ECHO("DIR_NAME_DELETED ");
    if (p.name[0] == DIR_NAME_FREE)
      SERIAL_ECHO("DIR_NAME_FREE ");
    if ((p.attributes & DIR_ATT_READ_ONLY) == DIR_ATT_READ_ONLY)
      SERIAL_ECHO("DIR_ATT_READ_ONLY ");
    if ((p.attributes & DIR_ATT_HIDDEN) == DIR_ATT_HIDDEN)
      SERIAL_ECHO("DIR_ATT_HIDDEN ");
    if ((p.attributes & DIR_ATT_SYSTEM) == DIR_ATT_SYSTEM)
      SERIAL_ECHO("DIR_ATT_SYSTEM ");
    if ((p.attributes & DIR_ATT_VOLUME_ID) == DIR_ATT_VOLUME_ID)
      SERIAL_ECHO("DIR_ATT_VOLUME_ID ");
    if ((p.attributes & DIR_ATT_DIRECTORY) == DIR_ATT_DIRECTORY)
      SERIAL_ECHO("DIR_ATT_DIRECTORY ");
    if ((p.attributes & DIR_ATT_ARCHIVE) == DIR_ATT_ARCHIVE)
      SERIAL_ECHO("DIR_ATT_ARCHIVE ");
    if (DIR_IS_LONG_NAME(&p))
      SERIAL_ECHO("DIR_ATT_LONG_NAME ");

    SERIAL_ECHO("Size: ");
    SERIAL_ECHOLN(p.fileSize);
  }
  SERIAL_ECHOLNPGM(STR_END_FILE_LIST);
}

static bool emit_text(
    const uint8_t* input,
    const int16_t input_length,
    const M6400Config &config)
{
  if (config.prefixed) {
    SERIAL_ECHOPGM(STR_B64_DATA);
    SERIAL_CHAR(' ');
  }

  if (config.mode == M6400Config::TEXT) {
    for (int16_t i = 0; i < input_length; ++i) {
      SERIAL_CHAR(input[i]);
      if (input[i] == '\n') {
        if (config.prefixed) {
          SERIAL_ECHOPGM(STR_B64_DATA);
          SERIAL_CHAR(' ');
        }
      }
    }

    SERIAL_EOL();
    return true;
  }

  char output[65] = {0};
  const int16_t output_length = kae_base64_encode(
      input,
      input_length,
      output,
      sizeof(output) - 1);

  if (output_length < 0) {
    SERIAL_ERROR_MSG(STR_B64_ERR_OVERRUN);
    return false;
  }

  output[output_length] = '\0';
  SERIAL_ECHOLN(output);
  return true;
}

static bool parse_M6400_args(GCodeParser &parser, M6400Config &config) {
  char *p = parser.string_arg;

  if (!p)
    return false;

  bool seen_a = false;
  bool seen_r = false;
  bool have_filename = false;

  while (*p) {
    while (*p && isspace((unsigned char)*p))
      ++p;

    if (!*p)
      break;

    char *token = p;

    while (*p && !isspace((unsigned char)*p))
      ++p;

    if (*p)
      *p++ = '\0';

    if (have_filename)
      return false;

    if (token[1] == '\0') {
      switch (token[0]) {
        case 'A':
          if (seen_a) return false;
          seen_a = true;
          config.mode = M6400Config::TEXT;
          continue;

        case 'R':
          if (seen_r) return false;
          seen_r = true;
          config.prefixed = false;
          continue;
      }
    }

    snprintf(config.filename, MAX_FILE_LENGTH, "%s", token);
    have_filename = true;
  }

  return have_filename;
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

//
// Get a DOS 8.3 filename in its useful form, e.g., "MYFILE  EXT" => "MYFILE.EXT"
//
static char *createFilename(char * const buffer, const dir_t &p) {
  char *pos = buffer;
  for (uint8_t i = 0; i < 11; ++i) {
    if (p.name[i] == ' ') continue;
    if (i == 8) *pos++ = '.';
    *pos++ = p.name[i];
  }
  *pos++ = '\0';
  return buffer;
}


#endif
