//===----------------------------------------------------------------------===//
//
//                         CMU-DB Project (15-445/645)
//                         ***DO NO SHARE PUBLICLY***
//
// Identification: src/include/page/b_plus_tree_leaf_page.h
//
// Copyright (c) 2018-2024, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#pragma once

#include <string>
#include <utility>
#include <vector>

#include "storage/page/b_plus_tree_page.h"

namespace bustub {

#define B_PLUS_TREE_LEAF_PAGE_TYPE BPlusTreeLeafPage<KeyType, ValueType, KeyComparator>
#define LEAF_PAGE_HEADER_SIZE 16
#define LEAF_PAGE_SLOT_CNT ((BUSTUB_PAGE_SIZE - LEAF_PAGE_HEADER_SIZE) / (sizeof(KeyType) + sizeof(ValueType)))

/**
 * Store indexed key and record id (record id = page id combined with slot id,
 * see `include/common/rid.h` for detailed implementation) together within leaf
 * page. Only support unique key.
 *
 * Leaf page format (keys are stored in order):
 *  ---------
 * | HEADER |
 *  ---------
 *  ---------------------------------
 * | KEY(1) | KEY(2) | ... | KEY(n) |
 *  ---------------------------------
 *  ---------------------------------
 * | RID(1) | RID(2) | ... | RID(n) |
 *  ---------------------------------
 *
 *  Header format (size in byte, 16 bytes in total):
 *  -----------------------------------------------
 * | PageType (4) | CurrentSize (4) | MaxSize (4) |
 *  -----------------------------------------------
 *  -----------------
 * | NextPageId (4) |
 *  -----------------
 */
INDEX_TEMPLATE_ARGUMENTS
class BPlusTreeLeafPage : public BPlusTreePage {
 public:
  BPlusTreeLeafPage() = delete;
  BPlusTreeLeafPage(const BPlusTreeLeafPage &other) = delete;

  void Init(int max_size = LEAF_PAGE_SLOT_CNT);

  auto GetNextPageId() const -> page_id_t;
  void SetNextPageId(page_id_t next_page_id);

  auto KeyAt(int index) const -> KeyType;
  auto ValueAt(int index) const -> ValueType;
  auto GetItem(int index) const -> MappingType;

  auto KeyIndex(const KeyType &key, const KeyComparator &comparator) const -> int;
  auto Lookup(const KeyType &key, ValueType *value, const KeyComparator &comparator) const -> bool;

  auto Insert(const KeyType &key, const ValueType &value, const KeyComparator &comparator) -> int;
  auto RemoveAndDeleteRecord(const KeyType &key, const KeyComparator &comparator) -> int;
  void RemoveAt(int index);

  void MoveHalfTo(BPlusTreeLeafPage *recipient);
  void MoveAllTo(BPlusTreeLeafPage *recipient);
  void MoveFirstToEndOf(BPlusTreeLeafPage *recipient);
  void MoveLastToFrontOf(BPlusTreeLeafPage *recipient);

  void CopyNFrom(const KeyType *keys, const ValueType *values, int size);
  void CopyLastFrom(const KeyType &key, const ValueType &value);
  void CopyFirstFrom(const KeyType &key, const ValueType &value);

  auto ToString() const -> std::string {
    std::string kstr = "(";
    bool first = true;

    for (int i = 0; i < GetSize(); i++) {
      KeyType key = KeyAt(i);
      if (first) {
        first = false;
      } else {
        kstr.append(",");
      }
      kstr.append(std::to_string(key.ToString()));
    }
    kstr.append(")");
    return kstr;
  }

 private:
  page_id_t next_page_id_;
  KeyType key_array_[LEAF_PAGE_SLOT_CNT];
  ValueType rid_array_[LEAF_PAGE_SLOT_CNT];
};

}  // namespace bustub
