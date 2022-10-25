#include <fmt/format.h>

#include <fstream>
#include <string>
#include <string_view>
#include <vector>

#include "jcc/codegen.h"
#include "jcc/decl.h"
#include "jcc/lexer.h"
#include "jcc/parser.h"

static std::string ReadFile(std::string_view name) {
  std::ifstream file{name.data()};
  std::string contents{std::istreambuf_iterator<char>(file),
                       std::istreambuf_iterator<char>()};
  return contents;
}

int main(int argc, char** argv) {
  if (argc != 2) {
    fmt::print("You should only pass one argument for now!\n");
    return 1;
  }

  std::string file_name = argv[1];
  std::string content = ReadFile(file_name);

  jcc::Lexer lexer(content, file_name);
  jcc::Parser parser(lexer);
  jcc::CodeGen codegen(file_name);

  std::vector<jcc::Decl*> decls = parser.ParseTranslateUnit();

  for (jcc::Decl* decl : decls) {
    decl->GenCode(codegen);
  }
}
