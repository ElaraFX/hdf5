# runTest.cmake executes a command and captures the output in a file. File is then compared
# against a reference file. Exit status of command can also be compared.
cmake_policy(SET CMP0007 NEW)

# arguments checking
if (NOT TEST_TESTER)
  message (FATAL_ERROR "Require TEST_TESTER to be defined")
endif (NOT TEST_TESTER)
if (NOT TEST_PROGRAM)
  message (FATAL_ERROR "Require TEST_PROGRAM to be defined")
endif (NOT TEST_PROGRAM)
if (NOT TEST_LIBRARY_DIRECTORY)
  message (STATUS "Require TEST_LIBRARY_DIRECTORY to be defined")
endif (NOT TEST_LIBRARY_DIRECTORY)
if (NOT TEST_FOLDER)
  message ( FATAL_ERROR "Require TEST_FOLDER to be defined")
endif (NOT TEST_FOLDER)
if (NOT TEST_OUTPUT)
  message (FATAL_ERROR "Require TEST_OUTPUT to be defined")
endif (NOT TEST_OUTPUT)
if (NOT TEST_CLASSPATH)
  message (STATUS "Require TEST_CLASSPATH to be defined")
endif (NOT TEST_CLASSPATH)
if (NOT TEST_REFERENCE)
  message (FATAL_ERROR "Require TEST_REFERENCE to be defined")
endif (NOT TEST_REFERENCE)

if (NOT TEST_ERRREF)
  set (ERROR_APPEND 1)
endif (NOT TEST_ERRREF)

if (NOT TEST_LOG_LEVEL)
  set (LOG_LEVEL "info")
else (NOT TEST_LOG_LEVEL)
  set (LOG_LEVEL "${TEST_LOG_LEVEL}")
endif (NOT TEST_LOG_LEVEL)

message (STATUS "COMMAND: ${TEST_TESTER} -Xmx1024M -Dorg.slf4j.simpleLogger.defaultLog=${LOG_LEVEL} -Djava.library.path=\"${TEST_LIBRARY_DIRECTORY}\" -cp \"${TEST_CLASSPATH}\" ${TEST_ARGS} ${TEST_PROGRAM} ${ARGN}")

if (WIN32 AND NOT MINGW)
  set (ENV{PATH} "$ENV{PATH}\\;${TEST_LIBRARY_DIRECTORY}")
endif (WIN32 AND NOT MINGW)

# run the test program, capture the stdout/stderr and the result var
execute_process (
    COMMAND ${TEST_TESTER} -Xmx1024M
    -Dorg.slf4j.simpleLogger.defaultLogLevel=${LOG_LEVEL}
    -Djava.library.path=${TEST_LIBRARY_DIRECTORY}
    -cp "${TEST_CLASSPATH}" ${TEST_ARGS} ${TEST_PROGRAM}
    ${ARGN}
    WORKING_DIRECTORY ${TEST_FOLDER}
    RESULT_VARIABLE TEST_RESULT
    OUTPUT_FILE ${TEST_OUTPUT}
    ERROR_FILE ${TEST_OUTPUT}.err
    ERROR_VARIABLE TEST_ERROR
)

message (STATUS "COMMAND Result: ${TEST_RESULT}")

if (EXISTS ${TEST_FOLDER}/${TEST_OUTPUT}.err)
  file (READ ${TEST_FOLDER}/${TEST_OUTPUT}.err TEST_STREAM)
  if (TEST_MASK_FILE)
    STRING(REGEX REPLACE "CurrentDir is [^\n]+\n" "CurrentDir is (dir name)\n" TEST_STREAM "${TEST_STREAM}") 
  endif (TEST_MASK_FILE)

  if (ERROR_APPEND)
    file (APPEND ${TEST_FOLDER}/${TEST_OUTPUT} "${TEST_STREAM}") 
  else (ERROR_APPEND)
    file (WRITE ${TEST_FOLDER}/${TEST_OUTPUT}.err "${TEST_STREAM}")
  endif (ERROR_APPEND)
endif (EXISTS ${TEST_FOLDER}/${TEST_OUTPUT}.err)

