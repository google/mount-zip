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

#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

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

    // Include hard links?
    bool include_hard_links = true;

    // Include special file types (block and character devices, FIFOs and
    // sockets)?
    bool include_special_files = true;

    // Checks the password validity on the first encrypted file found in the
    // ZIP?
    bool check_password = true;

    // Check if all the files use supported compression and encryption methods?
    bool check_compression = true;

    // Pre-cache data?
    bool pre_cache = false;

    // Merge multiple ZIPs at the root level?
    bool merge = true;

    // Trim top level if possible?
    bool trim = true;
  };

  // Opens ZIP archives and constructs the internal tree structure.
  // Throws an std::runtime_error in case of error.
  Tree(std::span<const std::string> paths, Options opts);

  // For tests.
  Tree(const std::string& path) : Tree(std::span(&path, 1), Options{}) {}

  // Finds an existing node with the given |path|.
  // Returns a null pointer if no matching node can be found.
  FileNode* Find(std::string_view path);

  static const blksize_t block_size = DataNode::block_size;
  blkcnt_t GetBlockCount() const { return total_block_count_; }
  fsfilcnt_t GetNodeCount() const { return files_by_path_.size(); }

 private:
  struct CloseZip {
    void operator()(zip_t* z) const { zip_close(z); }
  };

  using Zip = std::unique_ptr<zip_t, CloseZip>;
  using Zips = std::vector<std::pair<Zip, std::string>>;

  // Opens the given ZIP archives.
  static Zips OpenZips(std::span<const std::string> paths);

  // Constructor.
  Tree(Zips zips, Options opts)
      : zips_(std::move(zips)), opts_(std::move(opts)) {}

  // Returned by GetEntryAttributes.
  struct EntryAttributes {
    mode_t mode;        // Unix mode
    bool is_hard_link;  // PkWare hard link flag
  };

  // Gets the UNIX mode and the PkWare hard link flag from the entry external
  // attributes field.
  EntryAttributes GetEntryAttributes(zip_t* z,
                                     i64 id,
                                     std::string_view original_path);

  // Finds an existing dir node with the given |path|, or create one (and all
  // the needed intermediary nodes).
  FileNode* CreateDir(std::string_view path);

  // String conversion function.
  using ToUtf8 = std::function<std::string_view(std::string_view)>;

  // Creates and attaches a node for an existing file or dir entry.
  FileNode* CreateFile(zip_t* z,
                       i64 id,
                       FileNode* parent,
                       std::string_view name,
                       mode_t mode);

  // Creates and attaches a hard link node.
  FileNode* CreateHardLink(zip_t* z,
                           i64 id,
                           FileNode* parent,
                           std::string_view name,
                           mode_t mode,
                           std::string target_path,
                           const ToUtf8& toUtf8);

  // Attaches the given |node|, renaming it if necessary to prevent name
  // collisions.
  FileNode* Attach(FileNode::Ptr node);

  // Reads the password from the standard input.
  // Returns true if a non-empty password was read.
  bool ReadPasswordFromStdIn();

  // If necessary, ask for a password and check it on the given entry.
  // Throws ZipError if the password doesn't match.
  void CheckPassword(const FileNode* node);

  void Trim(FileNode& dest);
  void Deindex(FileNode& node);
  void Reindex(FileNode& node);

  // Computes the optimal number of buckets for the hash tables indexing the
  // given ZIP archives.
  static size_t GetBucketCount(const Zips& zips);

  // ZIP archives.
  const Zips zips_;

  // Extraction options.
  const Options opts_;

  // Path extractor for FileNode.
  struct GetPath {
    using type = std::string;
    std::string operator()(const FileNode& node) const {
      return node.GetPath();
    }
  };

  using OriginalPath = std::pair<const zip_t*, std::string_view>;

  // Original path extractor for FileNode.
  struct GetOriginalPath {
    using type = OriginalPath;
    OriginalPath operator()(const FileNode& node) const {
      return {node.zip, node.original_path};
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
      bi::key_of_value<GetOriginalPath>>;

  const size_t bucket_count_ = GetBucketCount(zips_);

  using BucketByPath = FilesByPath::bucket_type;
  const std::unique_ptr<BucketByPath[]> buckets_by_path_{
      new BucketByPath[bucket_count_]};

  using BucketByOriginalPath = FilesByOriginalPath::bucket_type;
  const std::unique_ptr<BucketByOriginalPath[]> buckets_by_original_path_{
      new BucketByOriginalPath[bucket_count_]};

  // Collection of all FileNodes indexed by full path.
  // Owns the nodes it references.
  FilesByPath files_by_path_{{buckets_by_path_.get(), bucket_count_}};

  // Collection of FileNodes indexed by original path.
  FilesByOriginalPath files_by_original_path_{
      {buckets_by_original_path_.get(), bucket_count_}};

  // Root node.
  FileNode* const root_ =
      new FileNode{.data = {.nlink = 2, .mode = S_IFDIR | 0755}, .name = "/"};

  blkcnt_t total_block_count_ = 1;

  // Has the password been verified?
  bool checked_password_ = false;
};

#endif  // TREE_H
