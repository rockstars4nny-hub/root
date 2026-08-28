#pragma once
#include <esp_heap_caps.h>
#include <cstddef>
#include <limits>

template <typename T>
struct PSRAMAllocator {
  using value_type = T;

  PSRAMAllocator() noexcept = default;
  template <typename U>
  PSRAMAllocator(const PSRAMAllocator<U>&) noexcept {}

  T* allocate(std::size_t n) {
    if (n > std::numeric_limits<std::size_t>::max() / sizeof(T)) return nullptr;
    void* p = heap_caps_malloc(n * sizeof(T), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!p) p = malloc(n * sizeof(T));
    return static_cast<T*>(p);
  }

  void deallocate(T* p, std::size_t) noexcept { free(p); }
};

template <typename T, typename U>
bool operator==(const PSRAMAllocator<T>&, const PSRAMAllocator<U>&) {
  return true;
}

template <typename T, typename U>
bool operator!=(const PSRAMAllocator<T>&, const PSRAMAllocator<U>&) {
  return false;
}
