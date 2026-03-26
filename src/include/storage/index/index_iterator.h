#pragma once

#include <utility>

#include "buffer/buffer_pool_manager.h"
#include "storage/page/b_plus_tree_leaf_page.h"

namespace bustub {

#define INDEXITERATOR_TYPE IndexIterator<KeyType, ValueType, KeyComparator>

INDEX_TEMPLATE_ARGUMENTS
class IndexIterator {
 public:
  IndexIterator();
  IndexIterator(BufferPoolManager *bpm, page_id_t leaf_page_id, int index);
  ~IndexIterator();  // NOLINT

  auto IsEnd() -> bool;

  auto operator*() -> std::pair<const KeyType &, const ValueType &>;

  auto operator++() -> IndexIterator &;

  auto operator==(const IndexIterator &itr) const -> bool;
  auto operator!=(const IndexIterator &itr) const -> bool;

 private:
  BufferPoolManager *bpm_{nullptr};
  page_id_t leaf_page_id_{INVALID_PAGE_ID};
  int index_{0};

  mutable KeyType key_holder_{};
  mutable ValueType value_holder_{};
};

}  // namespace bustub