# --- Script Setup

cmake_minimum_required (VERSION 3.4.3)

# Use Debug build by default.
if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE "Debug" CACHE STRING
		"Select build type. Options are: 'Debug', 'Release', 'RelWithDebInfo' and 'MinSizeRel'."
		FORCE
	)
	message(STATUS "Set CMAKE_BUILD_TYPE to default '${CMAKE_BUILD_TYPE}'.")
endif(NOT CMAKE_BUILD_TYPE)

if("${CMAKE_SOURCE_DIR}" STREQUAL "${CMAKE_BINARY_DIR}")
	message(FATAL_ERROR "In-source builds are not allowed.")
endif("${CMAKE_SOURCE_DIR}" STREQUAL "${CMAKE_BINARY_DIR}")

# Disable in-source builds and modifications
# to the source tree.
set(CMAKE_DISABLE_SOURCE_CHANGES ON)
set(CMAKE_DISABLE_IN_SOURCE_BUILD ON)

# Enable compiler tests.
enable_testing()

project(LOCIC C CXX)

# Add path for custom modules
set(module_paths
	"${CMAKE_CURRENT_SOURCE_DIR}/cmake"
	"${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules"
	"${CMAKE_CURRENT_SOURCE_DIR}/projects/llvm-abi/cmake"
	"${CMAKE_CURRENT_SOURCE_DIR}/projects/llvm-abi/cmake/modules"
)

# Check that all the custom module paths exist. This is particularly important
# to ensure the user has pulled any relevant Git submodules.
foreach(module_path ${module_paths})
	if(NOT EXISTS "${module_path}")
		message(FATAL_ERROR "Path '${module_path}' doesn't exist.")
	endif()
endforeach()

set(CMAKE_MODULE_PATH
	${CMAKE_MODULE_PATH}
	${module_paths}
)

# --- Compiler Properties

set(LOCIC_VERSION_MAJOR 1)
set(LOCIC_VERSION_MINOR 6)
set(LOCIC_VERSION_PATCH 0)
set(LOCIC_VERSION_BUILD 0)
set(LOCIC_VERSION ${LOCIC_VERSION_MAJOR}.${LOCIC_VERSION_MINOR}.${LOCIC_VERSION_PATCH}.${LOCIC_VERSION_BUILD})

message(STATUS "Building Loci Compiler Tools version ${LOCIC_VERSION} using build type '${CMAKE_BUILD_TYPE}'.")
message(STATUS "    Source directory is '${PROJECT_SOURCE_DIR}'.")
message(STATUS "    Build directory is '${PROJECT_BINARY_DIR}'.")
message(STATUS "    Install path is '${CMAKE_INSTALL_PREFIX}' (set -DCMAKE_INSTALL_PREFIX=<path> to change).")

# --- Options

set(LOCIC_BUILD_EXAMPLES ON CACHE BOOL "Build Loci examples")

if(LOCIC_BUILD_EXAMPLES)
	message(STATUS "Building examples is ENABLED (use -DLOCIC_BUILD_EXAMPLES=OFF to disable).")
else()
	message(STATUS "Building examples is DISABLED (use -DLOCIC_BUILD_EXAMPLES=ON to enable).")
endif()

# --- Compiler Flags

add_definitions(
	-D__STDC_LIMIT_MACROS
	-D__STDC_CONSTANT_MACROS
)

# Enable most warnings.
set(LOCIC_BUILD_WARNINGS "-pedantic -Wall -Wextra -Wshadow -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings")

set(LOCIC_SHOW_EXTRA_WARNINGS OFF CACHE BOOL "Show extra warnings.")
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND LOCIC_SHOW_EXTRA_WARNINGS)
	set(LOCIC_BUILD_WARNINGS "${LOCIC_BUILD_WARNINGS} -Weverything")
	
	# Not concerned about C++98 compatibility.
	set(LOCIC_BUILD_WARNINGS "${LOCIC_BUILD_WARNINGS} -Wno-c++98-compat-pedantic")
	
	# Not that concerned about switches which don't explicitly handle every case
	# (i.e. and use a default case instead).
	set(LOCIC_BUILD_WARNINGS "${LOCIC_BUILD_WARNINGS} -Wno-switch-enum")
	
	# Not that concerned about vtables emitted in every compilation unit (they
	# should be merged when linking).
	set(LOCIC_BUILD_WARNINGS "${LOCIC_BUILD_WARNINGS} -Wno-weak-vtables")
	
	# Often very difficult to avoid padding warnings.
	set(LOCIC_BUILD_WARNINGS "${LOCIC_BUILD_WARNINGS} -Wno-padded")
