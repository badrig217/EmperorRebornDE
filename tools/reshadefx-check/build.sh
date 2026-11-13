#!/bin/sh
# Builds a small checker that runs ReShade's own FX preprocessor + parser (Linux/macOS, g++).
# Use it to make sure a .fx file compiles before sending it to a tester.
#   ./build.sh && ./fxcheck path/to/Effect.fx path/to/reshade-shaders/Shaders
set -e
[ -d reshade ] || git clone --depth 1 https://github.com/crosire/reshade reshade
S=reshade/source
g++ -std=c++20 -O1 -w -I$S -Ireshade/include main.cpp \
  $S/effect_expression.cpp $S/effect_lexer.cpp $S/effect_parser_exp.cpp $S/effect_parser_stmt.cpp \
  $S/effect_preprocessor.cpp $S/effect_symbol_table.cpp $S/effect_codegen_hlsl.cpp -o fxcheck
echo "built ./fxcheck"
# Note: SMAA.fx and CAS.fx report tex2D*offset errors with this harness (shader model setup);
# they work fine in the real ReShade. Own effects and Glamarye compile cleanly.
