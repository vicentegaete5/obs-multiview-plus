cmake_minimum_required(VERSION 3.22)

# ─────────────────────────────────────────────────────────────────────────────
#  obs-multiview-plus
#  OBS Studio 32+  |  Windows (MSVC or MinGW)
# ─────────────────────────────────────────────────────────────────────────────

project(obs-multiview-plus VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Qt auto-tools ─────────────────────────────────────────────────────────────
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# ── Find OBS ──────────────────────────────────────────────────────────────────
# OBS_SOURCE_DIR and OBS_BUILD_DIR are set by the caller (see README).
# If building via the OBS plugin template, OBS CMake helpers are already
# on the module path — nothing extra is needed.

if(DEFINED OBS_SOURCE_DIR)
  # Stand-alone build: point to OBS headers/libs manually
  find_package(libobs REQUIRED
               HINTS "${OBS_BUILD_DIR}/cmake"
                     "${OBS_BUILD_DIR}")
  find_package(obs-frontend-api REQUIRED
               HINTS "${OBS_BUILD_DIR}/cmake"
                     "${OBS_BUILD_DIR}")
else()
  # Building inside OBS source tree or via cmake --preset
  find_package(libobs REQUIRED)
  find_package(obs-frontend-api REQUIRED)
endif()

# ── Find Qt6 (Qt5 fallback) ───────────────────────────────────────────────────
find_package(Qt6 QUIET COMPONENTS Core Widgets)
if(NOT Qt6_FOUND)
  find_package(Qt5 REQUIRED COMPONENTS Core Widgets)
  set(QT_LIB_PREFIX "Qt5")
else()
  set(QT_LIB_PREFIX "Qt6")
endif()

# ── Source files ──────────────────────────────────────────────────────────────
set(PLUGIN_SOURCES
    src/plugin-main.cpp
    src/multiview-plus-manager.cpp
)

set(PLUGIN_HEADERS
    src/multiview-plus.hpp
    src/multiview-plus-manager.hpp
    src/multiview-overlay.hpp
    src/scene-order-model.hpp
)

# ── Shared library (OBS plugin) ───────────────────────────────────────────────
add_library(${PROJECT_NAME} MODULE
    ${PLUGIN_SOURCES}
    ${PLUGIN_HEADERS}
)

target_include_directories(${PROJECT_NAME} PRIVATE src)

target_link_libraries(${PROJECT_NAME} PRIVATE
    OBS::libobs
    OBS::obs-frontend-api
    ${QT_LIB_PREFIX}::Core
    ${QT_LIB_PREFIX}::Widgets
)

# Windows-specific: suppress console window
if(WIN32)
  target_link_options(${PROJECT_NAME} PRIVATE /SUBSYSTEM:WINDOWS)
  set_target_properties(${PROJECT_NAME} PROPERTIES
      RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/rundir/$<CONFIG>/obs-plugins/64bit"
  )
endif()

# ── Install rules ─────────────────────────────────────────────────────────────
include(GNUInstallDirs)

install(TARGETS ${PROJECT_NAME}
    LIBRARY DESTINATION "${CMAKE_INSTALL_PREFIX}/obs-plugins/64bit"
    RUNTIME DESTINATION "${CMAKE_INSTALL_PREFIX}/obs-plugins/64bit"
)

# Locale files
install(DIRECTORY data/locale/
    DESTINATION "${CMAKE_INSTALL_PREFIX}/data/obs-plugins/${PROJECT_NAME}/locale"
)

# ── Packaging helper (optional) ───────────────────────────────────────────────
set(CPACK_PACKAGE_NAME              "obs-multiview-plus")
set(CPACK_PACKAGE_VERSION           "${PROJECT_VERSION}")
set(CPACK_PACKAGE_VENDOR            "obs-multiview-plus contributors")
set(CPACK_PACKAGE_DESCRIPTION_SHORT "Custom Multiview layouts and scene ordering for OBS Studio")
set(CPACK_GENERATOR                 "ZIP")
include(CPack)
