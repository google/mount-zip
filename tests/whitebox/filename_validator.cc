// Copyright 2021 Google LLC
// Copyright 2008-2019 Alexander Galanin <al@galanin.nnov.ru>
// http://galanin.nnov.ru/~al
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include "../config.h"

#include <cassert>
#include <string>

#include "path.h"

void checkConvertException(const char* fname, bool needPrefix) {
  std::string res;
  bool ok = Path::Normalize(&res, fname, needPrefix);
  assert(!ok);
}

void checkConversion(const char* fname, bool needPrefix, const char* expected) {
  std::string res;
  bool ok = Path::Normalize(&res, fname, needPrefix);
  assert(ok);
  assert(res == expected);
}

int main() {
  // converter
  checkConversion("normal.name", false, "/normal.name");
  checkConversion("normal.name", true, "/CUR/normal.name");
  checkConversion("path/to/normal.name", false, "/path/to/normal.name");
  checkConversion("path/to/normal.name", true, "/CUR/path/to/normal.name");

  checkConvertException(".", false);
  checkConvertException("./", false);
  checkConvertException("abc/./cde", false);
  checkConvertException("abc/.", false);

  checkConversion(".hidden", false, "/.hidden");
  checkConversion("path/to/.hidden", false, "/path/to/.hidden");
  checkConversion("path/to/.hidden/dir", false, "/path/to/.hidden/dir");

  checkConvertException(".", true);
  checkConvertException(".", true);
  checkConvertException("/.", true);
  checkConvertException("./", false);
  checkConvertException("./", false);
  checkConvertException("..", true);

  checkConversion("../", true, "/UP");
  checkConversion("../../../", true, "/UPUPUP");

  checkConvertException("/..", true);
  checkConvertException("/../blah", true);

  checkConversion("../abc", true, "/UP/abc");
  checkConversion("../../../abc", true, "/UPUPUP/abc");

  checkConvertException("abc/../cde", false);
  checkConvertException("abc/../cde", true);
  checkConvertException("abc/..", false);
  checkConvertException("abc/..", true);
  checkConvertException("../abc/..", true);

  checkConversion("/", true, "/ROOT");
  checkConversion("/rootname", true, "/ROOT/rootname");
  checkConversion("/path/name", true, "/ROOT/path/name");

  return EXIT_SUCCESS;
}
