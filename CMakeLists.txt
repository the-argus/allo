cmake_minimum_required(VERSION 3.8...3.26)

project(allo CXX)

set(headers
    allo.h
    allo/block_allocator.h
    allo/c_allocator.h
    allo/heap_allocator.h
    allo/make_into.h
    allo/memory_map.h
    allo/reservation_allocator.h
    allo/scratch_allocator.h
    allo/stack_allocator.h
    allo/status.h
    allo/typed_allocation.h
    allo/typed_freeing.h
    allo/typed_reallocation.h
    allo/ctti/typename.h
    allo/ctti/detail/algorithm.h
    allo/ctti/detail/cstring.h
    allo/ctti/detail/hash.h
    allo/ctti/detail/name_filters.h
    allo/ctti/detail/pretty_function.h
    allo/structures/collection.h
    allo/detail/abstracts.h
    allo/detail/alignment.h
    allo/detail/forward_decls.h
    allo/impl/abstracts.h
    allo/impl/all.h
    allo/impl/block_allocator.h
    allo/impl/c_allocator.h
    allo/impl/heap_allocator.h
    allo/impl/reservation_allocator.h
    allo/impl/scratch_allocator.h
    allo/impl/stack_allocator.h
)

# TODO: have cmake and pkg-config config files so that it makes sense to install allo
option(ALLO_INSTALL "Generate the install target." ON)
option(ALLO_SYSTEM_HEADERS "Expose headers with marking them as system." OFF)

set(ALLO_INC_DIR ${CMAKE_INSTALL_INCLUDEDIR} CACHE STRING
    "Installation directory for include files, a relative path that "
    "will be joined with ${CMAKE_INSTALL_PREFIX} or an absolute path.")

set(ALLO_SYSTEM_HEADERS_ATTRIBUTE "")
if (ALLO_SYSTEM_HEADERS)
    set(ALLO_SYSTEM_HEADERS_ATTRIBUTE SYSTEM)
endif()

add_library(allo-header-only INTERFACE)
add_library(allo::allo-header-only ALIAS allo-header-only)
target_compile_definitions(allo-header-only INTERFACE ALLO_HEADER_ONLY=1)
target_compile_features(allo-header-only INTERFACE cxx_std_17)

target_include_directories(allo-header-only ${ALLO_SYSTEM_HEADERS_ATTRIBUTE} INTERFACE
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:${ALLO_INC_DIR}>)

if (ALLO_INSTALL)
  set(INSTALL_TARGETS allo-header-only)

  set_verbose(FMT_LIB_DIR ${CMAKE_INSTALL_LIBDIR} CACHE STRING
              "Installation directory for libraries, a relative path that "
              "will be joined to ${CMAKE_INSTALL_PREFIX} or an absolute path.")

  install(TARGETS ${INSTALL_TARGETS}
          LIBRARY DESTINATION ${FMT_LIB_DIR}
          ARCHIVE DESTINATION ${FMT_LIB_DIR}
          PUBLIC_HEADER DESTINATION "${ALLO_INC_DIR}/fmt"
          RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()