if (TEST_MASK_ERROR)
  if (NOT TEST_ERRREF)
    file (READ ${TEST_FOLDER}/${TEST_OUTPUT} TEST_STREAM)
  else (NOT TEST_ERRREF)
    file (READ ${TEST_FOLDER}/${TEST_OUTPUT}.err TEST_STREAM)
  endif (NOT TEST_ERRREF)
  string (REGEX REPLACE "Time:[^\n]+\n" "Time:  XXXX\n" TEST_STREAM "${TEST_STREAM}") 
  string (REGEX REPLACE "thread [0-9]*:" "thread (IDs):" TEST_STREAM "${TEST_STREAM}") 
  string (REGEX REPLACE ": ([^\n]*)[.]c " ": (file name) " TEST_STREAM "${TEST_STREAM}") 
  string (REGEX REPLACE " line [0-9]*" " line (number)" TEST_STREAM "${TEST_STREAM}") 
  #string (REGEX REPLACE "v[1-9]*[.][0-9]*[.]" "version (number)." TEST_STREAM "${TEST_STREAM}") 
  string (REGEX REPLACE "HDF5 .[1-9]*[.][0-9]*[.][0-9]*[^)]*" "HDF5 (version (number)" TEST_STREAM "${TEST_STREAM}") 
  string (REGEX REPLACE "H5Eget_auto[1-2]*" "H5Eget_auto(1 or 2)" TEST_STREAM "${TEST_STREAM}") 
  string (REGEX REPLACE "H5Eset_auto[1-2]*" "H5Eset_auto(1 or 2)" TEST_STREAM "${TEST_STREAM}") 
  if (NOT TEST_ERRREF)
    file (WRITE ${TEST_FOLDER}/${TEST_OUTPUT} "${TEST_STREAM}")
  else (NOT TEST_ERRREF)
    file (WRITE ${TEST_FOLDER}/${TEST_OUTPUT}.err "${TEST_STREAM}")
  endif (NOT TEST_ERRREF)
endif (TEST_MASK_ERROR)

# if the return value is !=0 bail out
if (NOT ${TEST_RESULT} STREQUAL ${TEST_EXPECT})
  message (STATUS "ERROR OUTPUT: ${TEST_STREAM}")
  message (FATAL_ERROR "Failed: Test program ${TEST_PROGRAM} exited != 0.\n${TEST_ERROR}")
endif (NOT ${TEST_RESULT} STREQUAL ${TEST_EXPECT})

message (STATUS "COMMAND Error: ${TEST_ERROR}")

