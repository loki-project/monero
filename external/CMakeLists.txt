# Copyright (c) 2014-2018, The Monero Project
# 
# All rights reserved.
# 
# Redistribution and use in source and binary forms, with or without modification, are
# permitted provided that the following conditions are met:
# 
# 1. Redistributions of source code must retain the above copyright notice, this list of
#    conditions and the following disclaimer.
# 
# 2. Redistributions in binary form must reproduce the above copyright notice, this list
#    of conditions and the following disclaimer in the documentation and/or other
#    materials provided with the distribution.
# 
# 3. Neither the name of the copyright holder nor the names of its contributors may be
#    used to endorse or promote products derived from this software without specific
#    prior written permission.
# 
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
# THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
# STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
# THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
# 
# Parts of this file are originally copyright (c) 2012-2013 The Cryptonote developers


# We always compile if we are building statically to reduce static dependency issues...
# ...except for FreeBSD, because FreeBSD is a special case that doesn't play well with
# others.

include(FetchContent)

set(DEFAULT_WITH_MINIUPNPC ON)
if(ANDROID OR IOS)
  set(DEFAULT_WITH_MINIUPNPC OFF)
endif()

option(WITH_MINIUPNPC "Enable miniupnpc support for IGD NAT hole punching" ${DEFAULT_WITH_MINIUPNPC})

if(NOT STATIC AND NOT BUILD_STATIC_DEPS)
  find_package(PkgConfig REQUIRED)
  if(WITH_MINIUPNPC)
      pkg_check_modules(MINIUPNPC miniupnpc>=2.1 IMPORTED_TARGET)
  endif()
  pkg_check_modules(OXENMQ liboxenmq>=1.2.3 IMPORTED_TARGET)
endif()

if(NOT WITH_MINIUPNPC)
  message(STATUS "miniupnpc support disabled")
  target_compile_definitions(miniupnpc INTERFACE WITHOUT_MINIUPNPC)
elseif(MINIUPNPC_FOUND)
  message(STATUS "Found miniupnpc")
  target_link_libraries(miniupnpc INTERFACE PkgConfig::MINIUPNPC)
else()
  message(STATUS "Using in-tree miniupnpc")
  add_subdirectory(miniupnp/miniupnpc)
  if(MSVC)
    target_compile_options(libminiupnpc-static PRIVATE -wd4244 -wd4267)
  else()
    target_compile_options(libminiupnpc-static PRIVATE -Wno-undef -Wno-unused-result -Wno-unused-value)
  endif()
  if(CMAKE_SYSTEM_NAME MATCHES "NetBSD")
    target_compile_definitions(libminiupnpc-static PRIVATE _NETBSD_SOURCE)
  endif()

  target_link_libraries(miniupnpc INTERFACE libminiupnpc-static)
endif()


if(NOT OXENMQ_FOUND)
  message(STATUS "Using in-tree oxenmq")
  add_subdirectory(loki-mq)
else()
  add_library(oxenmq INTERFACE)
  target_link_libraries(oxenmq INTERFACE PkgConfig::OXENMQ)
  add_library(oxenmq::oxenmq ALIAS oxenmq)
  message(STATUS "Found liboxenmq ${OXENMQ_VERSION}")
endif()

add_subdirectory(db_drivers)
add_subdirectory(easylogging++ easyloggingpp)
add_subdirectory(randomx EXCLUDE_FROM_ALL)

# uSockets doesn't really have a proper build system (just a very simple Makefile) so build it
# ourselves.
if (NOT CMAKE_VERSION VERSION_LESS 3.12)
    set(conf_depends "CONFIGURE_DEPENDS")
else()
    set(conf_depends "")
