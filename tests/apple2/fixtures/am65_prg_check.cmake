# Verify am65 --prg writes a Commodore LE load-address header from the
# lowest emitted address. Invoked as a ctest -P script.
if(NOT AM65 OR NOT ASM OR NOT OUT)
  message(FATAL_ERROR "AM65, ASM, and OUT must be set")
endif()

execute_process(
  COMMAND "${AM65}" -i "${ASM}" -o "${OUT}" --prg
  RESULT_VARIABLE rc
  ERROR_VARIABLE err
)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "am65 --prg failed (${rc}): ${err}")
endif()

file(READ "${OUT}" prg_hex HEX)
if(NOT prg_hex STREQUAL "0108010203")
  message(FATAL_ERROR "unexpected --prg output hex '${prg_hex}' (want 0108010203)")
endif()

execute_process(
  COMMAND "${AM65}" -i "${ASM}" -o "${OUT}.raw"
  RESULT_VARIABLE rc
  ERROR_VARIABLE err
)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "am65 raw failed (${rc}): ${err}")
endif()

file(READ "${OUT}.raw" raw_hex HEX)
if(NOT raw_hex STREQUAL "010203")
  message(FATAL_ERROR "unexpected raw output hex '${raw_hex}' (want 010203)")
endif()
