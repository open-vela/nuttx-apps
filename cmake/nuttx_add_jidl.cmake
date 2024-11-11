# ##############################################################################
# cmake/nuttx_add_jidl.cmake
#
# Licensed to the Apache Software Foundation (ASF) under one or more contributor
# license agreements.  See the NOTICE file distributed with this work for
# additional information regarding copyright ownership.  The ASF licenses this
# file to you under the Apache License, Version 2.0 (the "License"); you may not
# use this file except in compliance with the License.  You may obtain a copy of
# the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
# License for the specific language governing permissions and limitations under
# the License.
#
# ##############################################################################

include(nuttx_parse_function_args)

# ~~~
# nuttx_add_jidl
#
# Description:
#   Generate source code files using JIDL and add them to the given target
#
# Example:
#  nuttx_add_jidl(
#    TARGET
#    libfeature
#    JIDL_SCRIPT
#    ${FEATURE_TOP}/tools/jidl/jsongensource.py
#    JIDL_OUT_DIR
#    ${CMAKE_CURRENT_BINARY_DIR}
#    JIDLS
#    test.jidl
#    JIDL_FLAGS
#    --lang=cpp
#    OUT_SRC_EXT
#    cpp)
# ~~~

function(nuttx_add_jidl)

  # parse arguments into variables

  nuttx_parse_function_args(
    FUNC
    nuttx_add_jidl
    ONE_VALUE
    TARGET
    JIDL_SCRIPT
    JIDL_OUT_DIR
    MULTI_VALUE
    JIDLS
    JIDL_FLAGS
    OUT_SRC_EXT
    REQUIRED
    TARGET
    JIDL_SCRIPT
    JIDL_OUT_DIR
    JIDLS
    ARGN
    ${ARGN})

  if(NOT OUT_SRC_EXT)
    set(OUT_SRC_EXT "cpp")
  endif()

  foreach(JIDL_PATH ${JIDLS})
    get_filename_component(JIDL_NAME ${JIDL_PATH} NAME_WE)
    set(JIDL_SRC ${JIDL_OUT_DIR}/${JIDL_NAME}.${OUT_SRC_EXT})
    set(JIDL_HEADER ${JIDL_OUT_DIR}/${JIDL_NAME}.h)
    file(WRITE ${JIDL_SRC} )
    file(WRITE ${JIDL_HEADER} )

    set(JIDL_TARGET jidl_${JIDL_NAME}_target)
    add_custom_target(
      ${JIDL_TARGET}
      COMMAND
        python3 ${JIDL_SCRIPT} ${JIDL_PATH} ${JIDL_FLAGS} -out-dir
        ${JIDL_OUT_DIR} -header ${JIDL_NAME}.h -source
        ${JIDL_NAME}.${OUT_SRC_EXT}
      WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
      COMMENT "JIDL: generating glue files for ${JIDL_NAME}.jidl")

    add_dependencies(${TARGET} ${JIDL_TARGET})
    # make a link list dep of jidl targets
    get_property(DEP_JIDL_TARGET GLOBAL PROPERTY GLOBAL_JIDL_TARGET)
    if(DEP_JIDL_TARGET)
      add_dependencies(${JIDL_TARGET} ${DEP_JIDL_TARGET})
    endif()
    set_property(GLOBAL PROPERTY GLOBAL_JIDL_TARGET ${JIDL_TARGET})
    target_sources(${TARGET} PRIVATE ${JIDL_SRC})
  endforeach()

  target_include_directories(${TARGET} PRIVATE ${JIDL_OUT_DIR})

endfunction()
