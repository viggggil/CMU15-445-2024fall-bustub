/**
 * b_plus_tree.h
 *
 * Implementation of simple b+ tree data structure where internal pages direct
 * the search and leaf pages contain actual data.
 * (1) We only support unique key
 * (2) support insert & remove
 * (3) The structure should shrink and grow dynamically
 * (4) Implement index iterator for range scan
 */
#pragma once

#include <algorithm>
#include <deque>
#include <filesystem>
#include <iostream>
#include <optional>
#include <queue>
#include <shared_mutex>
#include <string>
#include <utility>
#include <vector>

#include "common/config.h"
#include "common/macros.h"
#include "storage/index/index_iterator.h"
#include "storage/page/b_plus_tree_header_page.h"
#include "storage/page/b_plus_tree_internal_page.h"
#include "storage/page/b_plus_tree_leaf_page.h"
#include "storage/page/page_guard.h"

namespace bustub {

struct PrintableBPlusTree;

/**
 * @brief Definition of the Context class.
 *
 * Hint: This class is designed to help you keep track of the pages
 * that you're modifying or accessing.
 */
class Context {
 public:
  std::optional<WritePageGuard> header_page_{std::nullopt};
  page_id_t root_page_id_{INVALID_PAGE_ID};
  std::deque<WritePageGuard> write_set_;
  std::deque<ReadPageGuard> read_set_;

  auto IsRootPage(page_id_t page_id) -> bool { return page_id == root_page_id_; }
};

#define BPLUSTREE_TYPE BPlusTree<KeyType, ValueType, KeyComparator>

// Main class providing the API for the Interactive B+ Tree.
INDEX_TEMPLATE_ARGUMENTS
class BPlusTree {
  using InternalPage = BPlusTreeInternalPage<KeyType, page_id_t, KeyComparator>;
  using LeafPage = BPlusTreeLeafPage<KeyType, ValueType, KeyComparator>;

 public:
  explicit BPlusTree(std::string name, page_id_t header_page_id, BufferPoolManager *buffer_pool_manager,
                     const KeyComparator &comparator, int leaf_max_size = LEAF_PAGE_SLOT_CNT,
                     int internal_max_size = INTERNAL_PAGE_SLOT_CNT);

  auto IsEmpty() const -> bool;
  auto Insert(const KeyType &key, const ValueType &value) -> bool;
  void Remove(const KeyType &key);
  auto GetValue(const KeyType &key, std::vector<ValueType> *result) -> bool;
  auto GetRootPageId() -> page_id_t;

  auto Begin() -> INDEXITERATOR_TYPE;
  auto End() -> INDEXITERATOR_TYPE;
  auto Begin(const KeyType &key) -> INDEXITERATOR_TYPE;
  void CoalesceInternal(WritePageGuard *left_guard, WritePageGuard *right_guard, WritePageGuard *parent_guard,
                      int parent_sep_index, bool right_is_target);

  void RedistributeInternal(WritePageGuard *left_guard, WritePageGuard *right_guard, WritePageGuard *parent_guard,
                          int parent_sep_index, bool right_is_target);
  void UpdateParentKeyAfterLeafChange(Context *ctx);

  void Print(BufferPoolManager *bpm);
  void Draw(BufferPoolManager *bpm, const std::filesystem::path &outf);
  auto DrawBPlusTree() -> std::string;
  void InsertFromFile(const std::filesystem::path &file_name);
  void RemoveFromFile(const std::filesystem::path &file_name);
  void BatchOpsFromFile(const std::filesystem::path &file_name);

 private:
  /*****************************************************************************
   * Task 2 Helpers
   *****************************************************************************/

  auto ReadRootPageId() const -> page_id_t;
  void UpdateRootPageId(page_id_t root_page_id);

  auto FindLeafRead(const KeyType &key, bool leftmost = false) -> ReadPageGuard;
  auto FindLeafWrite(const KeyType &key, Context *ctx, bool leftmost = false) -> WritePageGuard;

  void StartNewTree(const KeyType &key, const ValueType &value, Context *ctx);

  auto InsertIntoLeaf(LeafPage *leaf, const KeyType &key, const ValueType &value) -> bool;

  auto SplitLeaf(LeafPage *leaf) -> std::pair<page_id_t, KeyType>;
  auto SplitInternal(InternalPage *internal) -> std::pair<page_id_t, KeyType>;

  void CreateNewRoot(page_id_t old_node_pid, const KeyType &middle_key, page_id_t new_node_pid, Context *ctx);

  void InsertIntoParent(page_id_t old_node_pid, const KeyType &middle_key, page_id_t new_node_pid, Context *ctx);

  auto GetMinSize(BPlusTreePage *page) const -> int;

  void AdjustRoot(BPlusTreePage *old_root, Context *ctx);
  void SetRootPageId(page_id_t root_page_id, Context *ctx);
  void CoalesceOrRedistribute(WritePageGuard *node_guard, Context *ctx);
  void CoalesceLeaf(WritePageGuard *left_guard, WritePageGuard *right_guard, WritePageGuard *parent_guard,
                    int parent_sep_index, bool right_is_target);
  void RedistributeLeaf(WritePageGuard *left_guard, WritePageGuard *right_guard, WritePageGuard *parent_guard,
                        int parent_sep_index, bool right_is_target);

  /*****************************************************************************
   * Debug Routines
   *****************************************************************************/

  void ToGraph(page_id_t page_id, const BPlusTreePage *page, std::ofstream &out);
  void PrintTree(page_id_t page_id, const BPlusTreePage *page);
  auto ToPrintableBPlusTree(page_id_t root_id) -> PrintableBPlusTree;

  // member variable
  std::string index_name_;
  BufferPoolManager *bpm_;
  KeyComparator comparator_;
  std::vector<std::string> log;  // NOLINT
  int leaf_max_size_;
  int internal_max_size_;
  page_id_t header_page_id_;
};

/**
 * @brief for test only. PrintableBPlusTree is a printable B+ tree.
 * We first convert B+ tree into a printable B+ tree and then print it.
 */
struct PrintableBPlusTree {
  int size_;
  std::string keys_;
  std::vector<PrintableBPlusTree> children_;

  void Print(std::ostream &out_buf) {
    std::vector<PrintableBPlusTree *> que = {this};
    while (!que.empty()) {
      std::vector<PrintableBPlusTree *> new_que;

      for (auto &t : que) {
        int padding = (t->size_ - static_cast<int>(t->keys_.size())) / 2;
        out_buf << std::string(padding, ' ');
        out_buf << t->keys_;
        out_buf << std::string(padding, ' ');

        for (auto &c : t->children_) {
          new_que.push_back(&c);
        }
      }
      out_buf << "\n";
      que = std::move(new_que);
    }
  }
};

}  // namespace bustub
