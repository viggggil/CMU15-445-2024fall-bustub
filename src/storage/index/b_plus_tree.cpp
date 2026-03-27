#include "storage/index/b_plus_tree.h"
#include "storage/index/b_plus_tree_debug.h"

namespace bustub {

INDEX_TEMPLATE_ARGUMENTS
BPLUSTREE_TYPE::BPlusTree(std::string name, page_id_t header_page_id, BufferPoolManager *buffer_pool_manager,
                          const KeyComparator &comparator, int leaf_max_size, int internal_max_size)
    : index_name_(std::move(name)),
      bpm_(buffer_pool_manager),
      comparator_(std::move(comparator)),
      leaf_max_size_(leaf_max_size),
      internal_max_size_(internal_max_size),
      header_page_id_(header_page_id) {
  WritePageGuard guard = bpm_->WritePage(header_page_id_);
  auto header_page = guard.AsMut<BPlusTreeHeaderPage>();
  header_page->root_page_id_ = INVALID_PAGE_ID;
}

/*****************************************************************************
 * ROOT / EMPTY
 *****************************************************************************/

INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::ReadRootPageId() const -> page_id_t {
  ReadPageGuard guard = bpm_->ReadPage(header_page_id_);
  auto header_page = guard.As<BPlusTreeHeaderPage>();
  return header_page->root_page_id_;
}

INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::UpdateRootPageId(page_id_t root_page_id) {
  WritePageGuard guard = bpm_->WritePage(header_page_id_);
  auto header_page = guard.AsMut<BPlusTreeHeaderPage>();
  header_page->root_page_id_ = root_page_id;
}

INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::GetRootPageId() -> page_id_t { return ReadRootPageId(); }

INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::IsEmpty() const -> bool { return ReadRootPageId() == INVALID_PAGE_ID; }

INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::GetMinSize(BPlusTreePage *page) const -> int {
  if (page->IsLeafPage()) {
    return (leaf_max_size_ + 1) / 2;
  }
  return (internal_max_size_ + 1) / 2;
}

/*****************************************************************************
 * SEARCH
 *****************************************************************************/

INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::FindLeafRead(const KeyType &key, bool leftmost) -> ReadPageGuard {
  page_id_t cur_page_id = ReadRootPageId();
  BUSTUB_ASSERT(cur_page_id != INVALID_PAGE_ID, "FindLeafRead called on empty tree");

  ReadPageGuard guard = bpm_->ReadPage(cur_page_id);
  auto page = guard.As<BPlusTreePage>();

  while (!page->IsLeafPage()) {
    auto internal = guard.As<InternalPage>();
    page_id_t child_pid = leftmost ? internal->ValueAt(0) : internal->Lookup(key, comparator_);
    guard = bpm_->ReadPage(child_pid);
    page = guard.As<BPlusTreePage>();
  }

  return guard;
}


INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::FindLeafWrite(const KeyType &key, Context *ctx, bool leftmost) -> WritePageGuard {
  ctx->root_page_id_ = GetRootPageId();
  BUSTUB_ASSERT(ctx->root_page_id_ != INVALID_PAGE_ID, "FindLeafWrite called on empty tree");

  page_id_t cur_page_id = ctx->root_page_id_;
  ctx->write_set_.push_back(bpm_->WritePage(cur_page_id));

  auto page = ctx->write_set_.back().template AsMut<BPlusTreePage>();
  while (!page->IsLeafPage()) {
    auto internal = ctx->write_set_.back().template AsMut<InternalPage>();
    page_id_t child_pid = leftmost ? internal->ValueAt(0) : internal->Lookup(key, comparator_);
    ctx->write_set_.push_back(bpm_->WritePage(child_pid));
    page = ctx->write_set_.back().template AsMut<BPlusTreePage>();
  }

  return WritePageGuard{};
}

INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::GetValue(const KeyType &key, std::vector<ValueType> *result) -> bool {
  if (IsEmpty()) {
    return false;
  }

  ReadPageGuard leaf_guard = FindLeafRead(key);
  auto leaf = leaf_guard.As<LeafPage>();

  ValueType value;
  if (leaf->Lookup(key, &value, comparator_)) {
    result->push_back(value);
    return true;
  }
  return false;
}

/*****************************************************************************
 * INSERTION
 *****************************************************************************/

INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::StartNewTree(const KeyType &key, const ValueType &value) {
  page_id_t root_pid = bpm_->NewPage();
  WritePageGuard root_guard = bpm_->WritePage(root_pid);
  auto root = root_guard.AsMut<LeafPage>();
  root->Init(leaf_max_size_);
  root->Insert(key, value, comparator_);
  UpdateRootPageId(root_pid);
}

INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::InsertIntoLeaf(LeafPage *leaf, const KeyType &key, const ValueType &value) -> bool {
  ValueType old_value;
  if (leaf->Lookup(key, &old_value, comparator_)) {
    return false;
  }
  leaf->Insert(key, value, comparator_);
  return true;
}

INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::SplitLeaf(LeafPage *leaf) -> std::pair<page_id_t, KeyType> {
  page_id_t new_leaf_pid = bpm_->NewPage();
  WritePageGuard new_leaf_guard = bpm_->WritePage(new_leaf_pid);
  auto new_leaf = new_leaf_guard.AsMut<LeafPage>();
  new_leaf->Init(leaf_max_size_);

  page_id_t old_next = leaf->GetNextPageId();
  leaf->MoveHalfTo(new_leaf);
  new_leaf->SetNextPageId(old_next);
  leaf->SetNextPageId(new_leaf_pid);

  return {new_leaf_pid, new_leaf->KeyAt(0)};
}

INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::SplitInternal(InternalPage *internal) -> std::pair<page_id_t, KeyType> {
  page_id_t new_internal_pid = bpm_->NewPage();
  WritePageGuard new_internal_guard = bpm_->WritePage(new_internal_pid);
  auto new_internal = new_internal_guard.AsMut<InternalPage>();
  new_internal->Init(internal_max_size_);

  KeyType middle_key{};
  internal->MoveHalfTo(new_internal, &middle_key);
  return {new_internal_pid, middle_key};
}

INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::CreateNewRoot(page_id_t old_node_pid, const KeyType &middle_key, page_id_t new_node_pid) {
  page_id_t new_root_pid = bpm_->NewPage();
  WritePageGuard root_guard = bpm_->WritePage(new_root_pid);
  auto root = root_guard.AsMut<InternalPage>();
  root->Init(internal_max_size_);
  root->PopulateNewRoot(old_node_pid, middle_key, new_node_pid);
  UpdateRootPageId(new_root_pid);
}

INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::InsertIntoParent(page_id_t old_node_pid, const KeyType &middle_key, page_id_t new_node_pid,
                                      Context *ctx) {
  // old node is root
  if (ctx->write_set_.size() == 1) {
    CreateNewRoot(old_node_pid, middle_key, new_node_pid);
    return;
  }

  // pop old node itself, parent becomes new back()
  ctx->write_set_.pop_back();
  auto parent = ctx->write_set_.back().template AsMut<InternalPage>();
  parent->InsertNodeAfter(old_node_pid, middle_key, new_node_pid);

  if (parent->GetSize() <= parent->GetMaxSize()) {
    return;
  }

  page_id_t old_parent_pid = ctx->write_set_.back().GetPageId();
  auto [new_internal_pid, promoted_key] = SplitInternal(parent);
  InsertIntoParent(old_parent_pid, promoted_key, new_internal_pid, ctx);
}

INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::Insert(const KeyType &key, const ValueType &value) -> bool {
  if (IsEmpty()) {
    StartNewTree(key, value);
    return true;
  }

  Context ctx;
  ctx.root_page_id_ = GetRootPageId();

  FindLeafWrite(key, &ctx, false);
  auto leaf = ctx.write_set_.back().template AsMut<LeafPage>();

  if (!InsertIntoLeaf(leaf, key, value)) {
    return false;
  }

  // Important:
  // if this leaf is not the leftmost child, and its first key changed,
  // parent separator should follow leaf->KeyAt(0).
  UpdateParentKeyAfterLeafChange(&ctx);

  if (leaf->GetSize() <= leaf->GetMaxSize()) {
    return true;
  }

  page_id_t old_leaf_pid = ctx.write_set_.back().GetPageId();
  auto [new_leaf_pid, middle_key] = SplitLeaf(leaf);
  InsertIntoParent(old_leaf_pid, middle_key, new_leaf_pid, &ctx);
  return true;
}


/*****************************************************************************
 * REMOVE
 *****************************************************************************/

INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::AdjustRoot(BPlusTreePage *old_root) {
  if (old_root->IsLeafPage()) {
    auto leaf_root = reinterpret_cast<LeafPage *>(old_root);
    if (leaf_root->GetSize() == 0) {
      UpdateRootPageId(INVALID_PAGE_ID);
    }
    return;
  }

  auto internal_root = reinterpret_cast<InternalPage *>(old_root);

  // Internal root with only one child: promote that child.
  if (internal_root->GetSize() == 1) {
    page_id_t child_pid = internal_root->ValueAt(0);

    WritePageGuard child_guard = bpm_->WritePage(child_pid);
    auto child_page = child_guard.template AsMut<BPlusTreePage>();

    // If the only child is an empty leaf, tree becomes empty.
    if (child_page->IsLeafPage()) {
      auto child_leaf = reinterpret_cast<LeafPage *>(child_page);
      if (child_leaf->GetSize() == 0) {
        UpdateRootPageId(INVALID_PAGE_ID);
        return;
      }
    }

    UpdateRootPageId(child_pid);
    return;
  }

  if (internal_root->GetSize() == 0) {
    UpdateRootPageId(INVALID_PAGE_ID);
  }
}

INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::RedistributeInternal(WritePageGuard *left_guard, WritePageGuard *right_guard,
                                          WritePageGuard *parent_guard, int parent_sep_index,
                                          bool right_is_target) {
  auto left = left_guard->template AsMut<InternalPage>();
  auto right = right_guard->template AsMut<InternalPage>();
  auto parent = parent_guard->template AsMut<InternalPage>();

  if (right_is_target) {
    // borrow one from left to right
    KeyType old_parent_key = parent->KeyAt(parent_sep_index);
    KeyType new_parent_key = left->KeyAt(left->GetSize() - 1);  // save BEFORE move
    left->MoveLastToFrontOf(right, old_parent_key);
    parent->SetKeyAt(parent_sep_index, new_parent_key);
  } else {
    // borrow one from right to left
    KeyType old_parent_key = parent->KeyAt(parent_sep_index);
    KeyType new_parent_key = right->KeyAt(1);  // save BEFORE move
    right->MoveFirstToEndOf(left, old_parent_key);
    parent->SetKeyAt(parent_sep_index, new_parent_key);
  }
}

INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::CoalesceInternal(WritePageGuard *left_guard, WritePageGuard *right_guard,
                                      WritePageGuard *parent_guard, int parent_sep_index,
                                      bool right_is_target) {
  auto left = left_guard->template AsMut<InternalPage>();
  auto right = right_guard->template AsMut<InternalPage>();
  auto parent = parent_guard->template AsMut<InternalPage>();

  KeyType middle_key = parent->KeyAt(parent_sep_index);

  // Always merge right into left
  right->MoveAllTo(left, middle_key);

  // Remove the separator / child slot corresponding to the right child
  parent->Remove(parent_sep_index);

  (void)right_is_target;
}

INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::RedistributeLeaf(WritePageGuard *left_guard, WritePageGuard *right_guard, WritePageGuard *parent_guard,
                                      int parent_sep_index, bool right_is_target) {
  auto left = left_guard->template AsMut<LeafPage>();
  auto right = right_guard->template AsMut<LeafPage>();
  auto parent = parent_guard->template AsMut<InternalPage>();

  if (right_is_target) {
    left->MoveLastToFrontOf(right);
    parent->SetKeyAt(parent_sep_index, right->KeyAt(0));
  } else {
    right->MoveFirstToEndOf(left);
    parent->SetKeyAt(parent_sep_index, right->KeyAt(0));
  }
}

INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::UpdateParentKeyAfterLeafChange(Context *ctx) {
  if (ctx->write_set_.size() < 2) {
    return;
  }

  KeyType current_min_key;
  {
    auto leaf = ctx->write_set_.back().template AsMut<LeafPage>();
    if (leaf->GetSize() == 0) {
      return;
    }
    current_min_key = leaf->KeyAt(0);
  }

  // child_pos 指向“当前子节点”在 write_set_ 里的位置
  for (int child_pos = static_cast<int>(ctx->write_set_.size()) - 1; child_pos > 0; --child_pos) {
    auto &child_guard = ctx->write_set_[child_pos];
    auto &parent_guard = ctx->write_set_[child_pos - 1];
    auto parent = parent_guard.template AsMut<InternalPage>();

    int value_index = parent->ValueIndex(child_guard.GetPageId());
    BUSTUB_ASSERT(value_index != -1, "child page not found in parent");

    if (value_index > 0) {
      // 当前子节点不是最左孩子：
      // 只需要更新这一层 parent 的 separator
      // 但 parent 整棵子树的最小值没变，必须停止向上传播
      parent->SetKeyAt(value_index, current_min_key);
      break;
    }

    // value_index == 0:
    // 当前子节点是最左孩子，parent 自己的子树最小值变了，
    // 需要继续向上找祖先更新
  }
}

INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::CoalesceLeaf(WritePageGuard *left_guard, WritePageGuard *right_guard,
                                  WritePageGuard *parent_guard, int parent_sep_index, bool right_is_target) {
  auto left = left_guard->template AsMut<LeafPage>();
  auto right = right_guard->template AsMut<LeafPage>();
  auto parent = parent_guard->template AsMut<InternalPage>();

  // Always merge right page into left page.
  right->MoveAllTo(left);
  left->SetNextPageId(right->GetNextPageId());

  // Remove the separator / child slot corresponding to the right child.
  parent->Remove(parent_sep_index);

  (void)right_is_target;
}
INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::CoalesceOrRedistribute(WritePageGuard *node_guard, Context *ctx) {
  auto node = node_guard->template AsMut<BPlusTreePage>();

  if (ctx->IsRootPage(node_guard->GetPageId())) {
    AdjustRoot(node);
    return;
  }

  if (node->GetSize() >= GetMinSize(node)) {
    return;
  }

  if (node->IsLeafPage()) {
    BUSTUB_ASSERT(ctx->write_set_.size() >= 2, "Leaf underflow without parent");

    auto &parent_guard = ctx->write_set_[ctx->write_set_.size() - 2];
    auto parent = parent_guard.template AsMut<InternalPage>();

    page_id_t node_pid = node_guard->GetPageId();
    int value_index = parent->ValueIndex(node_pid);

    if (value_index > 0) {
      page_id_t left_pid = parent->ValueAt(value_index - 1);
      WritePageGuard left_guard = bpm_->WritePage(left_pid);
      auto left = left_guard.AsMut<LeafPage>();

      if (left->GetSize() > GetMinSize(left)) {
        RedistributeLeaf(&left_guard, node_guard, &parent_guard, value_index, true);
        return;
      }
    }

    if (value_index + 1 < parent->GetSize()) {
      page_id_t right_pid = parent->ValueAt(value_index + 1);
      WritePageGuard right_guard = bpm_->WritePage(right_pid);
      auto right = right_guard.AsMut<LeafPage>();

      if (right->GetSize() > GetMinSize(right)) {
        RedistributeLeaf(node_guard, &right_guard, &parent_guard, value_index + 1, false);
        return;
      }
    }

    if (value_index > 0) {
      page_id_t left_pid = parent->ValueAt(value_index - 1);
      WritePageGuard left_guard = bpm_->WritePage(left_pid);
      CoalesceLeaf(&left_guard, node_guard, &parent_guard, value_index, true);
    } else {
      page_id_t right_pid = parent->ValueAt(value_index + 1);
      WritePageGuard right_guard = bpm_->WritePage(right_pid);
      CoalesceLeaf(node_guard, &right_guard, &parent_guard, value_index + 1, false);
    }

    ctx->write_set_.pop_back();
    CoalesceOrRedistribute(&ctx->write_set_.back(), ctx);
    return;
  }

  // ===== internal page underflow handling =====
  BUSTUB_ASSERT(ctx->write_set_.size() >= 2, "Internal underflow without parent");

  auto &parent_guard = ctx->write_set_[ctx->write_set_.size() - 2];
  auto parent = parent_guard.template AsMut<InternalPage>();

  page_id_t node_pid = node_guard->GetPageId();
  int value_index = parent->ValueIndex(node_pid);

  // Try borrow from left sibling
  if (value_index > 0) {
    page_id_t left_pid = parent->ValueAt(value_index - 1);
    WritePageGuard left_guard = bpm_->WritePage(left_pid);
    auto left = left_guard.AsMut<InternalPage>();

    if (left->GetSize() > GetMinSize(left)) {
      RedistributeInternal(&left_guard, node_guard, &parent_guard, value_index, true);
      return;
    }
  }

  // Try borrow from right sibling
  if (value_index + 1 < parent->GetSize()) {
    page_id_t right_pid = parent->ValueAt(value_index + 1);
    WritePageGuard right_guard = bpm_->WritePage(right_pid);
    auto right = right_guard.AsMut<InternalPage>();

    if (right->GetSize() > GetMinSize(right)) {
      RedistributeInternal(node_guard, &right_guard, &parent_guard, value_index + 1, false);
      return;
    }
  }

  // Need coalesce
  if (value_index > 0) {
    page_id_t left_pid = parent->ValueAt(value_index - 1);
    WritePageGuard left_guard = bpm_->WritePage(left_pid);
    CoalesceInternal(&left_guard, node_guard, &parent_guard, value_index, true);
  } else {
    page_id_t right_pid = parent->ValueAt(value_index + 1);
    WritePageGuard right_guard = bpm_->WritePage(right_pid);
    CoalesceInternal(node_guard, &right_guard, &parent_guard, value_index + 1, false);
  }

  ctx->write_set_.pop_back();
  CoalesceOrRedistribute(&ctx->write_set_.back(), ctx);
}



INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::Remove(const KeyType &key) {
  if (IsEmpty()) {
    return;
  }

  Context ctx;
  ctx.root_page_id_ = GetRootPageId();

  FindLeafWrite(key, &ctx, false);
  auto leaf = ctx.write_set_.back().template AsMut<LeafPage>();

  ValueType old_value;
  if (!leaf->Lookup(key, &old_value, comparator_)) {
    return;
  }

  leaf->RemoveAndDeleteRecord(key, comparator_);

  if (ctx.IsRootPage(ctx.write_set_.back().GetPageId())) {
    AdjustRoot(leaf);
    return;
  }

  // Important:
  // if delete changes this leaf's first key but does not trigger coalesce,
  // parent separator still needs update.
  UpdateParentKeyAfterLeafChange(&ctx);

  if (leaf->GetSize() >= GetMinSize(leaf)) {
    return;
  }

  CoalesceOrRedistribute(&ctx.write_set_.back(), &ctx);
}


/*****************************************************************************
 * INDEX ITERATOR
 *****************************************************************************/

INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::Begin() -> INDEXITERATOR_TYPE {
  if (IsEmpty()) {
    return End();
  }

  ReadPageGuard leaf_guard = FindLeafRead(KeyType{}, true);
  return INDEXITERATOR_TYPE(bpm_, leaf_guard.GetPageId(), 0);
}

INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::Begin(const KeyType &key) -> INDEXITERATOR_TYPE {
  if (IsEmpty()) {
    return End();
  }

  ReadPageGuard leaf_guard = FindLeafRead(key, false);
  auto leaf = leaf_guard.As<LeafPage>();
  int index = leaf->KeyIndex(key, comparator_);
  return INDEXITERATOR_TYPE(bpm_, leaf_guard.GetPageId(), index);
}

INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::End() -> INDEXITERATOR_TYPE { return INDEXITERATOR_TYPE(bpm_, INVALID_PAGE_ID, 0); }

/*****************************************************************************
 * Explicit template instantiation
 *****************************************************************************/

template class BPlusTree<GenericKey<4>, RID, GenericComparator<4>>;
template class BPlusTree<GenericKey<8>, RID, GenericComparator<8>>;
template class BPlusTree<GenericKey<16>, RID, GenericComparator<16>>;
template class BPlusTree<GenericKey<32>, RID, GenericComparator<32>>;
template class BPlusTree<GenericKey<64>, RID, GenericComparator<64>>;

}  // namespace bustub