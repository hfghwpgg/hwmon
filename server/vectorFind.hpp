#pragma once
#include <vector>
#include <algorithm>

template <typename T>
static bool isInVector(const std::vector<T> &vec, const T &thing) {
    return (std::find(vec.begin(), vec.end(), thing) != vec.end());
}