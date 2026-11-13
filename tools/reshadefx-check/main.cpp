#include "effect_parser.hpp"
#include "effect_preprocessor.hpp"
#include "effect_codegen.hpp"
#include <cstdio>
#include <memory>
int main(int argc, char** argv)
{
  reshadefx::preprocessor pp;
  pp.add_include_path(argv[2]);
  pp.add_macro_definition("__RESHADE__", "60800");
  pp.add_macro_definition("__RENDERER__", "0xb000");
  pp.add_macro_definition("BUFFER_WIDTH", "2560");
  pp.add_macro_definition("BUFFER_HEIGHT", "1440");
  pp.add_macro_definition("BUFFER_RCP_WIDTH", "(1.0 / BUFFER_WIDTH)");
  pp.add_macro_definition("BUFFER_RCP_HEIGHT", "(1.0 / BUFFER_HEIGHT)");
  pp.add_macro_definition("BUFFER_COLOR_BIT_DEPTH", "8");
  pp.add_macro_definition("BUFFER_COLOR_SPACE", "1");
  pp.add_macro_definition("RESHADE_DEPTH_INPUT_IS_UPSIDE_DOWN", "0");
  pp.add_macro_definition("RESHADE_DEPTH_INPUT_IS_REVERSED", "0");
  pp.add_macro_definition("RESHADE_DEPTH_INPUT_IS_LOGARITHMIC", "0");
  pp.add_macro_definition("RESHADE_DEPTH_LINEARIZATION_FAR_PLANE", "1000.0");
  if (!pp.append_file(argv[1])) { fprintf(stderr, "PP:\n%s\n", pp.errors().c_str()); return 1; }
  reshadefx::parser parser;
  std::unique_ptr<reshadefx::codegen> cg(reshadefx::create_codegen_hlsl(50, true, true));
  bool ok = parser.parse(pp.output(), cg.get());
  fprintf(stderr, "%s%s", pp.errors().c_str(), parser.errors().c_str());
  printf(ok ? "OK\n" : "FAIL\n");
  return ok ? 0 : 1;
}
