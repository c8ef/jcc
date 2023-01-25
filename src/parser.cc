#include "jcc/parser.h"

#include <algorithm>

#include "jcc/common.h"
#include "jcc/decl.h"
#include "jcc/declarator.h"
#include "jcc/expr.h"
#include "jcc/lexer.h"
#include "jcc/source_location.h"
#include "jcc/stmt.h"
#include "jcc/token.h"
#include "jcc/type.h"

namespace jcc {

static BinOpPreLevel GetBinOpPrecedence(TokenKind kind) {
  switch (kind) {
    default:
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
    case TokenKind::Greater:
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
  jcc_unreachable("MustConsumeToken() consumed unexpeted token!");
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

std::vector<Type*> Parser::ParseMembers() {
  std::vector<Type*> members;
  while (true) {
    DeclSpec decl_spec = ParseDeclSpec();
    Declarator declarator = ParseDeclarator(decl_spec);

    members.push_back(declarator.GetType());
    MustConsumeToken(TokenKind::Semi);
    if (TryConsumeToken(TokenKind::RightBracket)) {
      break;
    }
  }
  return members;
}

Type* Parser::ParseRecordType(TokenKind kind) {
  TypeKind type_kind;
  if (kind == TokenKind::Struct) {
    type_kind = TypeKind::Struct;
  } else if (kind == TokenKind::Union) {
    type_kind = TypeKind::Union;
  } else {
    jcc_unreachable("Can only parse struct or union here!");
  }

  Type* type = Type::CreateRecordType(GetASTContext(), type_kind);
  if (CurrentToken().Is<TokenKind::Identifier>()) {
    type->SetName(CurrentToken());
    ConsumeToken();
  }

  if (TryConsumeToken(TokenKind::LeftBracket)) {
    type->AsType<RecordType>()->SetMembers(ParseMembers());
  }

  return type;
}

Type* Parser::ParseTypename() {
  DeclSpec decl_spec = ParseDeclSpec();
  Declarator declarator = ParseDeclarator(decl_spec);
  return declarator.GetType();
}

DeclSpec Parser::ParseDeclSpec() {
  using enum TokenKind;
  DeclSpec decl_spec(GetASTContext());
  while (IsType(CurrentToken())) {
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
          jcc_unreachable("current token is not a typename!");
      }

      if (decl_spec.IsTypedef()) {
        if (decl_spec.IsStatic() || decl_spec.IsExtern() ||
            decl_spec.IsInline() || decl_spec.IsThreadLocal()) {
          // TODO(Jun): Can you have a nice diag instead of panic?
          jcc_unreachable(
              "typedef may not be used together with static, extern, inline, "
              "__thread or _Thread_local");
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
      jcc_unimplemented();
    }

    if (CurrentToken().IsOneOf<Struct, Union>()) {
      TokenKind cur_tok_kind = CurrentToken().GetKind();
      ConsumeToken();
      decl_spec.SetType(ParseRecordType(cur_tok_kind));
      return decl_spec;
    }
    // Handle user defined types
    if (CurrentToken().IsOneOf<Typedef, Enum>()) {
      jcc_unimplemented();
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
        decl_spec.SetTypeSpecWidth(DeclSpec::TypeSpecWidth::Short);
        break;
      case Int:
        decl_spec.SetTypeSpecKind(DeclSpec::TSK_Int);
        break;
      case Long:
        if (decl_spec.GetTypeSpecWidth() == DeclSpec::TypeSpecWidth::Long) {
          decl_spec.SetTypeSpecWidth(DeclSpec::TypeSpecWidth::LongLong);
        } else {
          decl_spec.SetTypeSpecWidth(DeclSpec::TypeSpecWidth::Long);
        }
        break;
      case Float:
        decl_spec.SetTypeSpecKind(DeclSpec::TSK_Float);
        break;
      case Double:
        decl_spec.SetTypeSpecKind(DeclSpec::TSK_Double);
        break;
      case Signed:
        decl_spec.SetTypeSpecSign(DeclSpec::TypeSpecSign::Signed);
        break;
      case Unsigned:
        decl_spec.SetTypeSpecSign(DeclSpec::TypeSpecSign::Unsigned);
        break;
      case Identifier: {
        // When running into an identifier, presumably it's type alias or user
        // defined type. Thus look up it in the scope, and set it for DeclSpec.
        decl_spec.SetType(LookupType(CurrentToken().GetAsString()));
        break;
      }
      default:
        jcc_unreachable("current token kind is not a type");
    }

    ConsumeToken();
    decl_spec.SynthesizeType();
  }

  return decl_spec;
}

Type* Parser::ParsePointers(Declarator& declarator) {
  using enum TokenKind;
  Type* type = declarator.GetType();
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
    Type* suffix_type = ParseTypeSuffix(type);
    DeclSpec suffix(GetASTContext());
    suffix.SetType(suffix_type);
    return ParseDeclarator(suffix);
  }

  // FIXME: Looks like we'll gonna screw up here if the token is not an
  // identifier.
  Token name;
  if (CurrentToken().Is<TokenKind::Identifier>()) {
    name = CurrentToken();
    ConsumeToken();
    // Keep the token and set it later, or it will be flushed away.
    declarator.SetType(ParseTypeSuffix(type));
    declarator.SetName(name);
  }
  return declarator;
}

// FIXME: Avoid the code ducplication.
Declarator Parser::ParseAbstractDeclarator(DeclSpec& decl_spec) {
  Declarator declarator(decl_spec);
  Type* type = ParsePointers(declarator);
  if (TryConsumeToken(TokenKind::LeftParen)) {
    DeclSpec dummy(GetASTContext());
    ParseDeclarator(dummy);
    ConsumeToken();  // Eat ')'
    Type* suffix_type = ParseTypeSuffix(type);
    DeclSpec suffix(GetASTContext());
    suffix.SetType(suffix_type);
    return ParseDeclarator(suffix);
  }

  declarator.SetType(ParseTypeSuffix(type));
  return declarator;
}

Type* Parser::ParseParams(Type* type) {
  Type* function_type = Type::CreateFuncType(GetASTContext(), type);
  // 1. int foo(void)
  // 2. int foo()
  if ((CurrentToken().Is<TokenKind::Void>() &&
       NextToken().Is<TokenKind::RightParen>()) ||
      CurrentToken().Is<TokenKind::RightParen>()) {
    SkipUntil(TokenKind::RightParen, /*skip_match=*/true);
    return function_type;
  }

  std::vector<Type*> params;
  while (true) {
    DeclSpec decl_spec = ParseDeclSpec();
    Declarator declarator = ParseDeclarator(decl_spec);
    Type* param_type = decl_spec.GetType();

    if (declarator.GetTypeKind() == TypeKind::Array) {
      param_type = Type::CreatePointerType(
          GetASTContext(),
          declarator.GetType()->AsType<PointerType>()->GetBase());
      // FIXME: set name to type.
    } else if (declarator.GetTypeKind() == TypeKind::Func) {
      param_type =
          Type::CreatePointerType(GetASTContext(), declarator.GetType());
    }

    params.push_back(param_type);

    if (TryConsumeToken(TokenKind::RightParen)) {
      break;
    }

    MustConsumeToken(TokenKind::Comma);
  }

  function_type->AsType<FunctionType>()->SetParams(std::move(params));
  return function_type;
}

Type* Parser::ParseArrayDimensions(Type* type) {
  while (CurrentToken().IsOneOf<TokenKind::Static, TokenKind::Restrict>()) {
    ConsumeToken();
  }

  if (TryConsumeToken(TokenKind::RightParen)) {
    Type* arr_type = ParseTypeSuffix(type);
    // FIXME: What is the length BTW?
    return Type::CreateArrayType(GetASTContext(), arr_type, 0);
  }

  // cond ? A : B
  // vla
  jcc_unimplemented();
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

Stmt* Parser::ParseIfStmt() {
  Stmt* else_stmt = nullptr;

  MustConsumeToken(TokenKind::LeftParen);
  Expr* condition = ParseExpr();
  MustConsumeToken(TokenKind::RightParen);

  Stmt* then = ParseStatement();

  if (TryConsumeToken(TokenKind::Else)) {
    else_stmt = ParseStatement();
  }

  return IfStatement::Create(GetASTContext(), SourceRange(), condition, then,
                             else_stmt);
}

Stmt* Parser::ParseDoStmt() {
  Stmt* body = ParseStatement();
  MustConsumeToken(TokenKind::While);
  MustConsumeToken(TokenKind::LeftParen);
  Expr* condition = ParseExpr();
  MustConsumeToken(TokenKind::RightParen);
  MustConsumeToken(TokenKind::Semi);
  return DoStatement::Create(GetASTContext(), SourceRange(), condition, body);
}

Stmt* Parser::ParseWhileStmt() {
  Stmt* body = nullptr;

  MustConsumeToken(TokenKind::LeftParen);
  Expr* condition = ParseExpr();
  MustConsumeToken(TokenKind::RightParen);

  body = ParseStatement();

  return WhileStatement::Create(GetASTContext(), SourceRange(), condition,
                                body);
}
Stmt* Parser::ParseSwitchStmt() {
  MustConsumeToken(TokenKind::LeftParen);
  Expr* condition = ParseExpr();
  MustConsumeToken(TokenKind::RightParen);
  MustConsumeToken(TokenKind::LeftBracket);
  auto* body = ParseCompoundStmt()->As<CompoundStatement>();
  return SwitchStatement::Create(GetASTContext(), SourceRange(), condition,
                                 body);
}

Stmt* Parser::ParseCaseStmt(bool is_default) {
  std::optional<std::string> value = std::nullopt;
  if (!is_default) {
    value = CurrentToken().GetAsString();
    ConsumeToken();
  }
  MustConsumeToken(TokenKind::Colon);
  Stmt* stmt = ParseStatement();
  return CaseStatement::Create(GetASTContext(), SourceRange(), stmt, value,
                               is_default);
}

Stmt* Parser::ParseBreakStmt() {
  MustConsumeToken(TokenKind::Semi);
  return BreakStatement::Create(GetASTContext(), SourceRange(), SourceRange());
}

Stmt* Parser::ParseContinueStmt() {
  MustConsumeToken(TokenKind::Semi);
  return ContinueStatement::Create(GetASTContext(), SourceRange(),
                                   SourceRange());
}

Stmt* Parser::ParseForStmt() {
  MustConsumeToken(TokenKind::LeftParen);
  Stmt* init = ParseStatement();
  Stmt* condition = ParseStatement();
  Stmt* increment = ParseExpr();
  MustConsumeToken(TokenKind::RightParen);

  MustConsumeToken(TokenKind::LeftBracket);
  Stmt* body = ParseCompoundStmt();

  return ForStatement::Create(GetASTContext(), SourceRange(), init, condition,
                              increment, body);
}

Stmt* Parser::ParseExprStmt() {
  if (TryConsumeToken(TokenKind::Semi)) {
    // TODO(Jun): We need an empty statement node.
    jcc_unimplemented();
  }

  Expr* expr = ParseExpr();
  MustConsumeToken(TokenKind::Semi);
  return ExprStatement::Create(GetASTContext(), SourceRange(), expr);
}

Stmt* Parser::ParseStatement() {
  if (TryConsumeToken(TokenKind::Return)) {
    return ParseReturnStmt();
  }

  if (TryConsumeToken(TokenKind::Break)) {
    return ParseBreakStmt();
  }

  if (TryConsumeToken(TokenKind::Continue)) {
    return ParseContinueStmt();
  }

  if (TryConsumeToken(TokenKind::If)) {
    return ParseIfStmt();
  }

  if (TryConsumeToken(TokenKind::While)) {
    return ParseWhileStmt();
  }

  if (TryConsumeToken(TokenKind::Switch)) {
    return ParseSwitchStmt();
  }

  if (TryConsumeToken(TokenKind::Case)) {
    return ParseCaseStmt();
  }

  if (TryConsumeToken(TokenKind::Default)) {
    return ParseCaseStmt(/*is_default=*/true);
  }

  if (TryConsumeToken(TokenKind::Do)) {
    return ParseDoStmt();
  }

  if (TryConsumeToken(TokenKind::Goto)) {
    jcc_unimplemented();
  }

  if (TryConsumeToken(TokenKind::For)) {
    return ParseForStmt();
  }

  if (TryConsumeToken(TokenKind::LeftBracket)) {
    return ParseCompoundStmt();
  }
  return ParseExprStmt();
}

std::vector<VarDecl*> Parser::CreateParams(FunctionType* type) {
  std::vector<VarDecl*> params;
  for (std::size_t idx = 0; idx < type->GetParamSize(); idx++) {
    Type* param_type = type->GetParamType(idx);
    // TODO(Jun): This doesn't work with parameters with names.
    params.push_back(VarDecl::Create(GetASTContext(), SourceRange(), nullptr,
                                     param_type,
                                     param_type->GetNameAsString()));
  }
  return params;
}

Stmt* Parser::ParseCompoundStmt() {
  CompoundStatement* stmt =
      CompoundStatement::Create(GetASTContext(), SourceRange());
  ScopeRAII scope_guard(*this);  // FIXME: Create another scope, but it could be
                                 // a function body, so we create it twice?

  while (!CurrentToken().Is<TokenKind::RightBracket>()) {
    if (IsType(CurrentToken()) && !NextToken().Is<TokenKind::Colon>()) {
      DeclSpec decl_spec = ParseDeclSpec();
      if (decl_spec.IsTypedef()) {
        // Parse Typedef
        ParseTypedef(decl_spec);
        continue;
      }
      Declarator declarator = ParseDeclarator(decl_spec);
      if (declarator.GetTypeKind() == TypeKind::Func) {
        stmt->AddStmt(DeclStatement::Create(GetASTContext(), SourceRange(),
                                            ParseFunction(declarator)));
        continue;
      }
      std::vector<Decl*> decls = ParseDeclaration(declarator);
      stmt->AddStmt(
          DeclStatement::Create(GetASTContext(), SourceRange(), decls));
      GetASTContext().GetCurFunc()->AddLocals(decls);
    } else {
      stmt->AddStmt(ParseStatement());
    }
    // Add type?
  }
  ConsumeToken();  // Eat '}'
  return stmt;
}

[[nodiscard]] bool Parser::IsType(Token token) const {
  // FIXME: the list seems to be not complete.
  switch (token.GetKind()) {
    case TokenKind::Auto:
    case TokenKind::Char:
    case TokenKind::Const:
    case TokenKind::Default:
    case TokenKind::Double:
    case TokenKind::Enum:
    case TokenKind::Extern:
    case TokenKind::Float:
    case TokenKind::Inline:
    case TokenKind::Int:
    case TokenKind::Long:
    case TokenKind::Register:
    case TokenKind::Restrict:
    case TokenKind::Short:
    case TokenKind::Signed:
    case TokenKind::Static:
    case TokenKind::Struct:
    case TokenKind::Typedef:
    case TokenKind::Union:
    case TokenKind::Unsigned:
    case TokenKind::Void:
    case TokenKind::Volatile:
    case TokenKind::DashAlignas:
    case TokenKind::DashAtmoic:
    case TokenKind::DashBool:
    case TokenKind::DashComplex:
    case TokenKind::DashNoReturn:
    case TokenKind::DashThreadLocal:
      return true;
    default:
      break;
  }
  return LookupType(token.GetAsString()) != nullptr;
}

Decl* Parser::ParseFunction(Declarator& declarator) {
  std::string func_name = declarator.GetName();
  if (func_name.empty()) {
    jcc_unreachable("function name is missing!");
  }
  // Check redefinition
  if (Decl* func = Lookup(func_name)) {
    // FIXME: This is not correct!
    jcc_unreachable("function redefinition!");
  }

  auto* func_type = declarator.GetType()->AsType<FunctionType>();

  FunctionDecl* function =
      FunctionDecl::Create(GetASTContext(), SourceRange(), func_name, func_type,
                           func_type->GetReturnType());
  GetASTContext().SetCurFunc(function);

  ScopeRAII scope_guard(*this);

  function->SetParams(CreateParams(func_type));

  if (TryConsumeToken(TokenKind::LeftBracket)) {
    function->SetBody(ParseCompoundStmt());
  } else if (CurrentToken().Is<TokenKind::Semi>()) {
    // this function doesn't have a body, nothing to do.
  } else {
    jcc_unreachable("error when parsing function body!");
  }

  return function;
}

// 1. int x;
// 2. int x = 0;
// 3. int x, y;
// 4. int x, y, z = 0;
std::vector<Decl*> Parser::ParseDeclaration(Declarator& declarator) {
  std::vector<Decl*> vars;

  vars.push_back(VarDecl::Create(GetASTContext(), SourceRange(), nullptr,
                                 declarator.GetType(), declarator.GetName()));
  // Parse optional decls.
  while (!CurrentToken().Is<TokenKind::Semi>() &&
         !CurrentToken().Is<TokenKind::Equal>()) {
    MustConsumeToken(TokenKind::Comma);

    if (CurrentToken().Is<TokenKind::Identifier>()) {
      vars.push_back(VarDecl::Create(GetASTContext(), SourceRange(), nullptr,
                                     declarator.GetType(),
                                     CurrentToken().GetAsString()));
      MustConsumeToken(TokenKind::Identifier);
    }
  }

  // init if any.
  // FIXME: Just a note, we need to check its redefinition.
  if (TryConsumeToken(TokenKind::Equal)) {
    Expr* init = ParseAssignmentExpr();
    std::for_each(vars.begin(), vars.end(),
                  [=](Decl* var) { var->As<VarDecl>()->SetInit(init); });
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
      // FIXME: This is not right for floating numbers.
      int value = std::stoi(CurrentToken().GetData());
      ConsumeToken();
      result = IntergerLiteral::Create(GetASTContext(), SourceRange(),
                                       GetASTContext().GetIntType(), value);
      break;
    }
    case TokenKind::StringLiteral: {
      result = StringLiteral::Create(GetASTContext(), SourceRange(),
                                     CurrentToken().GetAsString());
      ConsumeToken();
      break;
    }
    case TokenKind::Char: {
      result = CharacterLiteral::Create(GetASTContext(), SourceRange(),
                                        GetASTContext().GetCharType(),
                                        CurrentToken().GetAsString());
      ConsumeToken();
      break;
    }
    case TokenKind::Ampersand: {
      ConsumeToken();
      result = ParseCastExpr();
      result =
          UnaryExpr::Create(GetASTContext(), SourceRange(), result->GetType(),
                            UnaryOperatorKind::AddressOf, result);
      return result;
    }
    case TokenKind::Identifier: {
      std::string name = CurrentToken().GetAsString();
      ConsumeToken();
      // Lookup the identifier and find where it comes from.
      if (auto* decl = Lookup(name)) {
        auto* var = decl->As<VarDecl>();
        result = DeclRefExpr::Create(GetASTContext(), SourceRange(),
                                     decl->GetType(), decl);
      } else {
        jcc_unimplemented();
      }
      break;
    }
    default:
      jcc_unreachable("Unexpeted token kind!");
  }
  return ParsePostfixExpr(result);
}

std::vector<Expr*> Parser::ParseExprList() {
  std::vector<Expr*> expr_list;
  while (true) {
    expr_list.push_back(ParseAssignmentExpr());
    if (!CurrentToken().Is<TokenKind::Comma>()) {
      break;
    }
    MustConsumeToken(TokenKind::Comma);
  }
  return expr_list;
}

Expr* Parser::ParsePostfixExpr(Expr* lhs) {
  while (true) {
    switch (CurrentToken().GetKind()) {
      case TokenKind::LeftParen: {
        ConsumeToken();
        std::vector<Expr*> args;
        if (!CurrentToken().Is<TokenKind::RightParen>()) {
          args = ParseExprList();
        }
        Type* type = lhs->As<DeclRefExpr>()
                         ->GetRefDecl()
                         ->As<FunctionDecl>()
                         ->GetReturnType();
        lhs = CallExpr::Create(GetASTContext(), SourceRange(), type, lhs,
                               std::move(args));
        MustConsumeToken(TokenKind::RightParen);
        break;
      }
      case TokenKind::PlusPlus:
      case TokenKind::MinusMinus: {
        UnaryOperatorKind op_kind;
        if (CurrentToken().GetKind() == TokenKind::PlusPlus) {
          op_kind = UnaryOperatorKind::PostIncrement;
        } else if (CurrentToken().GetKind() == TokenKind::MinusMinus) {
          op_kind = UnaryOperatorKind::PostDecrement;
        } else {
          jcc_unreachable("Can only handle `++` or `--` here!");
        }

        ConsumeToken();
        lhs = UnaryExpr::Create(GetASTContext(), SourceRange(), lhs->GetType(),
                                op_kind, lhs);
        break;
      }
      case TokenKind::Arrow:
      case TokenKind::Period: {
        jcc_unimplemented();
        break;
      }
      default:
        return lhs;
    }
  }
}

static BinaryOperatorKind ConvertOpToKind(TokenKind kind) {
  switch (kind) {
    case TokenKind::Plus:
      return BinaryOperatorKind::Plus;
    case TokenKind::PlusEqual:
      return BinaryOperatorKind::PlusEqual;
    case TokenKind::Minus:
      return BinaryOperatorKind::Minus;
    case TokenKind::MinusEqual:
      return BinaryOperatorKind::MinusEqual;
    case TokenKind::Star:
      return BinaryOperatorKind::Multiply;
    case TokenKind::StarEqual:
      return BinaryOperatorKind::MultiplyEqual;
    case TokenKind::Slash:
      return BinaryOperatorKind::Divide;
    case TokenKind::SlashEqual:
      return BinaryOperatorKind::DivideEqual;
    case TokenKind::Greater:
      return BinaryOperatorKind::Greater;
    case TokenKind::GreaterEqual:
      return BinaryOperatorKind::GreaterEqual;
    case TokenKind::Less:
      return BinaryOperatorKind::Less;
    case TokenKind::LessEqual:
      return BinaryOperatorKind::LessEqual;
    case TokenKind::Equal:
      return BinaryOperatorKind::Equal;
    case TokenKind::EqualEqual:
      return BinaryOperatorKind::EqualEqual;
    default:
      jcc_unimplemented();
  }
}

Expr* Parser::ParseRhsOfBinaryExpr(Expr* lhs, BinOpPreLevel min_prec) {
  BinOpPreLevel next_tok_prec = GetBinOpPrecedence(CurrentToken().GetKind());
  while (true) {
    if (next_tok_prec < min_prec) {
      break;
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

    Expr* binary_expr =
        BinaryExpr::Create(GetASTContext(), SourceRange(), lhs->GetType(),
                           ConvertOpToKind(op_tok.GetKind()), lhs, rhs);
    lhs = binary_expr;
  }
  return lhs;
}

// Function or a simple declaration
std::vector<Decl*> Parser::ParseFunctionOrVar(DeclSpec& decl_spec) {
  std::vector<Decl*> decls;
  // Cases like struct { int x; };
  if (TryConsumeToken(TokenKind::Semi)) {
    return decls;
  }

  Declarator declarator = ParseDeclarator(decl_spec);
  if (declarator.GetTypeKind() == TypeKind::Func) {
    Decl* func = ParseFunction(declarator);
    decls.push_back(func);
  } else {
    std::vector<Decl*> vars = ParseDeclaration(declarator);
    decls.insert(decls.end(), vars.begin(), vars.end());
  }
  return decls;
}

// typedef int A, B, C;
void Parser::ParseTypedef(DeclSpec& decl_spec) {
  while (!CurrentToken().Is<TokenKind::Semi>()) {
    if (CurrentToken().Is<TokenKind::Comma>()) {
      // Skip the comma.
      ConsumeToken();
    }
    assert(CurrentToken().GetKind() == TokenKind::Identifier &&
           "Not an identifier in parsing typedef?");
    std::string ident = CurrentToken().GetAsString();
    GetCurScope().PushType(ident, decl_spec.GetType());
    ConsumeToken();
  }
  MustConsumeToken(TokenKind::Semi);
}

std::vector<Decl*> Parser::ParseTranslateUnit() {
  ScopeRAII scope_guard(*this);  // The file scope.

  std::vector<Decl*> top_decls;
  while (!CurrentToken().Is<TokenKind::Eof>()) {
    DeclSpec decl_spec = ParseDeclSpec();
    // TODO(Jun): Handle typedefs
    if (decl_spec.IsTypedef()) {
      ParseTypedef(decl_spec);
    }

    std::vector<Decl*> decls = ParseFunctionOrVar(decl_spec);
    top_decls.insert(top_decls.end(), decls.begin(), decls.end());
  }

  return top_decls;
}
}  // namespace jcc
