cmake_minimum_required(VERSION 2.8)

project(siphon C)

if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

if (NOT CMAKE_INSTALL_NAME_DIR)
	set(CMAKE_INSTALL_NAME_DIR "/usr/local")
endif()

set(CMAKE_C_FLAGS "-std=c99 -Wall -Wextra -Werror -pedantic -D_BSD_SOURCE -D_GNU_SOURCE -fPIC -fvisibility=hidden")
set(CMAKE_C_FLAGS_DEBUG "-g -DSP_ALLOC_DEBUG")
set(CMAKE_C_FLAGS_RELEASE "-O3 -march=native -DNDEBUG")
set(CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELEASE} -g")
SET(CMAKE_MACOSX_RPATH ON)

set(SIPHON_VER_MAJOR 0)
set(SIPHON_VER_MINOR 2)
set(SIPHON_VER_PATCH 6)
set(SIPHON_VER_STRING ${SIPHON_VER_MAJOR}.${SIPHON_VER_MINOR}.${SIPHON_VER_PATCH})



include(CheckIncludeFiles)
include(CheckCSourceCompiles)

check_include_files("execinfo.h" SP_HAVE_EXECINFO)
check_include_files("valgrind/valgrind.h" SP_HAVE_VALGRIND)
check_c_source_compiles("#include <dirent.h>\nint main(void) { struct dirent d; return d.d_namlen; }" SP_HAVE_DIRENT_NAMLEN)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/lib/config.h.in ${CMAKE_CURRENT_BINARY_DIR}/config.h)



find_program(RAGEL_COMMAND ragel)
if(NOT RAGEL_COMMAND)
	message(FATAL_ERROR "'ragel' not found")
endif()

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/uri_parser.c
	COMMAND ${RAGEL_COMMAND} -G2 -o
		${CMAKE_CURRENT_BINARY_DIR}/uri_parser.c
		${PROJECT_SOURCE_DIR}/lib/uri_parser.rl 
	DEPENDS
		${PROJECT_SOURCE_DIR}/lib/uri_parser.rl
)

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/http_cache_control.c
	COMMAND ${RAGEL_COMMAND} -G2 -o
		${CMAKE_CURRENT_BINARY_DIR}/http_cache_control.c
		${PROJECT_SOURCE_DIR}/lib/http/cache_control.rl 
	DEPENDS
		${PROJECT_SOURCE_DIR}/lib/http/cache_control.rl
)

file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/version.h
	"#ifndef SIPHON_VER_H\n"
	"#define SIPHON_VER_H\n\n"
	"#define SP_VER_MAJOR ${SIPHON_VER_MAJOR}\n"
	"#define SP_VER_MINOR ${SIPHON_VER_MINOR}\n"
	"#define SP_VER_PATCH ${SIPHON_VER_PATCH}\n"
	"#define SP_VER (SP_VER_MAJOR*1000000 + SP_VER_MINOR*1000 + SP_VER_PATCH)\n"
	"#define SP_VER_STRING \"siphon ${SIPHON_VER_MAJOR}.${SIPHON_VER_MINOR}.${SIPHON_VER_PATCH}\"\n"
	"\n#endif\n"
)

add_library(siphon OBJECT
	lib/error.c
	lib/clock.c
	lib/common.c
	lib/seed.c
	lib/fmt.c
	lib/crc.c
	lib/hash.c
	lib/bloom.c
	lib/map.c
	lib/vec.c
	lib/trie.c
	lib/rand.c
	lib/utf8.c
	lib/http.c
	lib/json.c
	lib/msgpack.c
	lib/path.c
	lib/uri.c
	lib/alloc.c
	lib/line.c
	lib/ring.c
	${CMAKE_CURRENT_BINARY_DIR}/uri_parser.c
	${CMAKE_CURRENT_BINARY_DIR}/http_cache_control.c
)

add_library(siphon-static STATIC $<TARGET_OBJECTS:siphon>)
add_library(siphon-shared SHARED $<TARGET_OBJECTS:siphon>)

set_target_properties(siphon-static siphon-shared PROPERTIES
	OUTPUT_NAME siphon
	VERSION ${SIPHON_VER_STRING}
	SOVERSION ${SIPHON_VER_STRING}
)

include_directories(BEFORE include ${CMAKE_CURRENT_BINARY_DIR})

install(TARGETS siphon-static siphon-shared
	LIBRARY DESTINATION lib
	ARCHIVE DESTINATION lib)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/version.h
	DESTINATION include/siphon)

install(DIRECTORY include/siphon
	DESTINATION include
	FILES_MATCHING PATTERN "*.h")

install(DIRECTORY man/
	DESTINATION ${CMAKE_INSTALL_PREFIX}/share/man/man3
	FILES_MATCHING PATTERN "*.3")


add_executable(test-http-input test/http-input.c)
target_link_libraries(test-http-input siphon-static m)

add_executable(test-json-input test/json-input.c)
target_link_libraries(test-json-input siphon-static m)

