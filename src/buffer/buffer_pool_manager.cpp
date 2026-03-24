#include "buffer/buffer_pool_manager.h"

#include <future>
#include <optional>

namespace bustub {

FrameHeader::FrameHeader(frame_id_t frame_id) : frame_id_(frame_id), data_(BUSTUB_PAGE_SIZE, 0) { Reset(); }

auto FrameHeader::GetData() const -> const char * { return data_.data(); }

auto FrameHeader::GetDataMut() -> char * { return data_.data(); }

void FrameHeader::Reset() {
  std::fill(data_.begin(), data_.end(), 0);
  pin_count_.store(0);
  is_dirty_ = false;
  page_id_ = INVALID_PAGE_ID;
}

BufferPoolManager::BufferPoolManager(size_t num_frames, DiskManager *disk_manager, size_t k_dist,
                                     LogManager *log_manager)
    : num_frames_(num_frames),
      next_page_id_(0),
      bpm_latch_(std::make_shared<std::mutex>()),
      replacer_(std::make_shared<LRUKReplacer>(num_frames, k_dist)),
      disk_scheduler_(std::make_unique<DiskScheduler>(disk_manager)),
      log_manager_(log_manager) {
  std::scoped_lock latch(*bpm_latch_);

  frames_.reserve(num_frames_);
  page_table_.reserve(num_frames_);

  for (size_t i = 0; i < num_frames_; i++) {
    frames_.push_back(std::make_shared<FrameHeader>(static_cast<frame_id_t>(i)));
    free_frames_.push_back(static_cast<frame_id_t>(i));
  }
}

BufferPoolManager::~BufferPoolManager() = default;

auto BufferPoolManager::Size() const -> size_t { return num_frames_; }

auto BufferPoolManager::GetAvailableFrame() -> std::optional<frame_id_t> {
  if (!free_frames_.empty()) {
    frame_id_t frame_id = free_frames_.front();
    free_frames_.pop_front();
    return frame_id;
  }
  return replacer_->Evict();
}

void BufferPoolManager::FlushFrameUnlocked(frame_id_t frame_id) {
  auto &frame = frames_[frame_id];
  if (frame->page_id_ == INVALID_PAGE_ID || !frame->is_dirty_) {
    return;
  }

  auto promise = disk_scheduler_->CreatePromise();
  auto future = promise.get_future();

  DiskRequest req;
  req.is_write_ = true;
  req.data_ = frame->GetDataMut();
  req.page_id_ = frame->page_id_;
  req.callback_ = std::move(promise);

  disk_scheduler_->Schedule(std::move(req));
  future.get();

  frame->is_dirty_ = false;
}

void BufferPoolManager::BringPageIntoFrame(page_id_t page_id, frame_id_t frame_id, AccessType access_type) {
  auto &frame = frames_[frame_id];

  if (frame->page_id_ != INVALID_PAGE_ID) {
    FlushFrameUnlocked(frame_id);
    page_table_.erase(frame->page_id_);
  }

  frame->Reset();

  auto promise = disk_scheduler_->CreatePromise();
  auto future = promise.get_future();

  DiskRequest req;
  req.is_write_ = false;
  req.data_ = frame->GetDataMut();
  req.page_id_ = page_id;
  req.callback_ = std::move(promise);

  disk_scheduler_->Schedule(std::move(req));
  future.get();

  frame->page_id_ = page_id;
  frame->pin_count_.store(1);
  frame->is_dirty_ = false;

  page_table_[page_id] = frame_id;
  replacer_->RecordAccess(frame_id, access_type);
  replacer_->SetEvictable(frame_id, false);
}

auto BufferPoolManager::NewPage() -> page_id_t {
  std::scoped_lock latch(*bpm_latch_);

  auto frame_opt = GetAvailableFrame();
  if (!frame_opt.has_value()) {
    return INVALID_PAGE_ID;
  }

  page_id_t new_page_id = next_page_id_.fetch_add(1);
  disk_scheduler_->IncreaseDiskSpace(static_cast<size_t>(new_page_id) + 1);

  frame_id_t frame_id = frame_opt.value();
  auto &frame = frames_[frame_id];

  if (frame->page_id_ != INVALID_PAGE_ID) {
    FlushFrameUnlocked(frame_id);
    page_table_.erase(frame->page_id_);
  }

  frame->Reset();
  frame->page_id_ = new_page_id;
  frame->pin_count_.store(0);
  frame->is_dirty_ = false;

  page_table_[new_page_id] = frame_id;
  replacer_->RecordAccess(frame_id, AccessType::Unknown);
  replacer_->SetEvictable(frame_id, true);

  return new_page_id;
}

auto BufferPoolManager::DeletePage(page_id_t page_id) -> bool {
  std::scoped_lock latch(*bpm_latch_);

  auto it = page_table_.find(page_id);
  if (it == page_table_.end()) {
    disk_scheduler_->DeallocatePage(page_id);
    return true;
  }

  frame_id_t frame_id = it->second;
  auto &frame = frames_[frame_id];

  if (frame->pin_count_.load() > 0) {
    return false;
  }

  page_table_.erase(it);
  replacer_->Remove(frame_id);
  frame->Reset();
  free_frames_.push_back(frame_id);

  disk_scheduler_->DeallocatePage(page_id);
  return true;
}

auto BufferPoolManager::CheckedWritePage(page_id_t page_id, AccessType access_type)
    -> std::optional<WritePageGuard> {
  std::shared_ptr<FrameHeader> frame;

  {
    std::scoped_lock latch(*bpm_latch_);

    auto it = page_table_.find(page_id);
    if (it != page_table_.end()) {
      frame_id_t frame_id = it->second;
      frame = frames_[frame_id];

      frame->pin_count_.fetch_add(1);
      replacer_->RecordAccess(frame_id, access_type);
      replacer_->SetEvictable(frame_id, false);
    } else {
      auto frame_opt = GetAvailableFrame();
      if (!frame_opt.has_value()) {
        return std::nullopt;
      }

      frame_id_t frame_id = frame_opt.value();
      BringPageIntoFrame(page_id, frame_id, access_type);
      frame = frames_[frame_id];
    }
  }

  return WritePageGuard(page_id, frame, replacer_, bpm_latch_);
}

auto BufferPoolManager::CheckedReadPage(page_id_t page_id, AccessType access_type)
    -> std::optional<ReadPageGuard> {
  std::shared_ptr<FrameHeader> frame;

  {
    std::scoped_lock latch(*bpm_latch_);

    auto it = page_table_.find(page_id);
    if (it != page_table_.end()) {
      frame_id_t frame_id = it->second;
      frame = frames_[frame_id];

      frame->pin_count_.fetch_add(1);
      replacer_->RecordAccess(frame_id, access_type);
      replacer_->SetEvictable(frame_id, false);
    } else {
      auto frame_opt = GetAvailableFrame();
      if (!frame_opt.has_value()) {
        return std::nullopt;
      }

      frame_id_t frame_id = frame_opt.value();
      BringPageIntoFrame(page_id, frame_id, access_type);
      frame = frames_[frame_id];
    }
  }

  return ReadPageGuard(page_id, frame, replacer_, bpm_latch_);
}

auto BufferPoolManager::WritePage(page_id_t page_id, AccessType access_type) -> WritePageGuard {
  auto guard_opt = CheckedWritePage(page_id, access_type);
  if (!guard_opt.has_value()) {
    fmt::println(stderr, "\n`CheckedWritePage` failed to bring in page {}\n", page_id);
    std::abort();
  }
  return std::move(guard_opt).value();
}

auto BufferPoolManager::ReadPage(page_id_t page_id, AccessType access_type) -> ReadPageGuard {
  auto guard_opt = CheckedReadPage(page_id, access_type);
  if (!guard_opt.has_value()) {
    fmt::println(stderr, "\n`CheckedReadPage` failed to bring in page {}\n", page_id);
    std::abort();
  }
  return std::move(guard_opt).value();
}

auto BufferPoolManager::FlushPage(page_id_t page_id) -> bool {
  std::scoped_lock latch(*bpm_latch_);

  auto it = page_table_.find(page_id);
  if (it == page_table_.end()) {
    return false;
  }

  FlushFrameUnlocked(it->second);
  return true;
}

void BufferPoolManager::FlushAllPages() {
  std::scoped_lock latch(*bpm_latch_);

  for (const auto &[page_id, frame_id] : page_table_) {
    (void)page_id;
    FlushFrameUnlocked(frame_id);
  }
}

auto BufferPoolManager::GetPinCount(page_id_t page_id) -> std::optional<size_t> {
  std::scoped_lock latch(*bpm_latch_);

  auto it = page_table_.find(page_id);
  if (it == page_table_.end()) {
    return std::nullopt;
  }

  return frames_[it->second]->pin_count_.load();
}

}  // namespace bustub