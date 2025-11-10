cmake_minimum_required(VERSION 3.20)

# Forcer temporairement nvcc à accepter un compilateur hôte non supporté.
# DOIT être défini avant tout appel activant CUDA (project / find_package).
if(NOT DEFINED CMAKE_CUDA_FLAGS_INIT)
  set(CMAKE_CUDA_FLAGS_INIT "--allow-unsupported-compiler" CACHE STRING "Initial CUDA flags" FORCE)
else()
  set(CMAKE_CUDA_FLAGS_INIT "${CMAKE_CUDA_FLAGS_INIT} --allow-unsupported-compiler" CACHE STRING "Initial CUDA flags" FORCE)
endif()

if(NOT DEFINED CMAKE_CUDA_FLAGS)
  set(CMAKE_CUDA_FLAGS "--allow-unsupported-compiler" CACHE STRING "CUDA flags" FORCE)
else()
  set(CMAKE_CUDA_FLAGS "${CMAKE_CUDA_FLAGS} --allow-unsupported-compiler" CACHE STRING "CUDA flags" FORCE)
endif()

project(GigaLearnBot LANGUAGES CXX)

# -----------------------------
# C++ STANDARD
# -----------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# -----------------------------
# SORTIE DES BINAIRES
# (on pose tout dans le build dir de la config)
# -----------------------------
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

# -----------------------------
# LIBTORCH CUDA 13 LOCAL
# - On privilégie libtorch-cu13 à la racine du projet
#   C:\Giga\GigaLearnCPP-Leak\libtorch-cu13
# - Ça donne un Torch_DIR propre pour les sous-projets
# -----------------------------
if (EXISTS "${PROJECT_SOURCE_DIR}/libtorch-cu13")
    message(STATUS "Using LibTorch (CUDA 13) from libtorch-cu13")
    set(Torch_DIR
        "${PROJECT_SOURCE_DIR}/libtorch-cu13/share/cmake/Torch"
        CACHE PATH "Path to Torch CMake config" FORCE
    )
elseif (EXISTS "${PROJECT_SOURCE_DIR}/libtorch")
    message(STATUS "Using LibTorch from libtorch/")
    set(Torch_DIR
        "${PROJECT_SOURCE_DIR}/libtorch/share/cmake/Torch"
        CACHE PATH "Path to Torch CMake config" FORCE
    )
else ()
    message(WARNING
        "No local libtorch-cu13 or libtorch directory found at project root. "
        "Make sure Torch_DIR is set or libtorch-cu13 is placed in the project root."
    )
endif ()

# -----------------------------
# SOURCES DU BINAIRE GigaLearnBot
# (src/ = orchestration RL, ExampleMain.cpp, etc.)
# -----------------------------
file(GLOB_RECURSE GIGALEARNBOT_SOURCES
    "src/*.cpp"
    "src/*.h"
    "src/*.hpp"
)

add_executable(GigaLearnBot ${GIGALEARNBOT_SOURCES} "src/Scenario.h")

# Assure que le linker traite bien ça comme du C++
set_target_properties(GigaLearnBot PROPERTIES
    LINKER_LANGUAGE CXX
)

target_include_directories(GigaLearnBot
    PRIVATE
        "${PROJECT_SOURCE_DIR}/src"
)

# -----------------------------
# SOUS-PROJETS
# - GigaLearnCPP : coeur RL / Torch / RocketSim
# - RLBotCPP    : bridge avec RLBot
# -----------------------------
add_subdirectory(GigaLearnCPP)
add_subdirectory(RLBotCPP)

# -----------------------------
# LIEN FINAL
# -----------------------------
target_link_libraries(GigaLearnBot
    PRIVATE
        GigaLearnCPP
        RLBotCPP
)
