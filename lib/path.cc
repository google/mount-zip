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

#include "path.h"

#include <algorithm>
#include <cassert>
#include <iomanip>
#include <unordered_set>

#include <limits.h>

#include "log.h"

bool Path::redact = false;

std::ostream& operator<<(std::ostream& out, const Path path) {
  if (Path::redact)
    return out << "(redacted)";

  out.put('\'');
  for (const char c : path) {
    switch (c) {
      case '\\':
      case '\'':
        out.put('\\');
        out.put(c);
        break;
      default:
        const int i = static_cast<unsigned char>(c);
        if (std::iscntrl(i)) {
          out << "\\x" << std::hex << std::setw(2) << std::setfill('0') << i
              << std::dec;
        } else {
          out.put(c);
        }
    }
  }

  out.put('\'');
  return out;
}

Path Path::WithoutTrailingSeparator() const {
  Path path = *this;

  // Don't remove the first character, even if it is a '/'.
  while (path.size() > 1 && path.back() == '/')
    path.remove_suffix(1);

  return path;
}

Path::size_type Path::FinalExtensionPosition() const {
  const size_type last_dot = find_last_of("/. ");
  if (last_dot == npos || at(last_dot) != '.' || last_dot == 0 ||
      last_dot == size() - 1 || size() - last_dot > 6)
    return size();

  if (const size_type i = find_last_not_of('.', last_dot - 1);
      i == npos || at(i) == '/')
    return size();

  return last_dot;
}

Path::size_type Path::ExtensionPosition() const {
  const size_type last_dot = FinalExtensionPosition();
  if (last_dot >= size())
    return last_dot;

  // Extract extension without dot and in ASCII lowercase.
  assert(at(last_dot) == '.');
  std::string ext(substr(last_dot + 1));
  for (char& c : ext) {
    if ('A' <= c && c <= 'Z')
      c += 'a' - 'A';
  }

  // Is it a special extension?
  static const std::unordered_set<std::string_view> special_exts = {
      "z", "gz", "bz", "bz2", "xz", "zst", "lz", "lzma"};
  if (special_exts.count(ext)) {
    return Path(substr(0, last_dot)).FinalExtensionPosition();
  }

  return last_dot;
}

Path::size_type Path::TruncationPosition(size_type i) const {
  if (i >= size())
    return size();

  while (true) {
    // Avoid truncating at a UTF-8 trailing byte.
    while (i > 0 && (at(i) & 0b1100'0000) == 0b1000'0000)
      --i;

    if (i == 0)
      return i;

    const std::string_view zero_width_joiner = "\u200D";

    // Avoid truncating at a zero-width joiner.
    if (substr(i).starts_with(zero_width_joiner)) {
      --i;
      continue;
    }

    // Avoid truncating just after a zero-width joiner.
    if (substr(0, i).ends_with(zero_width_joiner)) {
      i -= zero_width_joiner.size();
      if (i > 0) {
        --i;
        continue;
      }
    }

    return i;
  }
}

void Path::Append(std::string* const head, const std::string_view tail) {
  assert(head);

  if (tail.empty())
    return;

  if (head->empty() || tail.starts_with('/')) {
    *head = tail;
    return;
  }

  assert(!head->empty());
  assert(!tail.empty());

  if (!head->ends_with('/'))
    *head += '/';

  *head += tail;
}

std::string Path::Normalize(const bool need_prefix) const {
  std::string_view in = *this;

  if (in.empty()) {
    return "/?";
  }

  std::string result = "/";

  // Add prefix
  if (in.starts_with('/')) {
    Append(&result, "ROOT");
    in.remove_prefix(1);
  } else {
    bool parentRelative = false;

    while (in.starts_with("./")) {
      in.remove_prefix(2);
      while (in.starts_with('/')) {
        in.remove_prefix(1);
      }
    }

    while (in.starts_with("../")) {
      result += "UP";
      in.remove_prefix(3);
      parentRelative = true;
      while (in.starts_with('/')) {
        in.remove_prefix(1);
      }
    }

    if (need_prefix && !parentRelative)
      Append(&result, "CUR");
  }

  // Extract part after part
  while (true) {
    size_type i = in.find_first_not_of('/');
    if (i == npos)
      return result;

    in.remove_prefix(i);
    assert(!in.empty());

    i = in.find_first_of('/');
    std::string_view part = in.substr(0, i);
    assert(!part.empty());
    in.remove_prefix(part.size());

    std::string_view extension;
    if (i == npos) {
      const size_type last_dot = Path(part).ExtensionPosition();
      extension = part.substr(last_dot);
      part.remove_suffix(extension.size());
    }

    part = part.substr(
        0, Path(part).TruncationPosition(NAME_MAX - extension.size()));

    if (part.empty() || part == "." || part == "..")
      part = "?";

    if (extension.empty()) {
      Append(&result, part);
    } else {
      Append(&result, StrCat(part, extension));
    }
  }
}
