cmake_minimum_required(VERSION 3.10)

project(chdr VERSION 0.2 LANGUAGES C)

option(BUILD_SHARED_LIBS "Build libchdr as a shared library" OFF)
option(INSTALL_STATIC_LIBS "Install static libraries" OFF)
option(WITH_SYSTEM_ZLIB "Use system provided zlib library" OFF)
option(WITH_SYSTEM_LZMA "Use system provided lzma library" OFF)
option(WITH_SYSTEM_ZSTD "Use system provided zstd library" OFF)
option(BUILD_LTO "Compile libchdr with link-time optimization if supported" OFF)
option(BUILD_FUZZER "Build instrumented binary for fuzzing with libfuzzer, requires clang" OFF)
option(BUILD_TESTS "Build test programs" OFF) # Custom option to control tests

if(BUILD_LTO)
  include(CheckIPOSupported)
  check_ipo_supported(RESULT HAVE_IPO)
  if(HAVE_IPO)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
  endif()
endif()

if(BUILD_FUZZER)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address,fuzzer-no-link")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address,fuzzer-no-link")
  set(BUILD_SHARED_LIBS OFF)
endif()

include(GNUInstallDirs)

#-------------------------------
# dependencies
#-------------------------------

# LZMA
if (WITH_SYSTEM_LZMA)
  find_package(LZMA REQUIRED)
  list(APPEND PLATFORM_LIBS LZMA::LZMA)
else()
  if(NOT TARGET lzma)
    add_subdirectory(deps/lzma-24.05 EXCLUDE_FROM_ALL)
  endif()
  list(APPEND CHDR_LIBS lzma)
  list(APPEND CHDR_INCLUDES lzma)
endif()

# Zlib
if (WITH_SYSTEM_ZLIB)
  find_package(ZLIB REQUIRED)
  list(APPEND PLATFORM_LIBS ZLIB::ZLIB)
else()
  if(NOT TARGET zlibstatic)
    option(ZLIB_BUILD_EXAMPLES "Enable Zlib Examples" OFF)
    add_subdirectory(deps/zlib-1.3.1 EXCLUDE_FROM_ALL)
    set_target_properties(zlibstatic PROPERTIES POSITION_INDEPENDENT_CODE ON)
  endif()
  list(APPEND CHDR_LIBS zlibstatic)
endif()

# Zstd
if (WITH_SYSTEM_ZSTD)
  find_package(zstd REQUIRED)
  if(TARGET zstd::libzstd_shared)
    list(APPEND PLATFORM_LIBS zstd::libzstd_shared)
  else()
    list(APPEND PLATFORM_LIBS zstd::libzstd_static)
  endif()
else()
  if(NOT TARGET libzstd_static)
    option(ZSTD_BUILD_SHARED "BUILD SHARED LIBRARIES" OFF)
    option(ZSTD_BUILD_PROGRAMS "BUILD PROGRAMS" OFF)
    option(ZSTD_LEGACY_SUPPORT "LEGACY SUPPORT" OFF)
    add_subdirectory(deps/zstd-1.5.6/build/cmake EXCLUDE_FROM_ALL)
  endif()
  list(APPEND CHDR_LIBS libzstd_static)
endif()

#-------------------------------
# chdr library
#-------------------------------

set(CHDR_SOURCES
  src/libchdr_bitstream.c
  src/libchdr_cdrom.c
  src/libchdr_chd.c
  src/libchdr_flac.c
  src/libchdr_huffman.c
)

list(APPEND CHDR_INCLUDES ${CMAKE_CURRENT_BINARY_DIR}/include)

add_library(chdr-static STATIC ${CHDR_SOURCES})
target_include_directories(chdr-static PRIVATE ${CHDR_INCLUDES} PUBLIC include)
target_link_libraries(chdr-static PRIVATE ${CHDR_LIBS} ${PLATFORM_LIBS})

if(MSVC)
  target_compile_definitions(chdr-static PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

if (INSTALL_STATIC_LIBS)
  install(TARGETS chdr-static ${CHDR_LIBS}
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  )
endif()

if (BUILD_SHARED_LIBS)
  add_library(chdr SHARED ${CHDR_SOURCES})
  target_include_directories(chdr PRIVATE ${CHDR_INCLUDES} PUBLIC include)
  target_link_libraries(chdr PRIVATE ${CHDR_LIBS} ${PLATFORM_LIBS})

  if(MSVC)
    target_compile_definitions(chdr PUBLIC "CHD_DLL")
    target_compile_definitions(chdr PRIVATE "CHD_DLL_EXPORTS")
    target_compile_definitions(chdr PRIVATE _CRT_SECURE_NO_WARNINGS)
  elseif(APPLE)
    target_link_libraries(chdr PRIVATE -Wl,-dead_strip -Wl,-exported_symbol,_chd_*)
  elseif(CMAKE_SYSTEM_NAME STREQUAL OpenBSD)
    target_link_libraries(chdr PRIVATE -Wl,--version-script ${CMAKE_CURRENT_SOURCE_DIR}/src/link.T)
  else()
    target_link_libraries(chdr PRIVATE -Wl,--version-script ${CMAKE_CURRENT_SOURCE_DIR}/src/link.T -Wl,--no-undefined)
  endif()

  set_target_properties(chdr PROPERTIES C_VISIBILITY_PRESET hidden)
  set_target_properties(chdr PROPERTIES VISIBILITY_INLINES_HIDDEN 1)
  set_target_properties(chdr PROPERTIES PUBLIC_HEADER "include/libchdr/bitstream.h;include/libchdr/cdrom.h;include/libchdr/chd.h;include/libchdr/chdconfig.h;include/libchdr/coretypes.h;include/libchdr/flac.h;include/libchdr/huffman.h")
  set_target_properties(chdr PROPERTIES VERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}" SOVERSION ${PROJECT_VERSION_MAJOR})

  if (CMAKE_BUILD_TYPE MATCHES Release)
    #add_custom_command(TARGET chdr POST_BUILD COMMAND ${CMAKE_STRIP} libchdr.so)
  endif()

  install(TARGETS chdr
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    PUBLIC_HEADER DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/libchdr"
  )

  configure_file(pkg-config.pc.in ${CMAKE_BINARY_DIR}/libchdr.pc @ONLY)
  install(FILES ${CMAKE_BINARY_DIR}/libchdr.pc DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig")
endif()

# Only build tests if requested (default OFF; avoids subdir conflicts in superbuilds)
if(BUILD_TESTS)
  add_subdirectory(tests)
endif()
