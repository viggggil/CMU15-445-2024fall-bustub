//===----------------------------------------------------------------------===//
//
//                         CMU-DB Project (15-445/645)
//                         ***DO NO SHARE PUBLICLY***
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
#define INTERNAL_PAGE_SLOT_CNT ((BUSTUB_PAGE_SIZE - INTERNAL_PAGE_HEADER_SIZE) / (sizeof(KeyType) + sizeof(ValueType)))

/**
 * Store `n` indexed keys and `n + 1` child pointers (page_id) within internal page.
 * Pointer PAGE_ID(i) points to a subtree in which all keys K satisfy:
 * K(i) <= K < K(i+1).
 * NOTE: Since the number of keys does not equal to number of child pointers,
 * the first key in key_array_ is always invalid.
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
  auto ValueIndex(const ValueType &value) const -> int;

  auto GetItem(int index) const -> MappingType;

  auto Lookup(const KeyType &key, const KeyComparator &comparator) const -> ValueType;
  auto InsertNodeAfter(const ValueType &old_value, const KeyType &new_key, const ValueType &new_value) -> int;
  void PopulateNewRoot(const ValueType &old_value, const KeyType &new_key, const ValueType &new_value);

  auto Remove(int index) -> int;
  void RemoveAndReturnOnlyChild();

  void MoveHalfTo(BPlusTreeInternalPage *recipient);
  void MoveAllTo(BPlusTreeInternalPage *recipient);
  void MoveFirstToEndOf(BPlusTreeInternalPage *recipient);
  void MoveLastToFrontOf(BPlusTreeInternalPage *recipient);

  void CopyNFrom(const KeyType *keys, const ValueType *values, int size);
  void CopyLastFrom(const KeyType &key, const ValueType &value);
  void CopyFirstFrom(const KeyType &key, const ValueType &value);

 private:
  // key_array_[0] is invalid
  KeyType key_array_[INTERNAL_PAGE_SLOT_CNT];
  ValueType page_id_array_[INTERNAL_PAGE_SLOT_CNT];
};

}  // namespace bustub
