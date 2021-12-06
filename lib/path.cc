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

#include <cassert>

#include <limits.h>

#include "log.h"
#include "path.h"

bool Path::redact = false;

Path Path::WithoutTrailingSeparator() const {
  Path path = *this;

  // Don't remove the first character, even if it is a '/'.
  while (path.size() > 1 && path.back() == '/')
    path.remove_suffix(1);

  return path;
}

Path Path::WithoutExtension() const {
  const std::string_view::size_type i = find_last_of("/.");
  if (i > 0 && i + 1 < size() && at(i) == '.' && at(i - 1) != '/')
    return substr(0, i);

  return *this;
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

bool Path::Normalize(std::string* dest_path,
                     std::string_view original_path,
                     bool need_prefix) {
  assert(!original_path.empty());
  assert(dest_path);

  *dest_path = "/";
  std::string_view in = original_path;

  // Add prefix
  if (in.starts_with('/')) {
    assert(need_prefix);
    Append(dest_path, "ROOT");
    in.remove_prefix(1);
  } else {
    bool parentRelative = false;
    while (in.starts_with("../")) {
      assert(need_prefix);
      *dest_path += "UP";
      in.remove_prefix(3);
      parentRelative = true;
    }

    if (need_prefix && !parentRelative)
      Append(dest_path, "CUR");
  }

  // Extract part after part
  while (true) {
    const std::string_view::size_type i = in.find_first_not_of('/');
    if (i == std::string_view::npos)
      return true;

    in.remove_prefix(i);
    assert(!in.empty());

    const std::string_view part = in.substr(0, in.find_first_of('/'));
    assert(!part.empty());

    if (part == "." || part == ".." || part.size() > NAME_MAX ||
        std::any_of(part.begin(), part.end(),
                    [](char c) { return std::iscntrl(c); })) {
      Log(LOG_ERR, "Bad file name: ", Path(original_path));
      return false;
    }

    Append(dest_path, part);
    in.remove_prefix(part.size());
  }
}