add_executable(test-msgpack-input test/msgpack-input.c)
target_link_libraries(test-msgpack-input siphon-static m)

add_executable(test-uri-input test/uri-input.c)
target_link_libraries(test-uri-input siphon-static m)

add_executable(test-utf8-encode test/utf8-encode.c)
target_link_libraries(test-utf8-encode siphon-static m)

if(BUILD_TESTING)

	find_program(VALGRIND_COMMAND valgrind)

	if(VALGRIND_COMMAND)
		get_filename_component(VALGRIND_INCLUDE ${VALGRIND_COMMAND} DIRECTORY)
		get_filename_component(VALGRIND_INCLUDE ${VALGRIND_INCLUDE} DIRECTORY)
		set(VALGRIND_INCLUDE "${VALGRIND_INCLUDE}/include")
		set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -DSP_USE_VALGRIND")
		include_directories(BEFORE ${VALGRIND_INCLUDE})
	endif()

	enable_testing()

	set(MEMORYCHECK_TYPE Valgrind)
	set(VALGRIND_COMMAND_OPTIONS "-q --tool=memcheck --leak-check=full --show-reachable=yes --num-callers=50 --error-exitcode=2")
	if(APPLE)
		set(VALGRIND_COMMAND_OPTIONS "${VALGRIND_COMMAND_OPTIONS} --dsymutil=yes")
	endif()
	find_file(MEMORYCHECK_SUPPRESSIONS_FILE ${CMAKE_SYSTEM_NAME}.supp
		PATHS ${PROJECT_SOURCE_DIR}/test
		NO_DEFAULT_PATH)

	include(CTest)

	add_test(NAME common COMMAND test-common)
	add_executable(test-common test/common.c)
	target_link_libraries(test-common siphon-static m)

	add_test(NAME crc COMMAND test-crc)
	add_executable(test-crc test/crc.c)
	target_link_libraries(test-crc siphon-static m)

	add_test(NAME hash COMMAND test-hash)
	add_executable(test-hash test/hash.c)
	target_link_libraries(test-hash siphon-static m)

	add_test(NAME hashtier COMMAND test-hashtier)
	add_executable(test-hashtier test/hashtier.c)
	target_link_libraries(test-hashtier siphon-static m)

	add_test(NAME hashmap COMMAND test-hashmap)
	add_executable(test-hashmap test/hashmap.c)
	target_link_libraries(test-hashmap siphon-static m)

	add_test(NAME utf8 COMMAND test-utf8)
	add_executable(test-utf8 test/utf8.c)
	target_link_libraries(test-utf8 siphon-static m)

	add_test(NAME http COMMAND test-http)
	add_executable(test-http test/http.c)
	target_link_libraries(test-http siphon-static m)

	add_test(NAME json COMMAND test-json)
	add_executable(test-json test/json.c)
	target_link_libraries(test-json siphon-static m)

	add_test(NAME msgpack COMMAND test-msgpack)
	add_executable(test-msgpack test/msgpack.c)
	target_link_libraries(test-msgpack siphon-static m)

	add_test(NAME path COMMAND test-path)
	add_executable(test-path test/path.c)
	set_source_files_properties(test/path.c PROPERTIES
		COMPILE_DEFINITIONS PROJECT_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
	target_link_libraries(test-path siphon-static m)

	add_test(NAME uri COMMAND test-uri)
	add_executable(test-uri test/uri.c)
	target_link_libraries(test-uri siphon-static m)

	add_test(NAME bloom COMMAND test-bloom)
	add_executable(test-bloom test/bloom.c)
	target_link_libraries(test-bloom siphon-static m)

	add_test(NAME line COMMAND test-line)
	add_executable(test-line test/line.c)
	target_link_libraries(test-line siphon-static m)

	add_test(NAME error COMMAND test-error)
	add_executable(test-error test/error.c)
	target_link_libraries(test-error siphon-static m pthread)

	add_test(NAME clock COMMAND test-clock)
	add_executable(test-clock test/clock.c)
	target_link_libraries(test-clock siphon-static m)

	add_test(NAME map COMMAND test-map)
	add_executable(test-map test/map.c)
	target_link_libraries(test-map siphon-static m)

	add_test(NAME vec COMMAND test-vec)
	add_executable(test-vec test/vec.c)
	target_link_libraries(test-vec siphon-static m)

	add_test(NAME trie COMMAND test-trie)
	add_executable(test-trie test/trie.c)
	target_link_libraries(test-trie siphon-static m)

	add_test(NAME rand COMMAND test-rand)
	add_executable(test-rand test/rand.c)
	target_link_libraries(test-rand siphon-static m)

	add_test(NAME ring COMMAND test-ring)
	add_executable(test-ring test/ring.c)
	target_link_libraries(test-ring siphon-static m)

	add_test(NAME list COMMAND test-list)
	add_executable(test-list test/list.c)
	target_link_libraries(test-list siphon-static m)
endif()

