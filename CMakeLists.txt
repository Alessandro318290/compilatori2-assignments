# Versione minima di CMake richiesta 
cmake_minimum_required(VERSION 3.20)

# Nome del progetto
project(assignments)

# Use the same C++ standard as LLVM does
set(CMAKE_CXX_STANDARD 17 CACHE STRING "")

# LLVM is normally built without RTTI. Be consistent with that.
if(NOT LLVM_ENABLE_RTTI)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti")
endif()

# RICERCA DI LLVM nel sistema
list(APPEND CMAKE_PREFIX_PATH "${LT_LLVM_INSTALL_DIR}/lib/cmake/llvm/")

find_package(LLVM CONFIG)
if("${LLVM_VERSION_MAJOR}" VERSION_LESS 19)
  message(FATAL_ERROR "Found LLVM ${LLVM_VERSION_MAJOR}, but need LLVM 19 or above")
endif()

# Dico a CMake di includere le cartelle dove stanno gli header di LLVM (.h)
include_directories(SYSTEM ${LLVM_INCLUDE_DIRS})

# Applica le definizioni macro usate da LLVM (evita errori di linkaggio)
add_definitions(${LLVM_DEFINITIONS})

# Questa funzione abilita i comandi speciali di LLVM per creare i pass (i .so)
list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
include(AddLLVM)

# Voglio che tutti i file .so finiscano in build per comodità,
# a prescindere da dove si trova il codice sorgente.
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# CMake deve scendere nelle sottocartelle e cercare altri CMakeLists.txt.
# add_subdirectory(1Assignment)

# In caso di file dentro 'common' che devono essere compilati, 
# aggiungere anche questa (opzionale se sono solo header .h):
# add_subdirectory(common)