endif()
file(GLOB usockets_src ${conf_depends} uWebSockets/uSockets/src/*.c uWebSockets/uSockets/src/eventing/*.c)
file(COPY uWebSockets/uSockets/src/libusockets.h DESTINATION uWebSockets)
add_library(uSockets STATIC EXCLUDE_FROM_ALL ${usockets_src})
target_compile_definitions(uSockets PRIVATE LIBUS_NO_SSL=1)
target_include_directories(uSockets PRIVATE uWebSockets/uSockets/src)

# On Windows uSockets uses libuv for its event loop; on Mac kqueue is the default, but that seems to
# not be reliable on older macos versions (like 10.12), so we use libuv on macos as well.
if (WIN32 OR (APPLE AND NOT IOS))
  add_subdirectory(libuv EXCLUDE_FROM_ALL)
  target_link_libraries(uSockets uv_a)
  target_compile_definitions(uSockets PUBLIC LIBUS_USE_LIBUV)
endif()


# The uWebSockets C++ layer is header-only but isn't actually prefixed in the repository itself, but
# rather only on install (which, as above, is just a very simple Makefile).  This is unfortunate
# because it means that we can't use `#include <uWebSockets/App.h>` directly with the repo; so
# instead we emulate the installation process into the build directory and include it (with the
# prefix) from there.
file(COPY uWebSockets/src/ DESTINATION uWebSockets/uWebSockets FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp")
add_library(uWebSockets INTERFACE)
target_include_directories(uWebSockets INTERFACE ${CMAKE_CURRENT_BINARY_DIR}/uWebSockets)
target_link_libraries(uWebSockets INTERFACE uSockets)
target_compile_definitions(uWebSockets INTERFACE UWS_HTTPRESPONSE_NO_WRITEMARK UWS_NO_ZLIB)


# cpr configuration.  Ideally we'd just do this via add_subdirectory, but cpr's cmake requires
# 3.15+, and we target lower than that (and this is fairly simple to build).

if(NOT BUILD_STATIC_DEPS)
  find_package(CURL REQUIRED COMPONENTS HTTP HTTPS SSL)

  # CURL::libcurl wasn't added to FindCURL until cmake 3.12, so add it if necessary
  if (CMAKE_VERSION VERSION_LESS 3.12 AND NOT TARGET CURL::libcurl)
    add_library(libcurl UNKNOWN IMPORTED GLOBAL)
    set_target_properties(libcurl PROPERTIES
      IMPORTED_LOCATION ${CURL_LIBRARIES}
      INTERFACE_INCLUDE_DIRECTORIES "${CURL_INCLUDE_DIRS}")
    add_library(CURL_libcurl INTERFACE)
    target_link_libraries(CURL_libcurl INTERFACE libcurl)
    add_library(CURL::libcurl ALIAS CURL_libcurl)
  endif()
endif()

file(GLOB cpr_sources ${conf_depends} cpr/cpr/*.cpp)

add_library(cpr STATIC EXCLUDE_FROM_ALL ${cpr_sources})
target_link_libraries(cpr PUBLIC CURL::libcurl)
target_include_directories(cpr PUBLIC cpr/include)
target_compile_definitions(cpr PUBLIC CPR_CURL_NOSIGNAL)
add_library(cpr::cpr ALIAS cpr)



#if(NOT BUILD_STATIC_DEPS) # Under BUILD_STATIC_DEPS the SQLite::SQLite3 target is already set up
  # We require 3.35.x for some queries we use (such as "RETURNING"), and 3.35.5 for bug fixes, but
  # that release is very new as of this writing so we build a static release if the system one is
  # too old.
#pkg_check_modules(SQLite3 sqlite3>=3.35.5 IMPORTED_TARGET)

#if(SQLite3_FOUND)
  #add_library(SQLite::SQLite3 ALIAS PkgConfig::SQLite3)
#else()
  #include(ExternalProject)
  #unset(SQLITE3_VERSION CACHE)
  #include(sqlite3_source)

  #set(sqlite_urls)
  #foreach(mirror ${SQLITE3_MIRROR})
    #list(APPEND sqlite_urls "${mirror}/${SQLITE3_SOURCE}")
  #endforeach()

  #set(deps_cc "${CMAKE_C_COMPILER}")
  #if(CMAKE_C_COMPILER_LAUNCHER)
    #set(deps_cc "${CMAKE_C_COMPILER_LAUNCHER} ${deps_cc}")
  #endif()
  #set(deps_CFLAGS "-O2")
  #if(USE_LTO AND NOT CMAKE_C_COMPILER_ID STREQUAL "Clang")
    ## For unknown reasons, clang has trouble linking sqlite3 if we build it with lto
    #set(deps_CFLAGS "${deps_CFLAGS} -flto")
  #endif()

  #file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/sqlite3/include ${CMAKE_CURRENT_BINARY_DIR}/sqlite3/lib)
  #ExternalProject_Add(sqlite3_external
    #BUILD_IN_SOURCE ON
    #PREFIX ${CMAKE_CURRENT_BINARY_DIR}/sqlite3
    #URL ${sqlite_urls}
    #URL_HASH ${SQLITE3_HASH}
    #DOWNLOAD_NO_PROGRESS ON
    #CONFIGURE_COMMAND ./configure --disable-shared --prefix=${CMAKE_CURRENT_BINARY_DIR}/sqlite3 --with-pic "CC=${deps_cc}" "CFLAGS=${deps_CFLAGS}"
    #BUILD_COMMAND true
    #INSTALL_COMMAND make install-includeHEADERS install-libLTLIBRARIES
    #BUILD_BYPRODUCTS ${CMAKE_CURRENT_BINARY_DIR}/sqlite3/lib/libsqlite3.a ${CMAKE_CURRENT_BINARY_DIR}/sqlite3/include/sqlite3.h
    #)
  #add_library(SQLite::SQLite3 STATIC IMPORTED GLOBAL)
  #add_dependencies(SQLite::SQLite3 sqlite3_external)
  #set_target_properties(SQLite::SQLite3 PROPERTIES IMPORTED_LOCATION ${CMAKE_CURRENT_BINARY_DIR}/sqlite3/lib/libsqlite3.a)
  #target_include_directories(SQLite::SQLite3 INTERFACE ${CMAKE_CURRENT_BINARY_DIR}/sqlite3/include)
#endif()

## Hack around SQLiteCpp's attempts to locate sqlite3 because we *don't* want to link against the
## system one, but don't download and build the embedded one until build time.  Thankfully it
## actually links against the SQLite::SQLite3 cmake target if it already exists, so all we have to do
## is set that up and circumvent some of the non-target bits of its FindSQLite3.cmake.
#set(SQLite3_FOUND TRUE CACHE BOOL "" FORCE)
#file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/ignored")
#file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/ignored/sqlite3.h" "#define SQLITE_VERSION \"${SQLite3_VERSION}\"")
#set(SQLite3_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/ignored" CACHE STRING "" FORCE)
#set(SQLite3_LIBRARY "ignored" CACHE STRING "" FORCE)
#set(SQLITECPP_INTERNAL_SQLITE OFF CACHE BOOL "don't build SQLiteCpp's internal sqlite3" FORCE)
#set(SQLITE_ENABLE_COLUMN_METADATA OFF CACHE BOOL "" FORCE)
#set(SQLITECPP_RUN_CPPLINT OFF CACHE BOOL "" FORCE)
#set(SQLITECPP_RUN_CPPCHECK OFF CACHE BOOL "" FORCE)
#set(SQLITECPP_RUN_DOXYGEN OFF CACHE BOOL "" FORCE)
#set(SQLITECPP_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
#set(SQLITECPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)

add_subdirectory(SQLiteCpp)
