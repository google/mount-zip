// Copyright 2021 Google LLC
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

#ifndef PATH_H
#define PATH_H

#include <ostream>
#include <sstream>
#include <string>
#include <string_view>

class Path : public std::string_view {
 public:
  Path(const char* path) : std::string_view(path) {}
  Path(std::string_view path) : std::string_view(path) {}

  // Removes trailing separators.
  Path WithoutTrailingSeparator() const;

  // Gets the position of the dot where the filename extension starts, or
  // `size()` if there is no extension.
  //
  // If the path ends with a slash, then it does not have an extension:
  // * "/" -> no extension
  // * "foo/" -> no extension
  // * "a.b/" -> no extension
  //
  // If the path ends with a dot, then it does not have an extension:
  // * "." -> no extension
  // * ".." -> no extension
  // * "..." -> no extension
  // * "foo." -> no extension
  // * "foo..." -> no extension
  //
  // If the filename starts with a dot or a sequence of dots, but does not have
  // any other dot after that, then it does not have an extension:
  // ".foo" -> no extension
  // "...foo" -> no extension
  // "a.b/...foo" -> no extension
  //
  // An extension cannot contain a space:
  // * "foo. " -> no extension
  // * "foo.a b" -> no extension
  // * "foo. (1)" -> no extension
  //
  // An extension cannot be longer than 6 bytes, including the leading dot:
  // * "foo.tool" -> ".tool"
  // * "foo.toolong" -> no extension
  size_type ExtensionPosition() const;

  // Removes the extension, if any.
  Path WithoutExtension() const { return substr(0, ExtensionPosition()); }

  // Splits path between parent path and basename.
  std::pair<Path, Path> Split() const {
    const std::string_view::size_type i = find_last_of('/') + 1;
    return {Path(substr(0, i)).WithoutTrailingSeparator(), substr(i)};
  }

  // Appends the |tail| path to |*head|. If |tail| is an absolute path, then
  // |*head| takes the value of |tail|. If |tail| is a relative path, then it is
  // appended to |*head|. A '/' separator is added if |*head| doesn't already
  // end with one.
  static void Append(std::string* head, std::string_view tail);

  // Normalizes path.
  static bool Normalize(std::string* dest_path,
                        std::string_view original_path,
                        bool need_prefix);

  // Should paths be redacted from logs?
  static bool redact;
};

// Output operator for debugging.
std::ostream& operator<<(std::ostream& out, Path path);

#endif  // PATH_H
