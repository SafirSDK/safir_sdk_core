# Macro for setting up precompiled headers. Usage:
#
#   add_precompiled_header(target header.h [FORCEINCLUDE])
#
# MSVC: A source file with the same name as the header must exist and
# be included in the target (E.g. header.cpp).
#
# MSVC: Add FORCEINCLUDE to automatically include the precompiled
# header file from every source file.
#
# GCC: The precompiled header is always automatically included from
# every header file.
#
# Copyright (C) 2009-2013 Lars Christensen <larsch@belunktum.dk>
# Copyright (C) 2013-2014 Lars Hagström <lars@foldspace.nu>
#
# Permission is hereby granted, free of charge, to any person
# obtaining a copy of this software and associated documentation files
# (the ‘Software’), to deal in the Software without restriction,
# including without limitation the rights to use, copy, modify, merge,
# publish, distribute, sublicense, and/or sell copies of the Software,
# and to permit persons to whom the Software is furnished to do so,
# subject to the following conditions:
#
# The above copyright notice and this permission notice shall be
# included in all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED ‘AS IS’, WITHOUT WARRANTY OF ANY KIND,
# EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
# NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
# BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
# ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
# CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

function(ADD_PRECOMPILED_HEADER _targetName _input)
  FOREACH(arg ${ARGN})
    IF(arg STREQUAL FORCEINCLUDE)
      SET(FORCEINCLUDE ON)
    ELSE()
      SET(FORCEINCLUDE OFF)
    ENDIF()
  ENDFOREACH(arg)

  IF(MSVC)
    GET_FILENAME_COMPONENT(_inputWe ${_input} NAME_WE)
    GET_TARGET_PROPERTY(sources ${_targetName} SOURCES)
    SET(_sourceFound FALSE)
    FOREACH(_source ${sources})
      SET(PCH_COMPILE_FLAGS "")
      IF(_source MATCHES \\.\(cc|cxx|cpp\)$)
        GET_FILENAME_COMPONENT(_sourceWe ${_source} NAME_WE)
        IF(_sourceWe STREQUAL ${_inputWe})
          SET(PCH_COMPILE_FLAGS "${PCH_COMPILE_FLAGS} /Yc\"${_input}\"")
          SET_SOURCE_FILES_PROPERTIES(${_source} PROPERTIES OBJECT_OUTPUTS ${_inputWe}.pch)
          SET(_sourceFound TRUE)
        ELSE(_sourceWe STREQUAL ${_inputWe})
          SET(PCH_COMPILE_FLAGS "${PCH_COMPILE_FLAGS} /Yu\"${_input}\"")
          IF(FORCEINCLUDE)
            SET(PCH_COMPILE_FLAGS "${PCH_COMPILE_FLAGS} /FI\"${_input}\"")
          ENDIF(FORCEINCLUDE)
          SET_SOURCE_FILES_PROPERTIES(${_source} PROPERTIES OBJECT_DEPENDS ${_inputWe}.pch)
        ENDIF(_sourceWe STREQUAL ${_inputWe})
        SET_SOURCE_FILES_PROPERTIES(${_source} PROPERTIES COMPILE_FLAGS "${PCH_COMPILE_FLAGS}")
      ENDIF(_source MATCHES \\.\(cc|cxx|cpp\)$)
    ENDFOREACH()
    IF(NOT _sourceFound)
      MESSAGE(FATAL_ERROR "A source file for ${_input} was not found. Required for MSVC builds.")
    ENDIF(NOT _sourceFound)
  ENDIF(MSVC)

  IF(CMAKE_COMPILER_IS_GNUCXX)
    GET_FILENAME_COMPONENT(_name ${_input} NAME)
    GET_FILENAME_COMPONENT(_source "${_input}" REALPATH)
    SET(_outdir "${CMAKE_CURRENT_BINARY_DIR}/${_name}.gch")
    SET(_output "${_outdir}")

    STRING(TOUPPER "CMAKE_CXX_FLAGS_${CMAKE_BUILD_TYPE}" _flags_var_name)
    SET(_compiler_FLAGS ${${_flags_var_name}} -fPIC -pthread)

    GET_TARGET_PROPERTY(_include_dirs ${_targetName} INCLUDE_DIRECTORIES)
    FOREACH(item ${_include_dirs})
      LIST(APPEND _compiler_FLAGS "-I\"${item}\"")
    ENDFOREACH(item)

    GET_DIRECTORY_PROPERTY(_directory_flags DEFINITIONS)
    LIST(APPEND _compiler_FLAGS ${_directory_flags})

    SEPARATE_ARGUMENTS(_compiler_FLAGS)
    #MESSAGE(STATUS "PCH command line: ${CMAKE_CXX_COMPILER} ${_compiler_FLAGS} -x c++-header ${_source} -o ${_output}")
    ADD_CUSTOM_COMMAND(
      OUTPUT ${_output}
      COMMAND ${CMAKE_CXX_COMPILER} ${_compiler_FLAGS} -x c++-header  ${_source} -o ${_output}
      DEPENDS ${_source} )
    ADD_CUSTOM_TARGET(${_targetName}_gch DEPENDS ${_output})
    ADD_DEPENDENCIES(${_targetName} ${_targetName}_gch)
    SET_TARGET_PROPERTIES(${_targetName} PROPERTIES COMPILE_FLAGS "-include ${_name} -Winvalid-pch")
  ENDIF(CMAKE_COMPILER_IS_GNUCXX)

  UNSET(FORCEINCLUDE)
endfunction()
