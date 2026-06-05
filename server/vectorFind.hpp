#pragma once
#include <algorithm>
#include <vector>

template <typename T> static bool IsInVector(const std::vector<T> &vec, const T &thing) {
  return (std::find(vec.begin(), vec.end(), thing) != vec.end());
}
