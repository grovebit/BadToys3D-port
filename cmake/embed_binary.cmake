if (NOT DEFINED INPUT OR NOT DEFINED OUTPUT OR NOT DEFINED SYMBOL)
    message(FATAL_ERROR "INPUT, OUTPUT, and SYMBOL are required")
endif()

if (EXISTS "${INPUT}")
    file(READ "${INPUT}" _hex HEX)
    string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," _bytes "${_hex}")
    string(LENGTH "${_hex}" _hex_len)
    math(EXPR _size "${_hex_len} / 2")
else()
    set(_bytes "0x00,")
    set(_size 0)
endif()

file(WRITE "${OUTPUT}"
"#ifndef BT3D_EMBEDDED_MIDI_H
#define BT3D_EMBEDDED_MIDI_H

static const unsigned char ${SYMBOL}_data[] = {
${_bytes}
};
static const unsigned int ${SYMBOL}_size = ${_size}u;

#endif
")
