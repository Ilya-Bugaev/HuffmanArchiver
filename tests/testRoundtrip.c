#include "compress.h"
#include "decompress.h"

#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define IN_PATH "/tmp/huff_rt_in"
#define ARCHIVE_PATH "/tmp/huff_rt_archive"
#define OUT_PATH "/tmp/huff_rt_out"

static void cleanup(void)
{
    remove(IN_PATH);
    remove(ARCHIVE_PATH);
    remove(OUT_PATH);
}

static void createFile(const char* path, const void* data, size_t size)
{
    FILE* f = fopen(path, "wb");
    assert(f != NULL);
    if (size > 0) {
        fwrite(data, 1, size, f);
    }
    fclose(f);
}

static long getFileSize(const char* path)
{
    FILE* f = fopen(path, "rb");
    if (!f) {
        return -1;
    }
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return -1;
    }
    long size = ftell(f);
    fclose(f);
    return size;
}

// Побайтовое сравнение двух файлов
static bool filesEqual(const char* path1, const char* path2)
{
    long size1 = getFileSize(path1);
    long size2 = getFileSize(path2);
    if (size1 != size2) {
        return false;
    }
    if (size1 == 0) {
        return true;
    }

    FILE* f1 = fopen(path1, "rb");
    FILE* f2 = fopen(path2, "rb");
    assert(f1 != NULL);
    assert(f2 != NULL);

    bool equal = true;
    for (long i = 0; i < size1; i++) {
        int c1 = fgetc(f1);
        int c2 = fgetc(f2);
        if (c1 != c2) {
            equal = false;
            break;
        }
    }

    fclose(f1);
    fclose(f2);
    return equal;
}

// Вспомогательная функция: сжать, разжать, сравнить
static void roundtrip(const void* data, size_t size)
{
    createFile(IN_PATH, data, size);
    assert(compressFile(IN_PATH, ARCHIVE_PATH) == true);
    assert(decompressFile(ARCHIVE_PATH, OUT_PATH) == true);
    assert(filesEqual(IN_PATH, OUT_PATH) == true);
}

// Обычный текст
static void testRoundtripText(void)
{
    cleanup();
    const char* text = "hello world, this is a roundtrip test for huffman archiver";
    roundtrip(text, strlen(text));
    cleanup();
}

// Повторяющиеся байты (вырожденный случай: один уникальный символ)
static void testRoundtripRepeatedBytes(void)
{
    cleanup();
    char data[2000];
    memset(data, 'X', sizeof(data));
    roundtrip(data, sizeof(data));
    cleanup();
}

// Все 256 значений байтов
static void testRoundtripAllBytes(void)
{
    cleanup();
    uint8_t data[256];
    for (int i = 0; i < 256; i++) {
        data[i] = (uint8_t)i;
    }
    roundtrip(data, sizeof(data));
    cleanup();
}

// Пустой файл
static void testRoundtripEmpty(void)
{
    cleanup();
    roundtrip(NULL, 0);
    cleanup();
}

// Один байт
static void testRoundtripSingleByte(void)
{
    cleanup();
    uint8_t byte = 0xAB;
    roundtrip(&byte, 1);
    cleanup();
}

// Бинарные данные с нулями и 0xFF
static void testRoundtripBinary(void)
{
    cleanup();
    uint8_t data[512];
    for (int i = 0; i < 512; i++) {
        data[i] = (i % 3 == 0) ? 0x00 : (i % 3 == 1) ? 0xFF
                                                     : (uint8_t)(i % 256);
    }
    roundtrip(data, sizeof(data));
    cleanup();
}

// Большой файл (100 КБ) с псевдослучайным содержимым
static void testRoundtripLarge(void)
{
    cleanup();
    size_t size = 100 * 1024;
    uint8_t* data = malloc(size);
    assert(data != NULL);
    for (size_t i = 0; i < size; i++) {
        data[i] = (uint8_t)((i * 7 + 13) % 256);
    }
    roundtrip(data, size);
    free(data);
    cleanup();
}

int main(void)
{
    testRoundtripText();
    testRoundtripRepeatedBytes();
    testRoundtripAllBytes();
    testRoundtripEmpty();
    testRoundtripSingleByte();
    testRoundtripBinary();
    testRoundtripLarge();
    printf("All roundtrip tests passed.\n");
    return 0;
}