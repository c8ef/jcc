#include "jcc/parser.h"

#include "jcc/common.h"
#include "jcc/decl.h"
#include "jcc/expr.h"
#include "jcc/lexer.h"
#include "jcc/source_location.h"
#include "jcc/token.h"
#include "jcc/type.h"

static BinOpPreLevel GetBinOpPrecedence(TokenKind Kind) {
  switch (Kind) {
    default:
    case TokenKind::Greater:
      return BinOpPreLevel::Unknown;
    case TokenKind::Comma:
      return BinOpPreLevel::Comma;
    case TokenKind::Equal:
    case TokenKind::StarEqual:
    case TokenKind::SlashEqual:
    case TokenKind::PercentEqual:
    case TokenKind::PlusEqual:
    case TokenKind::MinusEqual:
    case TokenKind::LeftShiftEqual:
    case TokenKind::RightShiftEqual:
    case TokenKind::AmpersandEqual:
    case TokenKind::CarretEqual:
    case TokenKind::PipeEqual:
      return BinOpPreLevel::Assignment;
    case TokenKind::Question:
      return BinOpPreLevel::Conditional;
    case TokenKind::PipePipe:
      return BinOpPreLevel::LogicalOr;
    // case TokenKind::CarretCarret:
    case TokenKind::AmpersandAmpersand:
      return BinOpPreLevel::LogicalAnd;
    case TokenKind::Pipe:
      return BinOpPreLevel::InclusiveOr;
    case TokenKind::Carret:
      return BinOpPreLevel::ExclusiveOr;
    case TokenKind::Ampersand:
      return BinOpPreLevel::And;
    case TokenKind::EqualEqual:
      return BinOpPreLevel::Equality;
    case TokenKind::LessEqual:
    case TokenKind::Less:
    case TokenKind::GreaterEqual:
      return BinOpPreLevel::Relational;
    case TokenKind::LeftShift:
      return BinOpPreLevel::Shift;
    case TokenKind::Plus:
    case TokenKind::Minus:
      return BinOpPreLevel::Additive;
    case TokenKind::Percent:
    case TokenKind::Slash:
    case TokenKind::Star:
      return BinOpPreLevel::Multiplicative;
  }
}

Parser::Parser(Lexer& lexer) : lexer_(lexer) { token_ = lexer_.Lex(); }

Token Parser::CurrentToken() { return token_; }

Token Parser::ConsumeToken() {
  if (cache_) {
    token_ = *cache_;
    cache_ = std::nullopt;
  } else {
    token_ = lexer_.Lex();
  }
  return token_;
}

void Parser::MustConsumeToken(TokenKind expected) {
  if (CurrentToken().GetKind() == expected) {
    ConsumeToken();
    return;
  }
  jcc_unreachable();
}

Token Parser::NextToken() {
  Token next_tok = lexer_.Lex();
  cache_ = next_tok;
  return next_tok;
}

bool Parser::TryConsumeToken(TokenKind expected) {
  if (CurrentToken().GetKind() == expected) {
    ConsumeToken();
    return true;
  }
  return false;
}

void Parser::SkipUntil(TokenKind kind, bool skip_match) {
  while (CurrentToken().GetKind() != kind) {
    ConsumeToken();
  }
  if (skip_match && CurrentToken().GetKind() == kind) {
    ConsumeToken();
  }
}

Type* Parser::ParseTypename() {
  DeclSpec declSpec = ParseDeclSpec();
  return nullptr;
}