endif()

# Use C++11.
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

# Add version as preprocessor defines.
add_definitions(
	"-DLOCIC_VERSION=${LOCIC_VERSION}"
	"-DLOCIC_VERSION_MAJOR=${LOCIC_VERSION_MAJOR}"
	"-DLOCIC_VERSION_MINOR=${LOCIC_VERSION_MINOR}"
	"-DLOCIC_VERSION_PATCH=${LOCIC_VERSION_PATCH}"
	"-DLOCIC_VERSION_BUILD=${LOCIC_VERSION_BUILD}"
)

# --- User config

set(LLVM_ROOT_DIR "" CACHE STRING "Set LLVM root directory.")
set(CLANG_BINARY_NAME "" CACHE STRING "Set Clang binary name.")
set(CLANG_ROOT_DIR "" CACHE STRING "Set Clang root directory.")

if(NOT "${LLVM_ROOT_DIR}" STREQUAL "")
	message(STATUS "Using LLVM_ROOT_DIR=${LLVM_ROOT_DIR}")
else(NOT "${LLVM_ROOT_DIR}" STREQUAL "")
	message(STATUS "LLVM_ROOT_DIR wasn't specified (you can use this to search for LLVM in a particular path).")
endif(NOT "${LLVM_ROOT_DIR}" STREQUAL "")

if(NOT "${CLANG_BINARY_NAME}" STREQUAL "")
	message(STATUS "Using CLANG_BINARY_NAME=${CLANG_BINARY_NAME}")
else(NOT "${CLANG_BINARY_NAME}" STREQUAL "")
	message(STATUS "CLANG_BINARY_NAME wasn't specified (you can use this to search for Clang by a particular name).")
endif(NOT "${CLANG_BINARY_NAME}" STREQUAL "")

if(NOT "${CLANG_ROOT_DIR}" STREQUAL "")
	message(STATUS "Using CLANG_ROOT_DIR=${CLANG_ROOT_DIR}")
else(NOT "${CLANG_ROOT_DIR}" STREQUAL "")
	message(STATUS "CLANG_ROOT_DIR wasn't specified (you can use this to search for Clang in a particular path).")
endif(NOT "${CLANG_ROOT_DIR}" STREQUAL "")

# --- Dependencies

set(LLVM_SUPPORTED_VERSION "9.0")

# Find LLVM (for code generation).
set(LLVM_REQ_COMPONENTS all-targets bitreader bitwriter core irreader linker mcjit nativecodegen native ipo option)
find_package(LLVM REQUIRED ${LLVM_REQ_COMPONENTS})

# Check LLVM version.
set(LLVM_VERSION_SIMPLE_STRING "${LLVM_VERSION_MAJOR}.${LLVM_VERSION_MINOR}")

if("${LLVM_VERSION_SIMPLE_STRING}" VERSION_LESS "${LLVM_SUPPORTED_VERSION}")
	message(FATAL_ERROR "LLVM version ${LLVM_VERSION_SIMPLE_STRING} is not supported (supported version is ${LLVM_SUPPORTED_VERSION}).")
elseif("${LLVM_VERSION_SIMPLE_STRING}" VERSION_GREATER "${LLVM_SUPPORTED_VERSION}")
	message(WARNING "LLVM version ${LLVM_VERSION_SIMPLE_STRING} is not officially supported for this release (supported version is ${LLVM_SUPPORTED_VERSION}).")
endif()

math(EXPR LOCIC_LLVM_VERSION ${LLVM_VERSION_MAJOR}*100+${LLVM_VERSION_MINOR})
message(STATUS "Using LLVM ${LLVM_VERSION_SIMPLE_STRING} (version integer is ${LOCIC_LLVM_VERSION}).")

add_definitions(
	-DLOCIC_LLVM_VERSION=${LOCIC_LLVM_VERSION}
)

# Also use LLVM_OBJ_ROOT here for custom builds of LLVM.
set(LOCIC_LLVM_INCLUDE_DIRS "${LLVM_INCLUDE_DIRS}" CACHE STRING "Set LLVM include directories.")
set(LOCIC_LLVM_LIBRARY_DIRS "${LLVM_LIBRARY_DIRS}" CACHE STRING "Set LLVM library directories.")

