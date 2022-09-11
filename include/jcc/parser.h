#pragma once

#include <memory>
#include <optional>
#include <unordered_map>

#include "jcc/ast_context.h"
#include "jcc/decl.h"
#include "jcc/declarator.h"
#include "jcc/lexer.h"
#include "jcc/token.h"

class Parser;

struct Scope {
  explicit Scope(Parser& parser) : self(parser) {}
  Parser& self;
  std::unordered_map<std::string, std::string> D;
};

class Parser {
 public:
  explicit Parser(Lexer& lexer) : lexer_(lexer) {}

  std::vector<Decl*> parseTranslateUnit();

  DeclSpec parseDeclSpec();

  Declarator parseDeclarator(DeclSpec& declSpec);

  std::vector<Decl*> parseFunctionOrVar(DeclSpec& declSpec);

  std::vector<Decl*> parseGlobalVariables(DeclSpec& declSpec);

  std::vector<Decl*> parseDeclarations(DeclSpec& declSpec);

  Stmt* parseStatement();

  Decl* parseFunction(DeclSpec& declSpec);

  Stmt* parseFunctionBody();

  std::unique_ptr<Type> parseParams(std::unique_ptr<Type> type);

  std::unique_ptr<Type> parseArrayDimensions(std::unique_ptr<Type> type);

  std::unique_ptr<Type> parseTypeSuffix(std::unique_ptr<Type> type);

  std::unique_ptr<Type> parsePointers(Declarator& declrator);

  std::unique_ptr<Type> parseTypename();

  std::vector<VarDecl*> createParams(FunctionType* type);

  ASTContext& getASTContext() { return ctx_; }

 private:
  Token currentToken();
  void consumeToken();
  bool tryConsumeToken(TokenKind expected);
  Token nextToken();
  Lexer& lexer_;
  Token token_;
  std::optional<Token> cache_;
  ASTContext ctx_;
  std::vector<Scope> scopes;

  void enterScope();
  void exitScope();

  class ScopeRAII {
    Parser& self_;

   public:
    explicit ScopeRAII(Parser& self) : self_(self) { self_.enterScope(); }
    ~ScopeRAII() { self_.exitScope(); }
  };
};
