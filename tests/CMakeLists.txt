cmake_minimum_required(VERSION 3.10)

add_executable(test_engine
    test_engine.cpp
    ../src/engine/engine.cpp
    ../src/engine/sm2.cpp
    ../src/engine/Levenshtein.cpp
)

target_include_directories(test_engine PRIVATE ../src)
target_compile_features(test_engine PRIVATE cxx_std_17)

add_test(NAME EngineTests COMMAND test_engine)