DeclSpec Parser::ParseDeclSpec() {
  using enum TokenKind;
  DeclSpec decl_spec(GetASTContext());
  while (CurrentToken().IsTypename()) {
    if (CurrentToken()
            .IsOneOf<Typedef, Static, Extern, Inline, DashThreadLocal>()) {
      // Check if storage class specifier is allowed in this context.
      switch (CurrentToken().GetKind()) {
        case Typedef:
          decl_spec.SetStorageClassSpec(DeclSpec::StorageClassSpec::Typedef);
          break;
        case Static:
          decl_spec.SetStorageClassSpec(DeclSpec::StorageClassSpec::Static);
          break;
        case Extern:
          decl_spec.SetStorageClassSpec(DeclSpec::StorageClassSpec::Extern);
          break;
        case Inline:
          decl_spec.SetFunctionSpec(DeclSpec::FunctionSpec::Inline);
          break;
        case DashThreadLocal:
          decl_spec.SetStorageClassSpec(
              DeclSpec::StorageClassSpec::ThreadLocal);
          break;
        default:
          jcc_unreachable();
      }

      if (decl_spec.IsTypedef()) {
        if (decl_spec.IsStatic() || decl_spec.IsExtern() ||
            decl_spec.IsInline() || decl_spec.IsThreadLocal()) {
          // TODO(Jun): Can you have a nice diag instead of panic?
          jcc_unreachable();
        }
      }
      ConsumeToken();
    }

    // Ignore some keywords with no effects like `auto`
    if (CurrentToken()
            .IsOneOf<Const, Auto, Volatile, Register, Restrict,
                     DashNoReturn>()) {
      ConsumeToken();
    }

    // Deal with _Atomic
    if (CurrentToken().IsOneOf<DashAtmoic>()) {
      ConsumeToken();  // eat `(`
      decl_spec.SetType(ParseTypename());
      ConsumeToken();  // eat `)`
      decl_spec.SetTypeQual(DeclSpec::TypeQual::Atomic);
    }

    // Deal with _Alignas
    if (CurrentToken().Is<TokenKind::DashAlignas>()) {
      jcc_unreachable();
    }

    // Handle user defined types
    if (CurrentToken().IsOneOf<Struct, Union, Typedef, Enum>()) {
      jcc_unreachable();
    }

    // Handle builtin types

    switch (CurrentToken().GetKind()) {
      case Void:
        decl_spec.SetTypeSpecKind(DeclSpec::TSK_Void);
        break;
      case DashBool:
        decl_spec.SetTypeSpecKind(DeclSpec::TSK_Bool);
        break;
      case Char:
        decl_spec.SetTypeSpecKind(DeclSpec::TSK_Char);
        break;
      case Short:
        decl_spec.setTypeSpecWidth(DeclSpec::TypeSpecWidth::Short);
        break;
      case Int:
        decl_spec.SetTypeSpecKind(DeclSpec::TSK_Int);
        break;
      case Long:
        if (decl_spec.GetTypeSpecWidth() == DeclSpec::TypeSpecWidth::Long) {
          decl_spec.setTypeSpecWidth(DeclSpec::TypeSpecWidth::LongLong);
        } else {
          decl_spec.setTypeSpecWidth(DeclSpec::TypeSpecWidth::Long);
        }
        break;
      case Float:
        decl_spec.SetTypeSpecKind(DeclSpec::TSK_Float);
        break;
      case Double:
        decl_spec.SetTypeSpecKind(DeclSpec::TSK_Double);
        break;
      case Signed:
        decl_spec.setTypeSpecSign(DeclSpec::TypeSpecSign::Signed);
        break;
      case Unsigned:
        decl_spec.setTypeSpecSign(DeclSpec::TypeSpecSign::Unsigned);
        break;
      default:
        jcc_unreachable();
    }

    decl_spec.GenerateType();
    ConsumeToken();
  }

  return decl_spec;
}

Type* Parser::ParsePointers(Declarator& declarator) {
  using enum TokenKind;
  Type* type = declarator.GetBaseType();
  while (TryConsumeToken(Star)) {
    type = Type::CreatePointerType(GetASTContext(), type);
    while (CurrentToken().IsOneOf<Const, Volatile, Restrict>()) {
      ConsumeToken();
    }
  }
  return type;
}

