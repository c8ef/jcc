#pragma once

#include "jcc/source_location.h"

namespace jcc {

class CodeGen;

class ASTNode {
  SourceRange loc_;

 protected:
  explicit ASTNode(SourceRange loc) : loc_(std::move(loc)) {}

 public:
  virtual ~ASTNode();
  virtual void dump(int indent) const = 0;
  virtual void GenCode(CodeGen& gen) = 0;

  template <typename Ty>
  Ty* As() {
    return dynamic_cast<Ty*>(this);
  }
};
}  // namespace jcc
