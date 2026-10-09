# am65 -s matrix: N <= 1 is a combined dump; N >= 2 writes sidecars and
# ignores a non-"-" path, including when assemble fails and writes nothing.
if(NOT AM65 OR NOT WORK)
  message(FATAL_ERROR "AM65 and WORK must be set")
endif()

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
file(TO_CMAKE_PATH "${WORK}" work)

function(require_contains path needle)
  if(NOT EXISTS "${path}")
    message(FATAL_ERROR "missing ${path}")
  endif()
  file(READ "${path}" body)
  string(FIND "${body}" "${needle}" pos)
  if(pos LESS 0)
    message(FATAL_ERROR "${path} did not contain '${needle}':\n${body}")
  endif()
endfunction()

function(require_absent path needle)
  if(NOT EXISTS "${path}")
    return()
  endif()
  file(READ "${path}" body)
  string(FIND "${body}" "${needle}" pos)
  if(NOT pos LESS 0)
    message(FATAL_ERROR "${path} unexpectedly contained '${needle}'")
  endif()
endfunction()

function(require_missing path)
  if(EXISTS "${path}")
    message(FATAL_ERROR "unexpected file ${path}")
  endif()
endfunction()

# N == 1. The empty file= target emitted nothing, but its equate is in the
# combined dump. No sidecar is written for it.
file(WRITE "${WORK}/one.asm"
"* = \$C000\nstart: ldx #0\n.scope empty file=\"${work}/empty.bin\"\nghost = \$1234\n.endscope\n")
execute_process(
  COMMAND "${AM65}" -i "${WORK}/one.asm" -o "${WORK}/out.bin" -s "${WORK}/symbols.txt"
  RESULT_VARIABLE rc
  ERROR_VARIABLE err
)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "N<=1 assemble failed (${rc}): ${err}")
endif()
require_contains("${WORK}/symbols.txt" "C000  start")
require_contains("${WORK}/symbols.txt" "1234  empty::ghost")
require_missing("${WORK}/empty.bin")
require_missing("${WORK}/empty.bin.sym")

# N == 2. Do not create the user's -s path. One sidecar per file that wrote
# bytes. An equate on a file= target that emitted nothing stays out.
file(WRITE "${WORK}/many.asm"
".scope one file=\"${work}/a.bin\"\n* = \$C000\nstart_a: nop\n.endscope\n.scope two file=\"${work}/b.bin\"\n* = \$C000\nstart_b: lda #\$01\n.endscope\n.scope quiet file=\"${work}/quiet.bin\"\nquiet_equ = \$99\n.endscope\n")
execute_process(
  COMMAND "${AM65}" -i "${WORK}/many.asm" -s "${WORK}/listing.sym"
  RESULT_VARIABLE rc
  ERROR_VARIABLE err
)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "N>=2 assemble failed (${rc}): ${err}")
endif()
string(FIND "${err}" "-s ${WORK}/listing.sym ignored; wrote symbol sidecars beside 2 output files" note)
if(note LESS 0)
  message(FATAL_ERROR "missing sidecar stderr line:\n${err}")
endif()
require_missing("${WORK}/listing.sym")
require_missing("${WORK}/quiet.bin")
require_missing("${WORK}/quiet.bin.sym")
require_contains("${WORK}/a.bin.sym" "C000  one::start_a")
require_absent("${WORK}/a.bin.sym" "two::start_b")
require_absent("${WORK}/a.bin.sym" "quiet::quiet_equ")
require_contains("${WORK}/b.bin.sym" "C000  two::start_b")
require_absent("${WORK}/b.bin.sym" "one::start_a")
require_contains("${WORK}/a.bin.sym" "SEGMENTS -> ${work}/a.bin")
require_absent("${WORK}/a.bin.sym" "SEGMENTS -> ${work}/b.bin")

# N == 2 and assemble fails. The stderr line is still printed and no sidecar
# or user path is created.
file(WRITE "${WORK}/fail.asm"
".scope one file=\"${work}/fail_a.bin\"\n* = \$C000\nstart_a: nop\n.endscope\n.scope two file=\"${work}/fail_b.bin\"\n* = \$C000\nstart_b: nop\nlda missing\n.endscope\n")
execute_process(
  COMMAND "${AM65}" -i "${WORK}/fail.asm" -s "${WORK}/failed.sym"
  RESULT_VARIABLE rc
  ERROR_VARIABLE err
)
if(rc EQUAL 0)
  message(FATAL_ERROR "failing assemble returned success:\n${err}")
endif()
string(FIND "${err}" "-s ${WORK}/failed.sym ignored; wrote symbol sidecars beside 2 output files" note)
if(note LESS 0)
  message(FATAL_ERROR "failed assemble did not print the sidecar line:\n${err}")
endif()
require_missing("${WORK}/failed.sym")
require_missing("${WORK}/fail_a.bin")
require_missing("${WORK}/fail_a.bin.sym")
require_missing("${WORK}/fail_b.bin")
require_missing("${WORK}/fail_b.bin.sym")
