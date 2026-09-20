cmake_minimum_required(VERSION 3.25)

set(PROJECT_NAME cppcms_simple)
set(THIRD_PARTY_DIR ${CMAKE_SOURCE_DIR}/third_party)

project(${PROJECT_NAME})
add_definitions(-std=c++11)

find_library(LIB_BOOSTER booster)
find_library(LIB_CPPCMS cppcms)

# set(CMAKE_CXX_FLAGS_DEBUG "-g -DDEBUG")

find_path(INC_BOOSTER booster/config.h)
find_path(INC_CPPCMS cppcms/config.h)

# Model and service code, shared by the web application and the test binary.
set(CORE_SRC
    includes/models/Person.cpp
    includes/services/PersonService.cpp
)

set(API_SRC
    main.cpp
    includes/controllers/User.cpp
    includes/controllers/Auth.cpp
    includes/controllers/Person.cpp
    includes/data/Connection.cpp
    includes/data/Master.cpp
    includes/helpers/TokenManager.cpp
    ${CORE_SRC}
)

# add_subdirectory(${THIRD_PARTY_DIR}/cppdb/lib)

include_directories(${INC_BOOSTER})
include_directories(${INC_CPPCMS})
include_directories(${CMAKE_SOURCE_DIR}/includes)
include_directories(${THIRD_PARTY_DIR}/cppdb/include)
include_directories(${THIRD_PARTY_DIR}/picojson)
link_directories(${THIRD_PARTY_DIR}/cppdb/lib)

# The prebuilt cppdb dylibs still carry the absolute install names from the
# machine they were built on, so every executable linking them needs those
# names rewritten to point at third_party. Shared by the app and the tests.
function(fix_cppdb_library_paths target)
    if(NOT APPLE)
        return()
    endif()

    set_target_properties(${target} PROPERTIES
        BUILD_RPATH "${THIRD_PARTY_DIR}/cppdb/lib"
        INSTALL_RPATH "@executable_path/../third_party/cppdb/lib"
        MACOSX_RPATH TRUE
    )

    set(STALE_PREFIX /Users/pulung/Downloads/cppcms-code-r2420-cppdb-trunk/build)
    foreach(lib libcppdb.0.dylib libcppdb_sqlite3.0.dylib libcppdb_mysql.0.dylib)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND install_name_tool -change
                ${STALE_PREFIX}/${lib}
                ${THIRD_PARTY_DIR}/cppdb/lib/${lib}
                $<TARGET_FILE:${target}>
        )
    endforeach()
endfunction()

add_executable(${PROJECT_NAME} ${API_SRC})

# Unit tests. These exercise the model and service directly and never start a
# web server, so they link cppdb but not cppcms.
set(TEST_NAME person_tests)
add_executable(${TEST_NAME} EXCLUDE_FROM_ALL
    tests/PersonTests.cpp
    ${CORE_SRC}
)
target_include_directories(${TEST_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/tests)
target_link_libraries(${TEST_NAME} cppdb)

fix_cppdb_library_paths(${TEST_NAME})

# `make check` builds and runs the suite in one step.
add_custom_target(check
    COMMAND ${TEST_NAME}
    DEPENDS ${TEST_NAME}
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running person unit tests"
)

fix_cppdb_library_paths(${PROJECT_NAME})

target_link_libraries(${PROJECT_NAME}
    ${LIB_BOOSTER}
    ${LIB_CPPCMS}
    cppdb
)