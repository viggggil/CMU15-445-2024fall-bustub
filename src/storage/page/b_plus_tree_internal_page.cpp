//===----------------------------------------------------------------------===//
//
//                         CMU-DB Project (15-445/645)
//                         ***DO NOT SHARE PUBLICLY***
//
// Identification: src/storage/page/b_plus_tree_internal_page.cpp
//
// Copyright (c) 2018-2024, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include "storage/page/b_plus_tree_internal_page.h"

namespace bustub {

INDEX_TEMPLATE_ARGUMENTS
void B_PLUS_TREE_INTERNAL_PAGE_TYPE::Init(int max_size) {
  SetPageType(IndexPageType::INTERNAL_PAGE);
  SetSize(0);
  SetMaxSize(max_size);
}

INDEX_TEMPLATE_ARGUMENTS
auto B_PLUS_TREE_INTERNAL_PAGE_TYPE::KeyAt(int index) const -> KeyType {
  return key_array_[index];
}

INDEX_TEMPLATE_ARGUMENTS
void B_PLUS_TREE_INTERNAL_PAGE_TYPE::SetKeyAt(int index, const KeyType &key) {
  key_array_[index] = key;
}

INDEX_TEMPLATE_ARGUMENTS
auto B_PLUS_TREE_INTERNAL_PAGE_TYPE::ValueAt(int index) const -> ValueType {
  return page_id_array_[index];
}

INDEX_TEMPLATE_ARGUMENTS
void B_PLUS_TREE_INTERNAL_PAGE_TYPE::SetValueAt(int index, const ValueType &value) {
  page_id_array_[index] = value;
}

INDEX_TEMPLATE_ARGUMENTS
auto B_PLUS_TREE_INTERNAL_PAGE_TYPE::ValueIndex(const ValueType &value) const -> int {
  for (int i = 0; i < GetSize(); i++) {
    if (page_id_array_[i] == value) {
      return i;
    }
  }
  return -1;
}

INDEX_TEMPLATE_ARGUMENTS
auto B_PLUS_TREE_INTERNAL_PAGE_TYPE::GetItem(int index) const -> MappingType {
  return {key_array_[index], page_id_array_[index]};
}

/**
 * Lookup child pointer for key.
 *
 * Since key_array_[0] is invalid, we binary search on [1, size-1],
 * and return the child pointer at the largest index i such that
 * key_array_[i] <= key. If no such i exists, return page_id_array_[0].
 */
INDEX_TEMPLATE_ARGUMENTS
auto B_PLUS_TREE_INTERNAL_PAGE_TYPE::Lookup(const KeyType &key, const KeyComparator &comparator) const -> ValueType {
  int left = 1;
  int right = GetSize() - 1;
  int ans = 0;

  while (left <= right) {
    int mid = left + (right - left) / 2;
    if (comparator(key_array_[mid], key) <= 0) {
      ans = mid;
      left = mid + 1;
    } else {
      right = mid - 1;
    }
  }

  return page_id_array_[ans];
}

/**
 * Populate a brand new root after splitting old root.
 *
 * New layout:
 *   page_id_array_[0] = old_value
 *   key_array_[1]     = new_key
 *   page_id_array_[1] = new_value
 * size = 2
 */
INDEX_TEMPLATE_ARGUMENTS
void B_PLUS_TREE_INTERNAL_PAGE_TYPE::PopulateNewRoot(const ValueType &old_value, const KeyType &new_key,
                                                     const ValueType &new_value) {
  key_array_[0] = KeyType{};
  page_id_array_[0] = old_value;
  key_array_[1] = new_key;
  page_id_array_[1] = new_value;
  SetSize(2);
}

/**
 * Insert (new_key, new_value) right after old_value.
 */
INDEX_TEMPLATE_ARGUMENTS
auto B_PLUS_TREE_INTERNAL_PAGE_TYPE::InsertNodeAfter(const ValueType &old_value, const KeyType &new_key,
                                                     const ValueType &new_value) -> int {
  int index = ValueIndex(old_value);
  int insert_pos = index + 1;

  for (int i = GetSize(); i > insert_pos; --i) {
    key_array_[i] = key_array_[i - 1];
    page_id_array_[i] = page_id_array_[i - 1];
  }

  key_array_[insert_pos] = new_key;
  page_id_array_[insert_pos] = new_value;
  ChangeSizeBy(1);
  return GetSize();
}

/**
 * Remove slot at index.
 *
 * Note:
 * - index should refer to the slot being removed.
 * - for parent deletion in leaf coalesce cases, this usually removes
 *   the separator key together with the child pointer slot.
 */
INDEX_TEMPLATE_ARGUMENTS
auto B_PLUS_TREE_INTERNAL_PAGE_TYPE::Remove(int index) -> int {
  for (int i = index; i < GetSize() - 1; ++i) {
    key_array_[i] = key_array_[i + 1];
    page_id_array_[i] = page_id_array_[i + 1];
  }
  ChangeSizeBy(-1);
  return GetSize();
}

/**
 * Used when internal root has only one child left.
 * Keep only page_id_array_[0].
 */
INDEX_TEMPLATE_ARGUMENTS
void B_PLUS_TREE_INTERNAL_PAGE_TYPE::RemoveAndReturnOnlyChild() {
  SetSize(1);
}

/**
 * Split helper:
 *
 * Suppose current page size is S.
 * We choose mid = S / 2.
 * Promote key_array_[mid] to parent.
 *
 * Left page keeps:
 *   page_id_array_[0 .. mid-1]
 *   key_array_[1 .. mid-1]
 *   size = mid
 *
 * Right page receives:
 *   page_id_array_[mid .. S-1]
 *   key_array_[mid+1 .. S-1]
 * but stored in recipient as:
 *   recipient->page_id_array_[0] = old page_id_array_[mid]
 *   recipient->key_array_[1]     = old key_array_[mid+1]
 *   recipient->page_id_array_[1] = old page_id_array_[mid+1]
 *   ...
 *   recipient size = S - mid
 */
INDEX_TEMPLATE_ARGUMENTS
void B_PLUS_TREE_INTERNAL_PAGE_TYPE::MoveHalfTo(BPlusTreeInternalPage *recipient, KeyType *middle_key) {
  int old_size = GetSize();
  int mid = old_size / 2;

  *middle_key = key_array_[mid];

  recipient->SetSize(0);
  recipient->SetValueAt(0, page_id_array_[mid]);

  int recipient_index = 1;
  for (int i = mid + 1; i < old_size; ++i) {
    recipient->SetKeyAt(recipient_index, key_array_[i]);
    recipient->SetValueAt(recipient_index, page_id_array_[i]);
    recipient_index++;
  }

  recipient->SetSize(old_size - mid);
  SetSize(mid);
}

/**
 * Merge current page into recipient.
 *
 * The key from parent should be inserted between recipient's old last child
 * and current page's first child.
 */
INDEX_TEMPLATE_ARGUMENTS
void B_PLUS_TREE_INTERNAL_PAGE_TYPE::MoveAllTo(BPlusTreeInternalPage *recipient, const KeyType &middle_key) {
  int recipient_size = recipient->GetSize();

  recipient->SetKeyAt(recipient_size, middle_key);
  recipient->SetValueAt(recipient_size, page_id_array_[0]);

  for (int i = 1; i < GetSize(); ++i) {
    recipient->SetKeyAt(recipient_size + i, key_array_[i]);
    recipient->SetValueAt(recipient_size + i, page_id_array_[i]);
  }

  recipient->SetSize(recipient_size + GetSize());
  SetSize(0);
}

/**
 * Move first child from current page to end of recipient.
 * The separator key from parent becomes the appended key in recipient.
 * Current page's first valid key becomes the new parent separator later.
 */
INDEX_TEMPLATE_ARGUMENTS
void B_PLUS_TREE_INTERNAL_PAGE_TYPE::MoveFirstToEndOf(BPlusTreeInternalPage *recipient,
                                                      const KeyType &middle_key) {
  recipient->CopyLastFrom(middle_key, page_id_array_[0]);

  for (int i = 0; i < GetSize() - 1; ++i) {
    page_id_array_[i] = page_id_array_[i + 1];
    key_array_[i] = key_array_[i + 1];
  }

  ChangeSizeBy(-1);
  key_array_[0] = KeyType{};
}

/**
 * Move last child from current page to front of recipient.
 * The separator key from parent becomes recipient's new first valid key.
 */
INDEX_TEMPLATE_ARGUMENTS
void B_PLUS_TREE_INTERNAL_PAGE_TYPE::MoveLastToFrontOf(BPlusTreeInternalPage *recipient,
                                                       const KeyType &middle_key) {
  // move this page's last child pointer to recipient's front,
  // and use middle_key as recipient's first valid key
  recipient->CopyFirstFrom(middle_key, page_id_array_[GetSize() - 1]);
  ChangeSizeBy(-1);
}

/**
 * Copy `size` slots into current page.
 *
 * values[0] is copied to page_id_array_[old_size]
 * keys[1]   is copied to key_array_[old_size + 1], etc.
 *
 * This helper is less critical than MoveHalfTo/MoveAllTo, but useful.
 */
INDEX_TEMPLATE_ARGUMENTS
void B_PLUS_TREE_INTERNAL_PAGE_TYPE::CopyNFrom(const ValueType *values, const KeyType *keys, int size) {
  int old_size = GetSize();
  for (int i = 0; i < size; ++i) {
    page_id_array_[old_size + i] = values[i];
    if (i > 0) {
      key_array_[old_size + i] = keys[i];
    }
  }
  ChangeSizeBy(size);
}

INDEX_TEMPLATE_ARGUMENTS
void B_PLUS_TREE_INTERNAL_PAGE_TYPE::CopyLastFrom(const KeyType &key, const ValueType &value) {
  key_array_[GetSize()] = key;
  page_id_array_[GetSize()] = value;
  ChangeSizeBy(1);
}

INDEX_TEMPLATE_ARGUMENTS
void B_PLUS_TREE_INTERNAL_PAGE_TYPE::CopyFirstFrom(const KeyType &key, const ValueType &value) {
  for (int i = GetSize(); i > 0; --i) {
    key_array_[i] = key_array_[i - 1];
    page_id_array_[i] = page_id_array_[i - 1];
  }

  key_array_[0] = KeyType{};   // still invalid
  key_array_[1] = key;         // new first valid key
  page_id_array_[0] = value;   // new leftmost child pointer
  ChangeSizeBy(1);
}

template class BPlusTreeInternalPage<GenericKey<4>, page_id_t, GenericComparator<4>>;
template class BPlusTreeInternalPage<GenericKey<8>, page_id_t, GenericComparator<8>>;
template class BPlusTreeInternalPage<GenericKey<16>, page_id_t, GenericComparator<16>>;
template class BPlusTreeInternalPage<GenericKey<32>, page_id_t, GenericComparator<32>>;
template class BPlusTreeInternalPage<GenericKey<64>, page_id_t, GenericComparator<64>>;

}  // namespace bustub
