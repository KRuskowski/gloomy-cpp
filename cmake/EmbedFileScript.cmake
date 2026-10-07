#=====================================
# Script mode: writes the bytes of INPUT as a C++ array to OUTPUT
#=====================================
#
# -DINPUT=<file> -DOUTPUT=<header> -DSYMBOL=<name> -DWORDS=<ON|OFF>
# WORDS=ON emits std::uint32_t words (SPIR-V, little endian), otherwise bytes.

file(READ ${INPUT} hex HEX)
string(LENGTH "${hex}" hex_length)

if(WORDS)
  # Regroup each 4 bytes (8 hex digits) as a little endian 32 bit word.
  string(REGEX REPLACE
    "(..)(..)(..)(..)" "0x\\4\\3\\2\\1u," body "${hex}")
  set(type "std::uint32_t")
else()
  string(REGEX REPLACE "(..)" "0x\\1," body "${hex}")
  set(type "unsigned char")
endif()

# Break the initialiser into lines of 16 values.
string(REGEX REPLACE "(([^,]+,){16})" "\\1\n" body "${body}")

file(WRITE ${OUTPUT}
  "// Generated from ${INPUT}. Do not edit.\n"
  "#ifndef GLOOMY_GENERATED_${SYMBOL}_H_\n"
  "#define GLOOMY_GENERATED_${SYMBOL}_H_\n"
  "#include <cstdint>\n"
  "inline constexpr ${type} ${SYMBOL}[] = {\n${body}\n};\n"
  "#endif\n"
)
