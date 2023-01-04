#pragma once

#include <memory>

#include "jcc/source_location.h"
#include "jcc/stmt.h"

namespace jcc {

class Type;
class Decl;
class ASTContext;

// C11 6.5.1
// An expression is a sequence of operators and operands that specifies
// computation of a value, or that designates an object or a function, or that
// generates side effects, or that performs a combination thereof.
class Expr : public Stmt {
  Type* type_ = nullptr;

 protected:
  explicit Expr(SourceRange loc, Type* type)
      : Stmt(std::move(loc)), type_(type) {}

 public:
  Type* GetType() {
    assert(type_ && "Expr's type can't be null!");
    return type_;
  }

  void SetType(Type* type) { type_ = type; }

  ~Expr() override;
};

class StringLiteral : public Expr {
  std::string literal_;

  StringLiteral(SourceRange loc, Type* type, std::string literal)
      : Expr(std::move(loc), type), literal_(std::move(literal)) {}

 public:
  static StringLiteral* Create(ASTContext& ctx, SourceRange loc,
                               std::string literal);

  [[nodiscard]] std::string GetValue() const { return literal_; }
  void dump(int indent) const override;

  void GenCode(CodeGen& gen) override;
};

class CharacterLiteral : public Expr {
  // TODO(Jun): Support more character kinds.
  std::string value_;

  CharacterLiteral(SourceRange loc, Type* type, std::string value)
      : Expr(std::move(loc), type), value_(std::move(value)) {}

 public:
  static CharacterLiteral* Create(ASTContext& ctx, SourceRange loc, Type* type,
                                  std::string value);

  [[nodiscard]] std::string GetValue() const { return value_; }

  void dump(int indent) const override;

  void GenCode(CodeGen& gen) override;
};

class IntergerLiteral : public Expr {
  int value_{0};

  IntergerLiteral(SourceRange loc, Type* type, int value)
      : Expr(std::move(loc), type), value_(value) {}

 public:
  static IntergerLiteral* Create(ASTContext& ctx, SourceRange loc, Type* type,
                                 int value);

  [[nodiscard]] int GetValue() const { return value_; }

  void dump(int indent) const override;

  void GenCode(CodeGen& gen) override;
};

class FloatingLiteral : public Expr {
  double value_{0};

  FloatingLiteral(SourceRange loc, Type* type, double value)
      : Expr(std::move(loc), type), value_(value) {}

 public:
  static FloatingLiteral* Create(ASTContext& ctx, SourceRange loc, Type* type,
                                 double value);

  [[nodiscard]] double GetValue() const { return value_; }

  void dump(int indent) const override;

  void GenCode(CodeGen& gen) override;
};

class ConstantExpr : public Expr {
  // C Standard says we need it.
};

class CallExpr : public Expr {
  Expr* callee_ = nullptr;
  std::vector<Expr*> args_;

  CallExpr(SourceRange loc, Type* type, Expr* callee, std::vector<Expr*> args)
      : Expr(std::move(loc), type), callee_(callee), args_(std::move(args)) {}

 public:
  static CallExpr* Create(ASTContext& ctx, SourceRange loc, Type* type,
                          Expr* callee, std::vector<Expr*> args);

  [[nodiscard]] Expr* GetCallee() const { return callee_; }

  Expr* GetArg(std::size_t index) { return args_[index]; }

  [[nodiscard]] std::size_t GetArgNum() const { return args_.size(); }

  void dump(int indent) const override;

  void GenCode(CodeGen& gen) override;
};

class CastExpr : public Expr {
  // TODO(Jun): Implement this.
};

// TODO(Jun): Add more kinds.
enum class UnaryOperatorKind {
  PreIncrement,
  PreDecrement,
  PostIncrement,
  PostDecrement,
  AddressOf,
  Deref,
  Plus,
  Minus
};

class UnaryExpr : public Expr {
  UnaryOperatorKind kind_;
  Stmt* value_ = nullptr;

  UnaryExpr(SourceRange loc, Type* type, UnaryOperatorKind kind, Stmt* value)
      : Expr(std::move(loc), type), kind_(kind), value_(value) {}

 public:
  static UnaryExpr* Create(ASTContext& ctx, SourceRange loc, Type* type,
                           UnaryOperatorKind kind, Stmt* value);

  [[nodiscard]] UnaryOperatorKind getKind() const { return kind_; }

  Stmt* GetValue() { return value_; }

  void dump(int indent) const override;

  void GenCode(CodeGen& gen) override;
};

// TODO(Jun): Add more kinds.
enum class BinaryOperatorKind {
  Plus,
  Minus,
  Multiply,
  Divide,
  Greater,
  Less,
  Equal,
  PlusEqual
};

class BinaryExpr : public Expr {
  BinaryOperatorKind kind_;
  Expr* lhs_ = nullptr;
  Expr* rhs_ = nullptr;

  BinaryExpr(SourceRange loc, Type* type, BinaryOperatorKind kind, Expr* lhs,
             Expr* rhs)
      : Expr(std::move(loc), type), kind_(kind), lhs_(lhs), rhs_(rhs) {}

 public:
  static BinaryExpr* Create(ASTContext& ctx, SourceRange loc, Type* type,
                            BinaryOperatorKind kind, Expr* lhs, Expr* rhs);

  [[nodiscard]] BinaryOperatorKind GetKind() const { return kind_; }

  Expr* GetLhs() { return lhs_; }

  Expr* GetRhs() { return rhs_; }

  void dump(int indent) const override;

  void GenCode(CodeGen& gen) override;
};

class MemberExpr : public Expr {
  Stmt* base_{nullptr};
  Decl* member_{nullptr};

  MemberExpr(SourceRange loc, Type* type, Stmt* base, Decl* member)
      : Expr(std::move(loc), type), base_(base), member_(member) {}

 public:
  static MemberExpr* create(ASTContext& ctx, SourceRange loc, Stmt* base,
                            Decl* member);

  Stmt* getBase() { return base_; }

  Decl* getMember() { return member_; }

  void dump(int indent) const override;

  void GenCode(CodeGen& gen) override;
};

class DeclRefExpr : public Expr {
  Decl* decl_ = nullptr;

  DeclRefExpr(SourceRange loc, Type* type, Decl* decl)
      : Expr(std::move(loc), type), decl_(decl) {}

 public:
  static DeclRefExpr* Create(ASTContext& ctx, SourceRange loc, Type* type,
                             Decl* decl);

  Decl* GetRefDecl() { return decl_; }

  void dump(int indent) const override;

  void GenCode(CodeGen& gen) override;
};
}  // namespace jcc
