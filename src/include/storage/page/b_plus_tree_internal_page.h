//===----------------------------------------------------------------------===//
//
//                         CMU-DB Project (15-445/645)
//                         ***DO NOT SHARE PUBLICLY***
//
// Identification: src/include/storage/page/b_plus_tree_internal_page.h
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

#define B_PLUS_TREE_INTERNAL_PAGE_TYPE BPlusTreeInternalPage<KeyType, ValueType, KeyComparator>
#define INTERNAL_PAGE_HEADER_SIZE 12
#define INTERNAL_PAGE_SLOT_CNT \
  ((BUSTUB_PAGE_SIZE - INTERNAL_PAGE_HEADER_SIZE) / (sizeof(KeyType) + sizeof(ValueType)))

/**
 * Store `n` indexed keys and `n` child pointers in internal page.
 *
 * IMPORTANT:
 * - key_array_[0] is always invalid.
 * - page_id_array_[0] is the leftmost child pointer.
 * - for i >= 1, key_array_[i] is the separator key for page_id_array_[i].
 *
 * So if current size is `n`, then:
 * - there are `n` child pointers stored in page_id_array_[0 ... n-1]
 * - there are `n-1` valid keys stored in key_array_[1 ... n-1]
 */
INDEX_TEMPLATE_ARGUMENTS
class BPlusTreeInternalPage : public BPlusTreePage {
 public:
  BPlusTreeInternalPage() = delete;
  BPlusTreeInternalPage(const BPlusTreeInternalPage &other) = delete;

  void Init(int max_size = INTERNAL_PAGE_SLOT_CNT);

  auto KeyAt(int index) const -> KeyType;
  void SetKeyAt(int index, const KeyType &key);

  auto ValueAt(int index) const -> ValueType;
  void SetValueAt(int index, const ValueType &value);

  auto ValueIndex(const ValueType &value) const -> int;
  auto GetItem(int index) const -> MappingType;

  auto Lookup(const KeyType &key, const KeyComparator &comparator) const -> ValueType;

  void PopulateNewRoot(const ValueType &old_value, const KeyType &new_key, const ValueType &new_value);

  auto InsertNodeAfter(const ValueType &old_value, const KeyType &new_key, const ValueType &new_value) -> int;

  auto Remove(int index) -> int;
  void RemoveAndReturnOnlyChild();

  // split helper for insertion
  void MoveHalfTo(BPlusTreeInternalPage *recipient, KeyType *middle_key);

  // helpers for deletion / redistribution
  void MoveAllTo(BPlusTreeInternalPage *recipient, const KeyType &middle_key);
  void MoveFirstToEndOf(BPlusTreeInternalPage *recipient, const KeyType &middle_key);
  void MoveLastToFrontOf(BPlusTreeInternalPage *recipient, const KeyType &middle_key);

  void CopyNFrom(const ValueType *values, const KeyType *keys, int size);
  void CopyLastFrom(const KeyType &key, const ValueType &value);
  void CopyFirstFrom(const KeyType &key, const ValueType &value);

  auto ToString() const -> std::string {
    std::string kstr = "(";
    bool first = true;
    for (int i = 1; i < GetSize(); i++) {
      if (first) {
        first = false;
      } else {
        kstr.append(",");
      }
      kstr.append(std::to_string(KeyAt(i).ToString()));
    }
    kstr.append(")");
    return kstr;
  }

 private:
  // key_array_[0] is invalid
  KeyType key_array_[INTERNAL_PAGE_SLOT_CNT];
  ValueType page_id_array_[INTERNAL_PAGE_SLOT_CNT];
};

}  // namespace bustub
