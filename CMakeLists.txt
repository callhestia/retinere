cmake_minimum_required(VERSION 3.10)
project(Retinere)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(retinere
    src/main.cpp
    src/engine/engine.cpp
    src/engine/sm2.cpp
    src/storage/Flashcard.hpp
    src/engine/Levenshtein.cpp
    src/ui/menu.cpp
    src/storage/file_manager.cpp
)

enable_testing()
add_subdirectory(tests)