Type* Parser::ParseTypeSuffix(Type* type) {
  if (TryConsumeToken(TokenKind::LeftParen)) {
    return ParseParams(type);
  }
  if (TryConsumeToken(TokenKind::LeftSquare)) {
    return ParseArrayDimensions(type);
  }
  return type;
}

Declarator Parser::ParseDeclarator(DeclSpec& decl_spec) {
  Declarator declarator(decl_spec);
  Type* type = ParsePointers(declarator);
  if (TryConsumeToken(TokenKind::LeftParen)) {
    DeclSpec dummy(GetASTContext());
    ParseDeclarator(dummy);
    ConsumeToken();  // Eat ')'
    Type* suffixType = ParseTypeSuffix(type);
    DeclSpec suffix(GetASTContext());
    suffix.SetType(suffixType);
    return ParseDeclarator(suffix);
  }

  Token name;
  if (CurrentToken().Is<TokenKind::Identifier>()) {
    name = CurrentToken();
    ConsumeToken();
  }
  // FIXME: Looks like we'll gonna screw up here if the token is not an
  // identifier.
  Type* suffixType = ParseTypeSuffix(type);
  declarator.name_ = name;
  declarator.SetType(suffixType);
  return declarator;
}

Type* Parser::ParseParams(Type* type) {
  if (CurrentToken().Is<TokenKind::Void>() &&
      NextToken().Is<TokenKind::RightParen>()) {
    SkipUntil(TokenKind::RightParen, /*skip_match=*/true);
    return Type::CreateFuncType(GetASTContext(), type);
  }

  std::vector<Type*> params;
  while (!CurrentToken().Is<TokenKind::RightParen>()) {
    Type* type;
    DeclSpec decl_spec = ParseDeclSpec();
    Declarator declarator = ParseDeclarator(decl_spec);

    if (declarator.GetTypeKind() == TypeKind::Array) {
      type = Type::CreatePointerType(
          GetASTContext(),
          declarator.GetBaseType()->AsType<PointerType>()->GetBase());
      // FIXME: set name to type.
    } else if (declarator.GetTypeKind() == TypeKind::Func) {
      type = Type::CreatePointerType(GetASTContext(), declarator.GetBaseType());
    }
    params.push_back(type);
  }

  Type* function_type = Type::CreateFuncType(GetASTContext(), type);
  function_type->AsType<FunctionType>()->SetParams(std::move(params));
  return function_type;
}

Type* Parser::ParseArrayDimensions(Type* type) {
  while (CurrentToken().IsOneOf<TokenKind::Static, TokenKind::Restrict>()) {
    ConsumeToken();
  }

  if (TryConsumeToken(TokenKind::RightParen)) {
    Type* arr_type = ParseTypeSuffix(type);
    return Type::CreateArrayType(GetASTContext(), arr_type, -1);
  }

  // cond ? A : B
  // vla
  jcc_unreachable();
}

Expr* Parser::ParseExpr() {
  Expr* expr = ParseAssignmentExpr();
  return ParseRhsOfBinaryExpr(expr, BinOpPreLevel::Assignment);
}

Stmt* Parser::ParseReturnStmt() {
  Expr* return_expr = nullptr;
  if (!TryConsumeToken(TokenKind::Semi)) {
    return_expr = ParseExpr();
    SkipUntil(TokenKind::Semi, /*skip_match=*/true);
  }
  return ReturnStatement::Create(GetASTContext(), SourceRange(), return_expr);
}
Stmt* Parser::ParseStatement() {
  if (TryConsumeToken(TokenKind::Return)) {
    return ParseReturnStmt();
  }

  if (TryConsumeToken(TokenKind::If)) {
    MustConsumeToken(TokenKind::LeftParen);
    Expr* condition = ParseExpr();
    Stmt* then = ParseStatement();
    Stmt* else_stmt = nullptr;
    if (CurrentToken().Is<TokenKind::Else>()) {
      else_stmt = ParseStatement();
    }
    return IfStatement::Create(GetASTContext(), SourceRange(), condition, then,
                               else_stmt);
  }
  jcc_unreachable();
}

