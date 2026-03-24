#pragma once

#include <list>
#include <memory>
#include <optional>
#include <shared_mutex>
#include <unordered_map>
#include <vector>

#include "buffer/lru_k_replacer.h"
#include "common/config.h"
#include "recovery/log_manager.h"
#include "storage/disk/disk_scheduler.h"
#include "storage/page/page.h"
#include "storage/page/page_guard.h"

namespace bustub {

class BufferPoolManager;
class ReadPageGuard;
class WritePageGuard;

class FrameHeader {
  friend class BufferPoolManager;
  friend class ReadPageGuard;
  friend class WritePageGuard;

 public:
  explicit FrameHeader(frame_id_t frame_id);

 private:
  auto GetData() const -> const char *;
  auto GetDataMut() -> char *;
  void Reset();

  const frame_id_t frame_id_;
  std::shared_mutex rwlatch_;
  std::atomic<size_t> pin_count_;
  bool is_dirty_;
  std::vector<char> data_;

  // 新增：当前 frame 中装载的 page
  page_id_t page_id_{INVALID_PAGE_ID};
};

class BufferPoolManager {
 public:
  BufferPoolManager(size_t num_frames, DiskManager *disk_manager, size_t k_dist = LRUK_REPLACER_K,
                    LogManager *log_manager = nullptr);
  ~BufferPoolManager();

  auto Size() const -> size_t;
  auto NewPage() -> page_id_t;
  auto DeletePage(page_id_t page_id) -> bool;
  auto CheckedWritePage(page_id_t page_id, AccessType access_type = AccessType::Unknown)
      -> std::optional<WritePageGuard>;
  auto CheckedReadPage(page_id_t page_id, AccessType access_type = AccessType::Unknown) -> std::optional<ReadPageGuard>;
  auto WritePage(page_id_t page_id, AccessType access_type = AccessType::Unknown) -> WritePageGuard;
  auto ReadPage(page_id_t page_id, AccessType access_type = AccessType::Unknown) -> ReadPageGuard;
  auto FlushPage(page_id_t page_id) -> bool;
  void FlushAllPages();
  auto GetPinCount(page_id_t page_id) -> std::optional<size_t>;

 private:
  const size_t num_frames_;
  std::atomic<page_id_t> next_page_id_;
  std::shared_ptr<std::mutex> bpm_latch_;
  std::vector<std::shared_ptr<FrameHeader>> frames_;
  std::unordered_map<page_id_t, frame_id_t> page_table_;
  std::list<frame_id_t> free_frames_;
  std::shared_ptr<LRUKReplacer> replacer_;
  std::unique_ptr<DiskScheduler> disk_scheduler_;
  LogManager *log_manager_ __attribute__((__unused__));

  auto GetAvailableFrame() -> std::optional<frame_id_t>;
  void FlushFrameUnlocked(frame_id_t frame_id);
  void BringPageIntoFrame(page_id_t page_id, frame_id_t frame_id, AccessType access_type);
};

}  // namespace bustub