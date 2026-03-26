#include <cassert>

#include "storage/index/index_iterator.h"

namespace bustub {

INDEX_TEMPLATE_ARGUMENTS
INDEXITERATOR_TYPE::IndexIterator() = default;

INDEX_TEMPLATE_ARGUMENTS
INDEXITERATOR_TYPE::IndexIterator(BufferPoolManager *bpm, page_id_t leaf_page_id, int index)
    : bpm_(bpm), leaf_page_id_(leaf_page_id), index_(index) {}

INDEX_TEMPLATE_ARGUMENTS
INDEXITERATOR_TYPE::~IndexIterator() = default;  // NOLINT

INDEX_TEMPLATE_ARGUMENTS
auto INDEXITERATOR_TYPE::IsEnd() -> bool {
  return leaf_page_id_ == INVALID_PAGE_ID;
}

INDEX_TEMPLATE_ARGUMENTS
auto INDEXITERATOR_TYPE::operator*() -> std::pair<const KeyType &, const ValueType &> {
  ReadPageGuard guard = bpm_->ReadPage(leaf_page_id_);
  auto leaf = guard.As<BPlusTreeLeafPage<KeyType, ValueType, KeyComparator>>();

  key_holder_ = leaf->KeyAt(index_);
  value_holder_ = leaf->ValueAt(index_);
  return {key_holder_, value_holder_};
}

INDEX_TEMPLATE_ARGUMENTS
auto INDEXITERATOR_TYPE::operator++() -> INDEXITERATOR_TYPE & {
  if (IsEnd()) {
    return *this;
  }

  ReadPageGuard guard = bpm_->ReadPage(leaf_page_id_);
  auto leaf = guard.As<BPlusTreeLeafPage<KeyType, ValueType, KeyComparator>>();

  index_++;
  if (index_ >= leaf->GetSize()) {
    page_id_t next_page_id = leaf->GetNextPageId();
    if (next_page_id == INVALID_PAGE_ID) {
      leaf_page_id_ = INVALID_PAGE_ID;
      index_ = 0;
    } else {
      leaf_page_id_ = next_page_id;
      index_ = 0;
    }
  }

  return *this;
}

INDEX_TEMPLATE_ARGUMENTS
auto INDEXITERATOR_TYPE::operator==(const IndexIterator &itr) const -> bool {
  return bpm_ == itr.bpm_ && leaf_page_id_ == itr.leaf_page_id_ && index_ == itr.index_;
}

INDEX_TEMPLATE_ARGUMENTS
auto INDEXITERATOR_TYPE::operator!=(const IndexIterator &itr) const -> bool {
  return !(*this == itr);
}

template class IndexIterator<GenericKey<4>, RID, GenericComparator<4>>;
template class IndexIterator<GenericKey<8>, RID, GenericComparator<8>>;
template class IndexIterator<GenericKey<16>, RID, GenericComparator<16>>;
template class IndexIterator<GenericKey<32>, RID, GenericComparator<32>>;
template class IndexIterator<GenericKey<64>, RID, GenericComparator<64>>;

}  // namespace bustub
