#pragma once

#include <concepts>
#include <memory>

#include "jcc/stmt.h"

class Type;
class Decl;
class ASTContext;

// C11 6.5.1
// An expression is a sequence of operators and operands that specifies
// computation of a value, or that designates an object or a function, or that
// generates side effects, or that performs a combination thereof.
class Expr : public Stmt {
  std::unique_ptr<Type> type_{nullptr};

 protected:
  explicit Expr(SourceRange loc) : Stmt(std::move(loc)) {}

 public:
  static Expr* create(ASTContext& ctx, SourceRange loc);

  template <typename Ty>
  requires std::convertible_to<Ty, Expr> Ty* asExpr() {
    return static_cast<Ty*>(this);
  }

  Type* getType() { return type_.get(); }

  // TODO(Jun): What's the signature?
  void setType() {}

  virtual ~Expr() = default;
};

class StringLiteral : public Expr {
  std::string literal_;

  StringLiteral(SourceRange loc, const char* literal)
      : Expr(std::move(loc)), literal_(literal) {}

 public:
  static StringLiteral* create(ASTContext& ctx, SourceRange loc,
                               const char* literal);
};

class CharacterLiteral : public Expr {
  // TODO(Jun): Support more character kinds.
  char value_{0};

  CharacterLiteral(SourceRange loc, char value)
      : Expr(std::move(loc)), value_(value) {}

 public:
  static CharacterLiteral* create(ASTContext& ctx, SourceRange loc, char value);

  [[nodiscard]] char getValue() const { return value_; }
};

class IntergerLiteral : public Expr {
  int value_{0};

  IntergerLiteral(SourceRange loc, int value)
      : Expr(std::move(loc)), value_(value) {}

 public:
  static IntergerLiteral* create(ASTContext& ctx, SourceRange loc, int value);

  [[nodiscard]] int getValue() const { return value_; }
};

class FloatingLiteral : public Expr {
  double value_{0};

  FloatingLiteral(SourceRange loc, double value)
      : Expr(std::move(loc)), value_(value) {}

 public:
  static FloatingLiteral* create(ASTContext& ctx, SourceRange loc,
                                 double value);

  [[nodiscard]] double getValue() const { return value_; };
};

class ConstantExpr : public Expr {
  // C Standard says we need it.
};

class CallExpr : public Expr {
  Expr* callee_{nullptr};
  std::vector<Expr*> args_;

  CallExpr(SourceRange loc, Expr* callee, std::vector<Expr*> args)
      : Expr(std::move(loc)), callee_(callee), args_(std::move(args)) {}

 public:
  static CallExpr* create(ASTContext& ctx, SourceRange loc, Expr* callee,
                          std::vector<Expr*> args);

  Expr* getCallee() { return callee_; }

  Expr* getArg(std::size_t index) { return args_[index]; }

  [[nodiscard]] std::size_t getArgNum() const { return args_.size(); }
};

class CastExpr : public Expr {
  // TODO(Jun): Implement this.
};

class InitListExpr : public Expr {
  std::vector<Stmt*> initExprs_;

  explicit InitListExpr(SourceRange loc, std::vector<Stmt*> initExprs)
      : Expr(std::move(loc)), initExprs_(std::move(initExprs)) {}

 public:
  static InitListExpr* create(ASTContext& ctx, SourceRange loc,
                              std::vector<Stmt*> initExprs);
};

// TODO(Jun): Add more kinds.
enum class UnaryOperatorKind {
  PreIncrement,
  PostIncrement,
};

class UnaryExpr : public Expr {
  UnaryOperatorKind kind_;
  Stmt* value_{nullptr};

  UnaryExpr(SourceRange loc, UnaryOperatorKind kind, Stmt* value)
      : Expr(std::move(loc)), kind_(kind), value_(value) {}

 public:
  static UnaryExpr* create(ASTContext& ctx, SourceRange loc,
                           UnaryOperatorKind kind, Stmt* value);

  [[nodiscard]] UnaryOperatorKind getKind() const { return kind_; }
};

// TODO(Jun): Add more kinds.
enum class BinaryOperatorKind {
  Plus,
  Minus,
  Multiply,
  Divide,
  Greater,
  Less,
};

class BinaryExpr : public Expr {
  BinaryOperatorKind kind_;
  Expr* lhs_{nullptr};
  Expr* rhs_{nullptr};

  BinaryExpr(SourceRange loc, BinaryOperatorKind kind, Expr* lhs, Expr* rhs)
      : Expr(std::move(loc)), kind_(kind), lhs_(lhs), rhs_(rhs) {}

 public:
  static BinaryExpr* create(ASTContext& ctx, SourceRange loc,
                            BinaryOperatorKind kind, Expr* lhs, Expr* rhs);

  [[nodiscard]] BinaryOperatorKind getKind() const { return kind_; }

  Expr* getLhs() { return lhs_; }

  Expr* getRhs() { return rhs_; }
};

class ArraySubscriptExpr : public Expr {
  Expr* lhs_{nullptr};
  Expr* rhs_{nullptr};

  ArraySubscriptExpr(SourceRange loc, Expr* lhs, Expr* rhs)
      : Expr(std::move(loc)), lhs_(lhs), rhs_(rhs) {}

 public:
  static ArraySubscriptExpr* create(ASTContext& ctx, SourceRange loc, Expr* lhs,
                                    Expr* rhs);

  Expr* getLhs() { return lhs_; }

  Expr* getRhs() { return rhs_; }
};

class MemberExpr : public Expr {
  Stmt* base_{nullptr};
  Decl* member_{nullptr};

  MemberExpr(SourceRange loc, Stmt* base, Decl* member)
      : Expr(std::move(loc)), base_(base), member_(member) {}

 public:
  static MemberExpr* create(ASTContext& ctx, SourceRange loc, Stmt* base,
                            Decl* member);

  Stmt* getBase() { return base_; }

  Decl* getMember() { return member_; }
};

class DeclRefExpr : public Expr {
  Decl* decl_{nullptr};

  DeclRefExpr(SourceRange loc, Decl* decl)
      : Expr(std::move(loc)), decl_(decl) {}

 public:
  static DeclRefExpr* create(ASTContext& ctx, SourceRange loc, Decl* decl);

  Decl* getRefDecl() { return decl_; }
};