if (NOT TEST_SKIP_COMPARE)
  if (WIN32 AND NOT MINGW)
    file (READ ${TEST_FOLDER}/${TEST_REFERENCE} TEST_STREAM)
    file (WRITE ${TEST_FOLDER}/${TEST_REFERENCE} "${TEST_STREAM}")
  endif (WIN32 AND NOT MINGW)

  # now compare the output with the reference
  execute_process (
      COMMAND ${CMAKE_COMMAND} -E compare_files ${TEST_FOLDER}/${TEST_OUTPUT} ${TEST_FOLDER}/${TEST_REFERENCE}
      RESULT_VARIABLE TEST_RESULT
  )
  if (NOT ${TEST_RESULT} STREQUAL 0)
  set (TEST_RESULT 0)
  file (STRINGS ${TEST_FOLDER}/${TEST_OUTPUT} test_act)
  list (LENGTH test_act len_act)
  file (STRINGS ${TEST_FOLDER}/${TEST_REFERENCE} test_ref)
  list (LENGTH test_ref len_ref)
  if (NOT ${len_act} STREQUAL "0")
    MATH (EXPR _FP_LEN "${len_ref} - 1")
    foreach (line RANGE 0 ${_FP_LEN})
      list (GET test_act ${line} str_act)
      list (GET test_ref ${line} str_ref)
      if (NOT "${str_act}" STREQUAL "${str_ref}")
        if (NOT "${str_act}" STREQUAL "")
          set (TEST_RESULT 1)
          message ("line = ${line}\n***ACTUAL: ${str_act}\n****REFER: ${str_ref}\n")
         endif (NOT "${str_act}" STREQUAL "")
      endif (NOT "${str_act}" STREQUAL "${str_ref}")
    endforeach (line RANGE 0 ${_FP_LEN})
  endif (NOT ${len_act} STREQUAL "0")
  if (NOT ${len_act} STREQUAL ${len_ref})
    set (TEST_RESULT 1)
  endif (NOT ${len_act} STREQUAL ${len_ref})
  endif (NOT ${TEST_RESULT} STREQUAL 0)

  message (STATUS "COMPARE Result: ${TEST_RESULT}")

  # again, if return value is !=0 scream and shout
  if (NOT ${TEST_RESULT} STREQUAL 0)
    message (FATAL_ERROR "Failed: The output of ${TEST_OUTPUT} did not match ${TEST_REFERENCE}")
  endif (NOT ${TEST_RESULT} STREQUAL 0)
  
  if (TEST_ERRREF)
    if (WIN32 AND NOT MINGW)
      file (READ ${TEST_FOLDER}/${TEST_ERRREF} TEST_STREAM)
      file (WRITE ${TEST_FOLDER}/${TEST_ERRREF} "${TEST_STREAM}")
    endif (WIN32 AND NOT MINGW)

    # now compare the error output with the error reference
    execute_process (
        COMMAND ${CMAKE_COMMAND} -E compare_files ${TEST_FOLDER}/${TEST_OUTPUT}.err ${TEST_FOLDER}/${TEST_ERRREF}
        RESULT_VARIABLE TEST_RESULT
    )
    if (NOT ${TEST_RESULT} STREQUAL 0)
    set (TEST_RESULT 0)
    file (STRINGS ${TEST_FOLDER}/${TEST_OUTPUT}.err test_act)
    list (LENGTH test_act len_act)
    file (STRINGS ${TEST_FOLDER}/${TEST_ERRREF} test_ref)
    list (LENGTH test_ref len_ref)
    MATH (EXPR _FP_LEN "${len_ref} - 1")
    if (NOT ${len_act} STREQUAL "0")
      MATH (EXPR _FP_LEN "${len_ref} - 1")
      foreach (line RANGE 0 ${_FP_LEN})
        list (GET test_act ${line} str_act)
        list (GET test_ref ${line} str_ref)
        if (NOT "${str_act}" STREQUAL "${str_ref}")
          if (NOT "${str_act}" STREQUAL "")
            set (TEST_RESULT 1)
            message ("line = ${line}\n***ACTUAL: ${str_act}\n****REFER: ${str_ref}\n")
           endif (NOT "${str_act}" STREQUAL "")
        endif (NOT "${str_act}" STREQUAL "${str_ref}")
      endforeach (line RANGE 0 ${_FP_LEN})
    endif (NOT ${len_act} STREQUAL "0")
    if (NOT ${len_act} STREQUAL ${len_ref})
      set (TEST_RESULT 1)
    endif (NOT ${len_act} STREQUAL ${len_ref})
    endif (NOT ${TEST_RESULT} STREQUAL 0)

    message (STATUS "COMPARE Result: ${TEST_RESULT}")

    # again, if return value is !=0 scream and shout
    if (NOT ${TEST_RESULT} STREQUAL 0)
      message (FATAL_ERROR "Failed: The error output of ${TEST_OUTPUT}.err did not match ${TEST_ERRREF}")
    endif (NOT ${TEST_RESULT} STREQUAL 0)
  endif (TEST_ERRREF)
endif (NOT TEST_SKIP_COMPARE)

if (TEST_GREP_COMPARE)
  # now grep the output with the reference
  file (READ ${TEST_FOLDER}/${TEST_OUTPUT} TEST_STREAM)

  # TEST_REFERENCE should always be matched
  string (REGEX MATCH "${TEST_REFERENCE}" TEST_MATCH ${TEST_STREAM}) 
  string (COMPARE EQUAL "${TEST_REFERENCE}" "${TEST_MATCH}" TEST_RESULT) 
  if (${TEST_RESULT} STREQUAL "0")
    message (FATAL_ERROR "Failed: The output of ${TEST_PROGRAM} did not contain ${TEST_REFERENCE}")
  endif (${TEST_RESULT} STREQUAL "0")

  string (REGEX MATCH "${TEST_FILTER}" TEST_MATCH ${TEST_STREAM}) 
  if (${TEST_EXPECT} STREQUAL "1")
    # TEST_EXPECT (1) interperts TEST_FILTER as NOT to match
    string (LENGTH "${TEST_MATCH}" TEST_RESULT) 
    if (NOT ${TEST_RESULT} STREQUAL "0")
      message (FATAL_ERROR "Failed: The output of ${TEST_PROGRAM} did contain ${TEST_FILTER}")
    endif (NOT ${TEST_RESULT} STREQUAL "0")
  endif (${TEST_EXPECT} STREQUAL "0")
endif (TEST_GREP_COMPARE)

# everything went fine...
message ("${TEST_PROGRAM} Passed")

