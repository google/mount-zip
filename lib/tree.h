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

#ifndef TREE_H
#define TREE_H

#include <string>
#include <string_view>

#include "file_node.h"

// Holds the ZIP filesystem tree.
class Tree {
 public:
  ~Tree();

  // Extraction options.
  struct Options {
    // Filename encoding in ZIP.
    // Null, empty or "auto" for automatic detection.
    const char* encoding = nullptr;

    // Include symbolic links?
    bool include_symlinks = true;

    // Include hardlinks?
    bool include_hardlinks = true;

    // Include special file types (block and character devices, FIFOs and
    // sockets)?
    bool include_special_files = true;

    // Checks the password validity on the first encrypted file found in the
    // ZIP?
    bool check_password = true;

    // Check if all the files use supported compression and encryption methods?
    bool check_compression = true;
  };

  using Ptr = std::unique_ptr<Tree>;

  // Opens ZIP file and constructs the internal tree structure.
  // Throws an std::runtime_error in case of error.
  static Ptr Init(const char* filename, Options opts);
  static Ptr Init(const char* filename) { return Init(filename, Options{}); }

  // Finds an existing node with the given |path|.
  // Returns a null pointer if no matching node can be found.
  FileNode* Find(std::string_view path);

  static const blksize_t block_size = DataNode::block_size;
  blkcnt_t GetBlockCount() const { return total_block_count_; }

 private:
  // Constructor.
  Tree(zip_t* zip, Options opts) : zip_(zip), opts_(std::move(opts)) {}
  Tree(zip_t* zip) : zip_(zip) {}

  // Builds internal tree structure.
  void BuildTree();

  // Returned by GetEntryAttributes.
  struct EntryAttributes {
    mode_t mode;       // Unix mode
    bool is_hardlink;  // PkWare hardlink flag
  };

  // Gets the UNIX mode and the PkWare hardlink flag from the entry external
  // attributes field.
  EntryAttributes GetEntryAttributes(zip_uint64_t id,
                                     std::string_view original_path);

  // Finds an existing dir node with the given |path|, or create one (and all
  // the needed intermediary nodes).
  FileNode* CreateDir(std::string_view path);

  // Creates and attaches a node for an existing file or dir entry.
  FileNode* CreateFile(zip_int64_t id,
                       FileNode* parent,
                       std::string_view name,
                       mode_t mode);

  // Creates and attaches a hardlink node.
  FileNode* CreateHardlink(zip_int64_t id,
                           FileNode* parent,
                           std::string_view name,
                           mode_t mode);

  // Attaches the given |node|, renaming it if necessary to prevent name
  // collisions.
  FileNode* Attach(FileNode::Ptr node);

  // Reads the password from the standard input.
  // Returns true if a non-empty password was read.
  bool ReadPasswordFromStdIn();

  // If necessary, ask for a password and check it on the given entry.
  // Throws ZipError if the password doesn't match.
  void CheckPassword(const FileNode* node);

  // Computes the optimal number of buckets for the hash tables indexing the
  // given ZIP archive.
  static size_t GetBucketCount(zip_t* zip);

  // ZIP archive.
  zip_t* const zip_;

  // Extraction options.
  const Options opts_;

  // Path extractor for FileNode.
  struct GetPath {
    using type = std::string;
    std::string operator()(const FileNode& node) const { return node.path(); }
  };

  // Original path extractor for FileNode.
  struct GetOriginalPath {
    using type = std::string_view;
    std::string_view operator()(const FileNode& node) const {
      return node.original_path;
    }
  };

  using FilesByPath = bi::unordered_set<
      FileNode,
      bi::member_hook<FileNode, FileNode::ByPath, &FileNode::by_path>,
      bi::constant_time_size<true>,
      bi::power_2_buckets<true>,
      bi::compare_hash<true>,
      bi::key_of_value<GetPath>,
      bi::equal<std::equal_to<std::string_view>>,
      bi::hash<std::hash<std::string_view>>>;

  using FilesByOriginalPath = bi::unordered_set<
      FileNode,
      bi::member_hook<FileNode, FileNode::ByPath, &FileNode::by_original_path>,
      bi::constant_time_size<false>,
      bi::power_2_buckets<true>,
      bi::compare_hash<true>,
      bi::key_of_value<GetOriginalPath>,
      bi::equal<std::equal_to<std::string_view>>,
      bi::hash<std::hash<std::string_view>>>;

  const size_t bucket_count_ = GetBucketCount(zip_);

  using BucketByPath = FilesByPath::bucket_type;
  const std::unique_ptr<BucketByPath[]> buckets_by_path_{
      new BucketByPath[bucket_count_]};

  using BucketByOriginalPath = FilesByPath::bucket_type;
  const std::unique_ptr<BucketByOriginalPath[]> buckets_by_original_path_{
      new BucketByOriginalPath[bucket_count_]};

  // Collection of all FileNodes indexed by full path.
  // Owns the nodes it references.
  FilesByPath files_by_path_{{buckets_by_path_.get(), bucket_count_}};

  // Collection of FileNodes indexed by original path.
  FilesByOriginalPath files_by_original_path_{
      {buckets_by_original_path_.get(), bucket_count_}};

  blkcnt_t total_block_count_ = 1;

  // Does NormalizePath add a prefix?
  bool need_prefix_ = false;

  // Has the password been verified?
  bool checked_password_ = false;
};

#endif  // TREE_H