std::vector<VarDecl*> Parser::CreateParams(FunctionType* type) {
  std::vector<VarDecl*> params;
  for (std::size_t idx = 0; idx < type->GetParamSize(); idx++) {
    Type* param_type = type->GetParamType(idx);
    // TODO(Jun): This doesn't work with parameters with names.
    params.push_back(VarDecl::Create(GetASTContext(), SourceRange(), nullptr,
                                     nullptr, param_type->GetName()));
  }
  return params;
}

Stmt* Parser::ParseCompoundStmt() {
  CompoundStatement* stmt =
      CompoundStatement::Create(GetASTContext(), SourceRange());
  ScopeRAII scope_guard(*this);  // FIXME: Create another scope, but it could be
                                 // a function body, so we create it twice?

  while (!CurrentToken().Is<TokenKind::RightBracket>()) {
    if (CurrentToken().IsTypename() && !NextToken().Is<TokenKind::Colon>()) {
      DeclSpec decl_spec = ParseDeclSpec();
      if (decl_spec.IsTypedef()) {
        // Parse Typedef
        jcc_unreachable();
        continue;
      }
      Declarator declarator = ParseDeclarator(decl_spec);
      if (declarator.GetTypeKind() == TypeKind::Func) {
        stmt->AddStmt(DeclStatement::Create(GetASTContext(), SourceRange(),
                                            ParseFunction(declarator)));
        continue;
      }
      stmt->AddStmt(DeclStatement::Create(GetASTContext(), SourceRange(),
                                          ParseGlobalVariables(declarator)));
    } else {
      stmt->AddStmt(ParseStatement());
    }
    // Add type?
  }
  ConsumeToken();  // Eat '}'
  return stmt;
}

Decl* Parser::ParseFunction(Declarator& declarator) {
  Token name = declarator.name_;
  if (!name.IsValid()) {
    jcc_unreachable();
  }
  std::string func_name = name.GetAsString();
  // Check redefinition
  if (Decl* func = Lookup(func_name)) {
    // FIXME: This is not correct!
    jcc_unreachable();
  }

  auto* self = declarator.GetBaseType()->AsType<FunctionType>();

  FunctionDecl* function = FunctionDecl::Create(
      GetASTContext(), SourceRange(), func_name, self->GetReturnType());

  ScopeRAII scope_guard(*this);

  function->SetParams(CreateParams(self));

  if (TryConsumeToken(TokenKind::LeftBracket)) {
    function->SetBody(ParseCompoundStmt());
  } else if (CurrentToken().Is<TokenKind::Semi>()) {
    // this function doesn't have a body, nothing to do.
  } else {
    jcc_unreachable();
  }

  return function;
}

std::vector<Decl*> Parser::ParseGlobalVariables(Declarator& declarator) {
  std::vector<Decl*> vars;
  bool is_first = true;
  while (!CurrentToken().Is<TokenKind::Semi>()) {
    if (!is_first) {
      SkipUntil(TokenKind::Comma);
    }
    is_first = false;

    // FIXME: Just a note, we need to check its redefinition.
    VarDecl* var =
        VarDecl::Create(GetASTContext(), SourceRange(), nullptr,
                        declarator.GetBaseType(), declarator.GetName());
    if (TryConsumeToken(TokenKind::Equal)) {
      var->SetInit(ParseAssignmentExpr());
    }
    vars.push_back(var);
  }
  MustConsumeToken(TokenKind::Semi);  // Eat ';'
  return vars;
}

Expr* Parser::ParseAssignmentExpr() {
  Expr* lhs = ParseCastExpr();

  return ParseRhsOfBinaryExpr(lhs, BinOpPreLevel::Assignment);
}

