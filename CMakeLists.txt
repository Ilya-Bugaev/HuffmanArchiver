cmake_minimum_required(VERSION 3.16)
project(huffman C)

#Стандарт языка
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

#Флаги компиляции
add_compile_options(-Wall -Wextra -Wpedantic)

#Библиотека с модулями архиватора
add_library(huffman_lib
    src/huffmanTree.c
    src/priorityQueue.c
    src/bitStream.c
    src/compress.c
    src/decompress.c
)
target_include_directories(huffman_lib PUBLIC src)

#Основное приложение
add_executable(huffman src/main.c)
target_link_libraries(huffman PRIVATE huffman_lib)

#Тесты
enable_testing()

add_executable(testTree tests/testTree.c)
target_link_libraries(testTree PRIVATE huffman_lib)
add_test(NAME testTree COMMAND testTree)

add_executable(testPriorityQueue tests/testPriorityQueue.c)
target_link_libraries(testPriorityQueue PRIVATE huffman_lib)
add_test(NAME testPriorityQueue COMMAND testPriorityQueue)

add_executable(testBitStream tests/testBitStream.c)
target_link_libraries(testBitStream PRIVATE huffman_lib)
add_test(NAME testBitStream COMMAND testBitStream)

add_executable(testCompress tests/testCompress.c)
target_link_libraries(testCompress PRIVATE huffman_lib)
add_test(NAME testCompress COMMAND testCompress)

add_executable(testDecompress tests/testDecompress.c)
target_link_libraries(testDecompress PRIVATE huffman_lib)
add_test(NAME testDecompress COMMAND testDecompress)

add_executable(testRoundtrip tests/testRoundtrip.c)
target_link_libraries(testRoundtrip PRIVATE huffman_lib)
add_test(NAME testRoundtrip COMMAND testRoundtrip)
