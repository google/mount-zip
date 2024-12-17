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

void checkConversion(const std::string_view fname,
                     bool needPrefix,
                     const std::string_view expected) {
  const std::string res = Path(fname).Normalize(needPrefix);
  assert(res == expected);
}

int main() {
  // converter
  checkConversion("normal.name", false, "/normal.name");
  checkConversion("normal.name", true, "/CUR/normal.name");
  checkConversion("path/to/normal.name", false, "/path/to/normal.name");
  checkConversion("path/to/normal.name", true, "/CUR/path/to/normal.name");

  checkConversion("", false, "/?");
  checkConversion("", true, "/?");

  checkConversion("./", false, "/");
  checkConversion("./", true, "/CUR");
  checkConversion(".///", false, "/");
  checkConversion(".///", true, "/CUR");
  checkConversion("././/.///", false, "/");
  checkConversion("././/.///", true, "/CUR");
  checkConversion("././/.///a/b/c", false, "/a/b/c");
  checkConversion("././/.///a/b/c", true, "/CUR/a/b/c");
  checkConversion("././/.///a/b/c", false, "/a/b/c");
  checkConversion("././/.///a/b/c", true, "/CUR/a/b/c");

  checkConversion("a/./c", false, "/a/?/c");
  checkConversion("a/./c", true, "/CUR/a/?/c");
  checkConversion("a/../c", false, "/a/?/c");
  checkConversion("a/../c", true, "/CUR/a/?/c");
  checkConversion("a/.", false, "/a/?");
  checkConversion("a/.", true, "/CUR/a/?");
  checkConversion("a/..", false, "/a/?");
  checkConversion("a/..", true, "/CUR/a/?");

  checkConversion(".", false, "/?");
  checkConversion(".", true, "/CUR/?");
  checkConversion("..", false, "/?");
  checkConversion("..", true, "/CUR/?");

  checkConversion("/.", false, "/?");
  checkConversion("/.", true, "/?");
  checkConversion("/..", false, "/?");
  checkConversion("/..", true, "/?");
  checkConversion("/./a", false, "/?/a");
  checkConversion("/./a", true, "/?/a");
  checkConversion("/../a", false, "/?/a");
  checkConversion("/../a", true, "/?/a");

  checkConversion(".hidden", false, "/.hidden");
  checkConversion("path/to/.hidden", false, "/path/to/.hidden");
  checkConversion("path/to/.hidden/dir", false, "/path/to/.hidden/dir");

  checkConversion("../", true, "/UP");
  checkConversion("../../../", true, "/UPUPUP");

  checkConversion("../abc", true, "/UP/abc");
  checkConversion("../../../abc", true, "/UPUPUP/abc");
  checkConversion("..///..//..//abc", true, "/UPUPUP/abc");

  checkConversion("/", true, "/");
  checkConversion("///", true, "/");
  checkConversion("/rootname", true, "/rootname");
  checkConversion("///rootname", true, "/rootname");
  checkConversion("/path/name", true, "/path/name");
  checkConversion("///path///name", true, "/path/name");

  return EXIT_SUCCESS;
}
