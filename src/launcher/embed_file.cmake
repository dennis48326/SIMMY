# Writes a binary file as a C array: cmake -DSRC=<file> -DDST=<out.inc> -DNAME=<symbol> -P embed_file.cmake
# The .inc defines `static const unsigned char NAME[]` and `static const size_t NAME_size`.
file(READ "${SRC}" hex HEX)
string(LENGTH "${hex}" hexlen)
math(EXPR size "${hexlen} / 2")
string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," bytes "${hex}")
# Break the line every 32 bytes (160 characters) so compilers and editors cope.
string(REGEX REPLACE "((0x..,){32})" "\\1\n" bytes "${bytes}")
file(WRITE "${DST}.tmp" "// Generated from ${SRC} by embed_file.cmake; do not edit.\n"
     "static const unsigned char ${NAME}[] = {\n${bytes}\n};\n"
     "static const size_t ${NAME}_size = ${size};\n")
file(RENAME "${DST}.tmp" "${DST}")
