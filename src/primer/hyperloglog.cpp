#include "primer/hyperloglog.h"

namespace bustub {

template <typename KeyType>
HyperLogLog<KeyType>::HyperLogLog(int16_t n_bits) : cardinality_(0) {
  if (n_bits < 0) {
    n_bits_ = 0;
    m_ = 0;
    return;
  }

  n_bits_ = n_bits;
  m_ = static_cast<size_t>(1ULL << n_bits_);
  registers_ = std::vector<uint64_t>(m_, 0);
}

template <typename KeyType>
auto HyperLogLog<KeyType>::ComputeBinary(const hash_t &hash) const -> std::bitset<BITSET_CAPACITY> {
  return std::bitset<BITSET_CAPACITY>(hash);
}

template <typename KeyType>
auto HyperLogLog<KeyType>::PositionOfLeftmostOne(const std::bitset<BITSET_CAPACITY> &bset) const -> uint64_t {
  if (static_cast<size_t>(n_bits_) >= BITSET_CAPACITY) {
    return 1;
  }

  uint64_t pos = 1;
  for (int64_t i = static_cast<int64_t>(BITSET_CAPACITY) - 1 - n_bits_; i >= 0; i--) {
    if (bset[static_cast<size_t>(i)]) {
      return pos;
    }
    pos++;
  }
  return pos;
}

template <typename KeyType>
auto HyperLogLog<KeyType>::AddElem(KeyType val) -> void {
  if (m_ == 0) {
    return;
  }

  auto hash = CalculateHash(val);
  auto bset = ComputeBinary(hash);

  size_t bucket_idx = 0;
  for (int16_t i = 0; i < n_bits_; i++) {
    bucket_idx <<= 1;
    if (bset[BITSET_CAPACITY - 1 - i]) {
      bucket_idx |= 1ULL;
    }
  }

  uint64_t pos = PositionOfLeftmostOne(bset);

  std::lock_guard<std::mutex> guard(latch_);
  registers_[bucket_idx] = std::max(registers_[bucket_idx], pos);
}

template <typename KeyType>
auto HyperLogLog<KeyType>::ComputeCardinality() -> void {
  if (m_ == 0) {
    cardinality_ = 0;
    return;
  }

  double sum = 0.0;
  for (size_t i = 0; i < m_; i++) {
    sum += std::pow(2.0, -static_cast<double>(registers_[i]));
  }

  double estimate = CONSTANT * static_cast<double>(m_) * static_cast<double>(m_) / sum;
  cardinality_ = static_cast<size_t>(std::floor(estimate));
}
template class HyperLogLog<int64_t>;
template class HyperLogLog<std::string>;

}  // namespace bustub