Expr* Parser::ParseCastExpr() {
  TokenKind kind = CurrentToken().GetKind();
  Expr* result;

  // TODO(Jun): It doesn't really parse a cast expression for now.
  // TO do that, we need a flag to indicate the parsing kind, like:
  // UnaryExpr, CastExpr, PrimaryExpr...
  switch (kind) {
    case TokenKind::NumericConstant: {
      int value = std::stoi(CurrentToken().GetData());
      ConsumeToken();
      result = IntergerLiteral::Create(GetASTContext(), SourceRange(), value);
      break;
    }
    case TokenKind::StringLiteral: {
      result = StringLiteral::Create(GetASTContext(), SourceRange(),
                                     CurrentToken().GetAsString());
      ConsumeToken();
      break;
    }
    default:
      jcc_unreachable();
  }
  return result;
}

static BinaryOperatorKind ConvertOpToKind(TokenKind kind) {
  switch (kind) {
    case TokenKind::Plus:
      return BinaryOperatorKind::Plus;
    case TokenKind::Minus:
      return BinaryOperatorKind::Minus;
    case TokenKind::Star:
      return BinaryOperatorKind::Multiply;
    case TokenKind::Slash:
      return BinaryOperatorKind::Divide;
    case TokenKind::Greater:
      return BinaryOperatorKind::Greater;
    case TokenKind::Less:
      return BinaryOperatorKind::Less;
    default:
      jcc_unreachable();
  }
}

Expr* Parser::ParseRhsOfBinaryExpr(Expr* lhs, BinOpPreLevel min_prec) {
  BinOpPreLevel next_tok_prec = GetBinOpPrecedence(CurrentToken().GetKind());
  while (true) {
    if (next_tok_prec < min_prec) {
      return lhs;
    }
    Token op_tok = CurrentToken();  // Save the operator.
    ConsumeToken();
    // FIXME: deal with cond ? a : b
    Expr* rhs;
    rhs = ParseCastExpr();

    BinOpPreLevel this_tok_prec = next_tok_prec;  // Save this.
    next_tok_prec = GetBinOpPrecedence(CurrentToken().GetKind());

    bool is_right_assoc = this_tok_prec == BinOpPreLevel::Conditional ||
                          this_tok_prec == BinOpPreLevel::Assignment;

    if (this_tok_prec < next_tok_prec ||
        (this_tok_prec == next_tok_prec && is_right_assoc)) {
      rhs =
          ParseRhsOfBinaryExpr(rhs, static_cast<BinOpPreLevel>(this_tok_prec));
      next_tok_prec = GetBinOpPrecedence(CurrentToken().GetKind());
    }

    Expr* origin_lhs = lhs;

    Expr* binary_expr =
        BinaryExpr::Create(GetASTContext(), SourceRange(),
                           ConvertOpToKind(op_tok.GetKind()), lhs, rhs);
    lhs = binary_expr;
  }
  return lhs;
}

// Function or a simple declaration
std::vector<Decl*> Parser::ParseFunctionOrVar(DeclSpec& decl_spec) {
  std::vector<Decl*> decls;
  if (CurrentToken().Is<TokenKind::Semi>()) {
    jcc_unreachable();
  }

  Declarator declarator = ParseDeclarator(decl_spec);
  if (declarator.GetTypeKind() == TypeKind::Func) {
    decls.push_back(ParseFunction(declarator));
  } else {
    std::vector<Decl*> vars = ParseGlobalVariables(declarator);
    decls.insert(decls.end(), vars.begin(), vars.end());
  }
  return decls;
}

std::vector<Decl*> Parser::ParseTranslateUnit() {
  ScopeRAII scope_guard(*this);  // The file scope.

  std::vector<Decl*> top_decls;
  while (!CurrentToken().Is<TokenKind::Eof>()) {
    DeclSpec decl_spec = ParseDeclSpec();
    // TODO(Jun): Handle typedefs
    if (decl_spec.IsTypedef()) {
      jcc_unreachable();
    }

    std::vector<Decl*> decls = ParseFunctionOrVar(decl_spec);
    top_decls.insert(top_decls.end(), decls.begin(), decls.end());
  }

  return top_decls;
}
