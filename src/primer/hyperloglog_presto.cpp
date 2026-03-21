#include "primer/hyperloglog_presto.h"

namespace bustub {

template <typename KeyType>
HyperLogLogPresto<KeyType>::HyperLogLogPresto(int16_t n_leading_bits) : cardinality_(0) {
  if (n_leading_bits < 0) {
    n_bits_ = 0;
    m_ = 0;
    return;
  }

  n_bits_ = n_leading_bits;
  m_ = static_cast<uint16_t>(1U << n_bits_);
  dense_bucket_ = std::vector<std::bitset<DENSE_BUCKET_SIZE>>(m_, std::bitset<DENSE_BUCKET_SIZE>(0));
}

template <typename KeyType>
auto HyperLogLogPresto<KeyType>::AddElem(KeyType val) -> void {
  if (m_ == 0) {
    return;
  }

  auto hash = CalculateHash(val);
  std::bitset<BITSET_CAPACITY> bset(static_cast<uint64_t>(hash));

  uint16_t bucket_idx = 0;
  for (int16_t i = 0; i < n_bits_; i++) {
    bucket_idx <<= 1;
    if (bset[BITSET_CAPACITY - 1 - i]) {
      bucket_idx |= 1U;
    }
  }

  uint64_t zero_count = 0;
  for (int64_t i = 0; i < static_cast<int64_t>(BITSET_CAPACITY) - n_bits_; i++) {
    if (bset[static_cast<size_t>(i)]) {
      break;
    }
    zero_count++;
  }

  uint64_t new_val = zero_count;
  uint64_t new_dense = new_val & ((1ULL << DENSE_BUCKET_SIZE) - 1);
  uint64_t new_overflow = new_val >> DENSE_BUCKET_SIZE;

  std::lock_guard<std::mutex> guard(latch_);

  uint64_t cur_dense = dense_bucket_[bucket_idx].to_ullong();
  uint64_t cur_overflow = 0;
  if (overflow_bucket_.count(bucket_idx) != 0) {
    cur_overflow = overflow_bucket_[bucket_idx].to_ullong();
  }
  uint64_t cur_val = (cur_overflow << DENSE_BUCKET_SIZE) | cur_dense;

  if (new_val >= cur_val) {
    dense_bucket_[bucket_idx] = std::bitset<DENSE_BUCKET_SIZE>(new_dense);
    if (new_overflow == 0) {
      overflow_bucket_.erase(bucket_idx);
    } else {
      overflow_bucket_[bucket_idx] = std::bitset<OVERFLOW_BUCKET_SIZE>(new_overflow);
    }
  }
}

template <typename T>
auto HyperLogLogPresto<T>::ComputeCardinality() -> void {
  if (m_ == 0) {
    cardinality_ = 0;
    return;
  }

  double sum = 0.0;
  for (uint16_t i = 0; i < m_; i++) {
    uint64_t dense = dense_bucket_[i].to_ullong();
    uint64_t overflow = 0;
    if (overflow_bucket_.count(i) != 0) {
      overflow = overflow_bucket_[i].to_ullong();
    }
    uint64_t value = (overflow << DENSE_BUCKET_SIZE) | dense;
    sum += std::pow(2.0, -static_cast<double>(value));
  }

  double estimate = CONSTANT * static_cast<double>(m_) * static_cast<double>(m_) / sum;
  cardinality_ = static_cast<uint64_t>(std::floor(estimate));
}

template class HyperLogLogPresto<int64_t>;
template class HyperLogLogPresto<std::string>;
}  // namespace bustub
