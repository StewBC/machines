# Leftover C64 format parsers. am65 / disasm_6502 / symbols live under src/shell/tools/.

add_library(tools INTERFACE)

add_subdirectory(crt)
add_subdirectory(d64)
add_subdirectory(g64)
add_subdirectory(t64)

target_link_libraries(tools INTERFACE
    assembler
    crt
    d64
    g64
    disasm_6502
    symbols
    t64
)
