#pragma once

#include <cstdlib>
#include <memory>
#include <vector>

class ASTContext {
  std::vector<void*> slabs_;

 public:
  ASTContext() = default;

  template <typename T>
  void* Allocate() {
    void* mem = malloc(sizeof(T));
    slabs_.push_back(mem);
    return mem;
  }

  static void Deallocate(void* mem) { free(mem); }

  ~ASTContext() {
    for (auto& slab : slabs_) {
      Deallocate(slab);
    }
  }
};
