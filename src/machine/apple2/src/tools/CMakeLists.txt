# Leftover tools INTERFACE. am65 / disasm_6502 / symbols live under src/shell/tools/.

add_library(tools INTERFACE)

target_link_libraries(tools INTERFACE
    assembler
    disasm_6502
    symbols
)
