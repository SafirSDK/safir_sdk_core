FUNCTION(BUILD_GENERATED_LIBRARY)
  cmake_parse_arguments(GEN "NO_INSTALL" "NAME" "DEPENDENCIES" ${ARGN})
  #message("++ Will build generated library ${GEN_NAME} with dependencies '${GEN_DEPENDENCIES}'")

  if ("${GEN_NAME}" STREQUAL "")
    message(FATAL_ERROR "Invalid NAME passed to BUILD_GENERATED_LIBRARY")
  endif()

  if (NOT "${GEN_UNPARSED_ARGUMENTS}" STREQUAL "")
    message(FATAL_ERROR "Unknown argument to BUILD_GENERATED_LIBRARY '${GEN_UNPARSED_ARGUMENTS}'")
  endif()

  # 
  # Dependency resolution
  #
  # Create custom targets dummy that we just use to have somewhere to put the dependencies.
  # Then, when we need to resolve a targets dependencies, we recursively look at the dummy
  # targets to find all of the dependencies of our dependencies.
  add_custom_target(${GEN_NAME}-dou)
  set_target_properties(${GEN_NAME}-dou PROPERTIES
    DOU_DEPENDENCIES "${GEN_DEPENDENCIES}"
    DOU_DIR ${CMAKE_CURRENT_SOURCE_DIR})

  #recursively get all dependendencies
  function(GET_DEPS)
    cmake_parse_arguments(DEP "" "" "DEPENDENCIES" ${ARGN})

    FOREACH (DEP ${DEP_DEPENDENCIES})
      if (NOT "${DEP}" STREQUAL "")
        get_target_property(deps ${DEP}-dou DOU_DEPENDENCIES)
        if ("${deps}" STREQUAL "deps-NOTFOUND")
          MESSAGE(FATAL_ERROR "Could not resolve dependency '${DEP}'")
        else()
          GET_DEPS(DEPENDENCIES ${deps})
          set (GET_DEPS_RESULT ${GET_DEPS_RESULT} ${DEP})
        endif()
      endif()
    ENDFOREACH()
    if(GET_DEPS_RESULT)
      LIST(REMOVE_DUPLICATES GET_DEPS_RESULT)
    endif()
    set (GET_DEPS_RESULT ${GET_DEPS_RESULT} PARENT_SCOPE)
  endfunction()

  GET_DEPS(DEPENDENCIES ${GEN_DEPENDENCIES})
  set(ALL_DEPENDENCIES ${GET_DEPS_RESULT})
  ################


  #
  # Set up variables containing all dou files and all expected source code files
  #
  FILE(GLOB_RECURSE dou_files *.dou)
  FILE(GLOB_RECURSE dom_files *.dom)
  FILE(GLOB_RECURSE namespace_files *.namespace.txt)
  
  #loop over all dou files
  foreach (dou ${dou_files})
    #message ("Dou ${dou}")
    string (REGEX REPLACE ".*/([a-zA-Z\\.0-9]*)\\.dou" "\\1" base_name ${dou})
    #message ("base_name ${base_name}")
    set (cpp_files ${cpp_files} generated_code/cpp/${base_name}.cpp)


    string (REGEX REPLACE "^([a-zA-Z\\.0-9]*)\\.[a-zA-Z0-9]+$" "\\1" namespace ${base_name})
    string (REGEX REPLACE "^[a-zA-Z\\.0-9]*\\.([a-zA-Z0-9]+)$" "\\1" java_base_name ${base_name})
    string (TOLOWER ${namespace} java_namespace)
    string (REPLACE "." "/" java_path ${java_namespace})
    #message ("namespace ${java_path}")
    #message ("class name ${java_base_name}")
    set (java_files ${java_files} generated_code/java/src/com/saabgroup/${java_path}/${java_base_name}.java)
    #message(generated_code/java/src/com/saabgroup/${java_path}/${java_base_name}.java)
    #TODO: read the namespace files and do this right
  endforeach()

  #message("Cpp files: ${cpp_files}")
  ##############  
  

  #TODO: fix the paths below!
  #
  # Generate code
  #
  # We set up some variables to point to dots_v and dod-files and directories,
  # and then we define the custom command that generates the code
  #
  foreach(DEP ${ALL_DEPENDENCIES})
    get_target_property(sd ${DEP}-dou DOU_DIR)
    set(DOTS_V_DEPS ${DOTS_V_DEPS} ${sd})
  endforeach()

  FIND_PACKAGE(PythonInterp)

  SET(dod_directory ${safir_sdk_core_SOURCE_DIR}/dots/dots_v.ss/data/)
  FILE(GLOB dod_files ${dod_directory} *.dod)
  SET(dots_v_command ${PYTHON_EXECUTABLE} 
    "${safir_sdk_core_SOURCE_DIR}/dots/dots_v.ss/dots_v.py" 
    --dod-files=${dod_directory} 
    --dependencies ${DOTS_V_DEPS} 
    --library-name ${GEN_NAME}
    --output-path=generated_code)
  
  ADD_CUSTOM_COMMAND(
    OUTPUT ${cpp_files} #TODO: ${java_files}

    COMMAND ${dots_v_command} ${CMAKE_CURRENT_SOURCE_DIR}

    DEPENDS ${dod_files} ${dou_files}
    COMMENT "Generating code for ${CMAKE_CURRENT_SOURCE_DIR}")

  #make clean target remove the generated_code directory
  SET_DIRECTORY_PROPERTIES(PROPERTIES ADDITIONAL_MAKE_CLEAN_FILES generated_code)
  #############



  
  #
  # Build CPP
  #
  ADD_LIBRARY(dots_generated-${GEN_NAME}-cpp SHARED ${cpp_files}) #TODO headers?
  
  target_include_directories(dots_generated-${GEN_NAME}-cpp
    PRIVATE ${safir_sdk_core_SOURCE_DIR}/dots/dots_v.ss/data
    PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/generated_code/cpp/include)
  
  target_link_libraries(dots_generated-${GEN_NAME}-cpp 
    dots_cpp
    dots_internal
    dots_kernel
    lluf_utils)

  FOREACH (DEP ${GEN_DEPENDENCIES})
    TARGET_LINK_LIBRARIES(dots_generated-${GEN_NAME}-cpp dots_generated-${DEP}-cpp)
  ENDFOREACH()

  #TODO:precompiled headers?!

  ############

  #
  # Build Java
  #
  if (Java_FOUND)

    INCLUDE(UseJava)

    FOREACH (DEP ${GEN_DEPENDENCIES})
      SET(include_jars ${include_jars} dots_generated-${DEP}-java)
    ENDFOREACH()

    ADD_JAR(dots_generated-${GEN_NAME}-java
      SOURCES ${java_files}
      INCLUDE_JARS dots_java ${include_jars})

  endif()

  ############



  #
  # Install everything
  #
  if (NOT GEN_NO_INSTALL)
    INSTALL(TARGETS dots_generated-${GEN_NAME}-cpp
      RUNTIME DESTINATION bin
      LIBRARY DESTINATION lib
      ARCHIVE DESTINATION lib)

    INSTALL(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/generated_code/cpp/include/ DESTINATION include
      PATTERN ".svn" EXCLUDE
      PATTERN "*~" EXCLUDE)
    
    INSTALL(FILES ${dou_files} ${dom_files} ${namespace_files}
      DESTINATION share/safir_sdk_core/${GEN_NAME})

    #TODO: install jars
    if (Java_FOUND)
      install_jar(dots_generated-${GEN_NAME}-java DESTINATION share/java/safir_sdk_core)
    endif()

  endif()
  ##############

ENDFUNCTION()


return()










