//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// lru_k_replacer.cpp
//
// Identification: src/buffer/lru_k_replacer.cpp
//
// Copyright (c) 2015-2022, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include "buffer/lru_k_replacer.h"
#include "common/exception.h"

namespace bustub {

LRUKReplacer::LRUKReplacer(size_t num_frames, size_t k) : replacer_size_(num_frames), k_(k) {}

auto LRUKReplacer::Evict() -> std::optional<frame_id_t> {
  std::lock_guard<std::mutex> guard(latch_);

  if (curr_size_ == 0) {
    return std::nullopt;
  }

  bool found = false;
  frame_id_t victim = -1;

  bool victim_is_inf = false;
  size_t victim_earliest = 0;
  size_t victim_distance = 0;

  for (auto it = node_store_.begin(); it != node_store_.end(); ++it) {
    const auto &node = it->second;
    if (!node.is_evictable_) {
      continue;
    }

    bool cur_is_inf = node.history_.size() < k_;
    size_t cur_earliest = node.history_.front();

    size_t cur_distance = 0;
    if (!cur_is_inf) {
      auto kth_it = node.history_.begin();
      cur_distance = current_timestamp_ - *kth_it;
    }

    if (!found) {
      found = true;
      victim = it->first;
      victim_is_inf = cur_is_inf;
      victim_earliest = cur_earliest;
      victim_distance = cur_distance;
      continue;
    }

    if (victim_is_inf && cur_is_inf) {
      if (cur_earliest < victim_earliest) {
        victim = it->first;
        victim_earliest = cur_earliest;
      }
    } else if (!victim_is_inf && cur_is_inf) {
      victim = it->first;
      victim_is_inf = true;
      victim_earliest = cur_earliest;
    } else if (!victim_is_inf && !cur_is_inf) {
      if (cur_distance > victim_distance) {
        victim = it->first;
        victim_distance = cur_distance;
      }
    }
  }

  if (!found) {
    return std::nullopt;
  }

  node_store_.erase(victim);
  curr_size_--;
  return victim;
}

void LRUKReplacer::RecordAccess(frame_id_t frame_id, AccessType access_type) {
  std::lock_guard<std::mutex> guard(latch_);

  if (frame_id < 0 || static_cast<size_t>(frame_id) >= replacer_size_) {
    throw Exception("RecordAccess: invalid frame_id");
  }

  current_timestamp_++;

  auto it = node_store_.find(frame_id);
  if (it == node_store_.end()) {
    LRUKNode node;
    node.fid_ = frame_id;
    node.k_ = k_;
    node_store_[frame_id] = node;
    it = node_store_.find(frame_id);
  }

  auto &node = it->second;
  node.history_.push_back(current_timestamp_);

  if (node.history_.size() > k_) {
    node.history_.pop_front();
  }
}

void LRUKReplacer::SetEvictable(frame_id_t frame_id, bool set_evictable) {
  std::lock_guard<std::mutex> guard(latch_);

  if (frame_id < 0 || static_cast<size_t>(frame_id) >= replacer_size_) {
    throw Exception("invalid frame_id");
  }

  auto iter = node_store_.find(frame_id);
  if (iter == node_store_.end()) {
    return;
  }

  if (iter->second.is_evictable_ == set_evictable) {
    return;
  }

  iter->second.is_evictable_ = set_evictable;
  if (set_evictable) {
    curr_size_++;
  } else {
    curr_size_--;
  }
}

void LRUKReplacer::Remove(frame_id_t frame_id) {
  std::lock_guard<std::mutex> guard(latch_);

  if (frame_id < 0 || static_cast<size_t>(frame_id) >= replacer_size_) {
    throw Exception("invalid frame_id");
  }

  auto iter = node_store_.find(frame_id);
  if (iter == node_store_.end()) {
    return;
  }

  if (!iter->second.is_evictable_) {
    throw Exception("frame is not evictable");
  }

  node_store_.erase(iter);
  curr_size_--;
}

auto LRUKReplacer::Size() -> size_t {
  std::lock_guard<std::mutex> guard(latch_);
  return curr_size_;
}
}

