# Turns files into C++ byte arrays, so the program carries them (the UI's font) and stays one
# file. Run at build time by CMakeLists.txt; plain CMake, no xxd, the same on every system:
#   cmake -DOUT=<file.cpp> -DNAMES="name1|name2" -DFILES="path1|path2" -P embed.cmake
# Each name becomes `extern const unsigned char name[]` and `extern const unsigned long name_size`.
# (Lists are given with '|': a ';' would split the command's arguments.)
string(REPLACE "|" ";" NAMES "${NAMES}")
string(REPLACE "|" ";" FILES "${FILES}")
set(body "// Made at build time by cmake/embed.cmake: do not edit.\n")
list(LENGTH NAMES count)
math(EXPR last "${count} - 1")
foreach(i RANGE ${last})
    list(GET NAMES ${i} name)
    list(GET FILES ${i} path)
    file(READ "${path}" hex HEX)
    string(LENGTH "${hex}" chars)
    math(EXPR size "${chars} / 2")
    # 0x.. for every byte, a line every 32 bytes.
    string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," hex "${hex}")
    string(REGEX REPLACE "((0x..,){32})" "\\1\n" hex "${hex}")
    string(APPEND body "extern const unsigned char ${name}[] = {\n${hex}\n};\n")
    string(APPEND body "extern const unsigned long ${name}_size = ${size};\n")
endforeach()
file(WRITE "${OUT}" "${body}")
