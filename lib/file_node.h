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

#ifndef FILE_NODE_H
#define FILE_NODE_H

#include <memory>
#include <ostream>
#include <string>

#include <unistd.h>
#include <zip.h>

#include <boost/intrusive/slist.hpp>
#include <boost/intrusive/unordered_set.hpp>

#include "data_node.h"
#include "path.h"

namespace bi = boost::intrusive;

// Represents a named file or directory entry in the filesystem tree.
struct FileNode {
  // FileNodes are dynamically allocated and passed around by unique_ptr when
  // the ownership is transferred.
  using Ptr = std::unique_ptr<FileNode>;

  // Reference to the ZIP archive.
  zip_t* const zip;

  // Index of the entry represented by this node in the ZIP archive, or -1 if it
  // is not directly represented in the ZIP archive (like the root directory, or
  // any intermediate directory).
  const zip_int64_t id = -1;

  // Inode data of this entry.
  DataNode data;

  // If this FileNode is a hardlink, this points to the target inode.
  const DataNode* link = &data;

  // Pointer to the parent node. Should be non null. The only exception is the
  // root directory which has a null parent pointer.
  FileNode* parent = nullptr;

  // Name of this node in the context of its parent. This name should be a valid
  // and non-empty filename, and it shouldn't contain any '/' separator. The
  // only exception is the root directory, which is just named "/".
  std::string name;

  // Original path as recorded in the ZIP archive. This is used to find hardlink
  // targets.
  std::string_view original_path;

  // Number of entries whose name have initially collided with this file node.
  int collision_count = 0;

#ifdef NDEBUG
  using LinkMode = bi::link_mode<bi::normal_link>;
#else
  using LinkMode = bi::link_mode<bi::safe_link>;
#endif

  // Hook used to index FileNodes by parent.
  using ByParent = bi::slist_member_hook<LinkMode>;
  ByParent by_parent;

  // Children of this node. The children are not sorted and their order is not
  // relevant. This collection doesn't own the children nodes. The |parent|
  // pointer of every child in |children| should point back to this node.
  using Children =
      bi::slist<FileNode,
                bi::member_hook<FileNode, ByParent, &FileNode::by_parent>,
                bi::constant_time_size<false>,
                bi::linear<true>,
                bi::cache_last<false>>;
  Children children;

  // Hooks used to index FileNodes by full path and by original path.
  using ByPath = bi::unordered_set_member_hook<LinkMode, bi::store_hash<true>>;
  ByPath by_path, by_original_path;

  // Gets attributes.
  using Stat = struct stat;
  operator Stat() const { return *link; }

  FileType type() const { return GetFileType(link->mode); }
  bool is_dir() const { return type() == FileType::Directory; }

  // Gets the full absolute path of this node.
  std::string path() const {
    if (!parent)
      return name;

    std::string s = parent->path();
    Path::Append(&s, name);
    return s;
  }

  // Adds a child to this node.
  void AddChild(FileNode* const child) {
    assert(child);
    assert(this == child->parent);
    children.push_front(*child);
  }

  // Gets a Reader to read file contents.
  Reader::Ptr GetReader() const { return link->GetReader(zip, *this); }

  // Output operator for debugging.
  friend std::ostream& operator<<(std::ostream& out, const FileNode& node) {
    return out << node.type() << " [" << node.id << "] " << Path(node.path());
  }
};

#endif