message(STATUS "    LLVM binary directory: ${LLVM_BINARY_DIR}")
message(STATUS "        llc path: ${LLVM_LLC_EXECUTABLE}")
message(STATUS "        llvm-dis path: ${LLVM_DIS_EXECUTABLE}")
message(STATUS "        llvm-link path: ${LLVM_LINK_EXECUTABLE}")
message(STATUS "        llvm-nm path: ${LLVM_NM_EXECUTABLE}")
message(STATUS "        opt path: ${LLVM_OPT_EXECUTABLE}")

message(STATUS "    LLVM include directories: ${LOCIC_LLVM_INCLUDE_DIRS}")
message(STATUS "        (if this is wrong, specify LOCIC_LLVM_INCLUDE_DIRS)")
include_directories(SYSTEM ${LOCIC_LLVM_INCLUDE_DIRS})

message(STATUS "    LLVM library directories: ${LOCIC_LLVM_LIBRARY_DIRS}")
message(STATUS "        (if this is wrong, specify LOCIC_LLVM_LIBRARY_DIRS)")
link_directories(${LOCIC_LLVM_LIBRARY_DIRS})

# Build LLVM ABI
include(LLVMABI)

# --- Subdirectories

# All headers are in the /include directory.
include_directories (
	"${CMAKE_SOURCE_DIR}/include"
)

set (STD_CONCURRENCY_API_FILES
	"${CMAKE_SOURCE_DIR}/stdlib/std/concurrency/api/condition_variable.loci"
	"${CMAKE_SOURCE_DIR}/stdlib/std/concurrency/api/fiber.loci"
	"${CMAKE_SOURCE_DIR}/stdlib/std/concurrency/api/message_queue.loci"
	"${CMAKE_SOURCE_DIR}/stdlib/std/concurrency/api/mutex.loci"
	"${CMAKE_SOURCE_DIR}/stdlib/std/concurrency/api/runnable.loci"
	"${CMAKE_SOURCE_DIR}/stdlib/std/concurrency/api/thread.loci"
)

set (STD_CONCURRENCY_BINARY_FILES
	"${CMAKE_BINARY_DIR}/stdlib/std/concurrency/std_concurrency.bc"
)

set (STD_CONCURRENCY_DEPENDENCIES
	"std.container"
	"std.event"
)

set (STD_CONTAINER_API_FILES
	"${CMAKE_SOURCE_DIR}/stdlib/std/container/api/circular_queue.loci"
	"${CMAKE_SOURCE_DIR}/stdlib/std/container/api/hash.loci"
	"${CMAKE_SOURCE_DIR}/stdlib/std/container/api/list.loci"
	"${CMAKE_SOURCE_DIR}/stdlib/std/container/api/map.loci"
	"${CMAKE_SOURCE_DIR}/stdlib/std/container/api/range.loci"
	"${CMAKE_SOURCE_DIR}/stdlib/std/container/api/set.loci"
	"${CMAKE_SOURCE_DIR}/stdlib/std/container/api/varray.loci"
)

set (STD_CONTAINER_BINARY_FILES
	"${CMAKE_BINARY_DIR}/stdlib/std/container/std_container.bc"
)

set (STD_CONTAINER_DEPENDENCIES
	"std.memory"
)

set (STD_EVENT_API_FILES
	"${CMAKE_SOURCE_DIR}/stdlib/std/event/api/generator.loci"
	"${CMAKE_SOURCE_DIR}/stdlib/std/event/api/source.loci"
	"${CMAKE_SOURCE_DIR}/stdlib/std/event/api/wait_set.loci"
)

set (STD_EVENT_BINARY_FILES
	"${CMAKE_BINARY_DIR}/stdlib/std/event/std_event.bc"
)

set (STD_EVENT_DEPENDENCIES
	"std.memory"
)

set (STD_FILESYSTEM_API_FILES
	"${CMAKE_SOURCE_DIR}/stdlib/std/filesystem/api/file_stream.loci"
	"${CMAKE_SOURCE_DIR}/stdlib/std/filesystem/api/path.loci"
)

set (STD_FILESYSTEM_BINARY_FILES
	"${CMAKE_BINARY_DIR}/stdlib/std/filesystem/std_filesystem.bc"
)

set (STD_FILESYSTEM_DEPENDENCIES
	"std.container"
	"std.memory"
	"std.string"
)

