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
#include <iomanip>
#include <iostream>
#include <string>

#include "path.h"

void checkConversion(const std::string_view fname,
                     const std::string_view expected) {
  const std::string res = Path(fname).Normalized();
  if (res != expected) {
    std::cerr << "got " << std::quoted(res) << ", want "
              << std::quoted(expected) << std::endl;
  }
  assert(res == expected);
}

int main() {
  checkConversion("normal.name", "/normal.name");
  checkConversion("path/to/normal.name", "/path/to/normal.name");
  checkConversion("", "/?");
  checkConversion("./", "/");
  checkConversion(".///", "/");
  checkConversion("./..//.///", "/");
  checkConversion("./..//.///a/b/c", "/a/b/c");
  checkConversion("a/./c", "/a/?/c");
  checkConversion("a/../c", "/a/?/c");
  checkConversion("a/.", "/a/?");
  checkConversion("a/..", "/a/?");
  checkConversion(".", "/?");
  checkConversion("..", "/?");
  checkConversion("/.", "/?");
  checkConversion("/..", "/?");
  checkConversion("/./a", "/?/a");
  checkConversion("/../a", "/?/a");
  checkConversion(".hidden", "/.hidden");
  checkConversion("path/to/.hidden", "/path/to/.hidden");
  checkConversion("path/to/.hidden/dir", "/path/to/.hidden/dir");
  checkConversion("../", "/");
  checkConversion("../../../", "/");
  checkConversion("../abc", "/abc");
  checkConversion("../../../abc", "/abc");
  checkConversion("..///..//..//abc", "/abc");
  checkConversion("/", "/");
  checkConversion("///", "/");
  checkConversion("/rootname", "/rootname");
  checkConversion("///rootname", "/rootname");
  checkConversion("/path/name", "/path/name");
  checkConversion("///path///name", "/path/name");

  return EXIT_SUCCESS;
}