set (STD_MEMORY_API_FILES
	"${CMAKE_SOURCE_DIR}/stdlib/std/memory/api/alloc.loci"
	"${CMAKE_SOURCE_DIR}/stdlib/std/memory/api/raw.loci"
	"${CMAKE_SOURCE_DIR}/stdlib/std/memory/api/shared.loci"
	"${CMAKE_SOURCE_DIR}/stdlib/std/memory/api/unique.loci"
)

set (STD_MEMORY_BINARY_FILES
	"${CMAKE_BINARY_DIR}/stdlib/std/memory/std_memory.bc"
)

set (STD_MEMORY_DEPENDENCIES )

set (STD_NETWORK_API_FILES
	"${CMAKE_SOURCE_DIR}/stdlib/std/network/api/ip/address.loci"
	"${CMAKE_SOURCE_DIR}/stdlib/std/network/api/ip/address_v4.loci"
	"${CMAKE_SOURCE_DIR}/stdlib/std/network/api/tcp/connector.loci"
	"${CMAKE_SOURCE_DIR}/stdlib/std/network/api/tcp/endpoint.loci"
	"${CMAKE_SOURCE_DIR}/stdlib/std/network/api/tcp/listener.loci"
	"${CMAKE_SOURCE_DIR}/stdlib/std/network/api/tcp/stream.loci"
	"${CMAKE_SOURCE_DIR}/stdlib/std/network/api/tcp/sync_stream.loci"
)

set (STD_NETWORK_BINARY_FILES
	"${CMAKE_BINARY_DIR}/stdlib/std/network/std_network.bc"
)

set (STD_NETWORK_DEPENDENCIES
	"std.event"
	"std.memory"
)

set (STD_STRING_API_FILES
	"${CMAKE_SOURCE_DIR}/stdlib/std/string/api/string.loci"
	"${CMAKE_SOURCE_DIR}/stdlib/std/string/api/string_builder.loci"
	"${CMAKE_SOURCE_DIR}/stdlib/std/string/api/string_iterator.loci"
	"${CMAKE_SOURCE_DIR}/stdlib/std/string/api/string_range.loci"
	"${CMAKE_SOURCE_DIR}/stdlib/std/string/api/utf8.loci"
	"${CMAKE_SOURCE_DIR}/stdlib/std/string/api/utf8_buffer.loci"
)

set (STD_STRING_BINARY_FILES
	"${CMAKE_BINARY_DIR}/stdlib/std/string/std_string.bc"
)

set (STD_STRING_DEPENDENCIES
	"std.container"
	"std.memory"
)

set(STD_MODULES
	"concurrency"
	"container"
	"event"
	"filesystem"
	"memory"
	"network"
	"string"
)

# Compute list of standard library modules as build target dependencies.
set(STD_MODULE_DEPENDS "")
foreach(std_module ${STD_MODULES})
	set(STD_MODULE_DEPENDS ${STD_MODULE_DEPENDS} "std.${std_module}")
endforeach()

# 3rd-party library Loci bindings.
add_subdirectory(3rdparty)

# Documentation.
add_subdirectory(docs)

if(LOCIC_BUILD_EXAMPLES)
	# Loci examples that are build by the compiler.
	add_subdirectory(examples)
endif(LOCIC_BUILD_EXAMPLES)

# Compiler libraries (e.g. SemanticAnalysis).
add_subdirectory(lib)

# Language runtime.
add_subdirectory(runtime)

# Standard library.
add_subdirectory(stdlib)

# Compiler tests (e.g. that particular source files compile or don't compile).
add_subdirectory(test)

# Compiler tools (such as the 'locic' command line tool).
add_subdirectory(tools)

# Unit tests.
add_subdirectory(unittests)

# Utilities.
add_subdirectory(utils)

# Add a 'check' target that builds and tests the compiler.
add_custom_target(check
	COMMAND "${CMAKE_BINARY_DIR}/unittests/Support/locic-unittest-Support"
	COMMAND "${CMAKE_BINARY_DIR}/unittests/Lex/locic-unittest-Lex"
	COMMAND "${CMAKE_BINARY_DIR}/unittests/Parser/locic-unittest-Parser"
	COMMAND lit -sv "${CMAKE_SOURCE_DIR}/test"
	DEPENDS
		locic
		locic-unittest-Support
		locic-unittest-Lex
		locic-unittest-Parser
		${STD_MODULE_DEPENDS}
		FileCheck
	WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
)

