#include "compress.h"

#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define IN_PATH "/tmp/huff_test_in"
#define OUT_PATH "/tmp/huff_test_out"

static void cleanup(void)
{
    remove(IN_PATH);
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

// Размер файла в байтах, -1 при ошибке открытия
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

// Неверные аргументы
static void testNullArgs(void)
{
    cleanup();
    assert(compressFile(NULL, OUT_PATH) == false);
    assert(compressFile(IN_PATH, NULL) == false);
}

// Входной файл не существует
static void testMissingInput(void)
{
    cleanup();
    assert(compressFile("/tmp/this_file_does_not_exist_123", OUT_PATH) == false);
}

/* Пустой файл: архив должен содержать ровно 8-байтный заголовок (размер 0),
без дерева и данных. */
static void testEmptyFile(void)
{
    cleanup();
    createFile(IN_PATH, NULL, 0);
    assert(compressFile(IN_PATH, OUT_PATH) == true);
    assert(getFileSize(OUT_PATH) == 8);
    cleanup();
}

// Обычный текст: архив должен реально содержать заголовок + дерево + данные
static void testNormalText(void)
{
    cleanup();
    const char* text = "hello world, this is a simple huffman compression test";
    createFile(IN_PATH, text, strlen(text));
    assert(compressFile(IN_PATH, OUT_PATH) == true);
    assert(getFileSize(OUT_PATH) > 8);
    cleanup();
}

// Повторяющиеся байты (вырожденный случай: дерево из одного листа).
static void testRepeatedBytes(void)
{
    cleanup();
    char data[1000];
    memset(data, 'A', sizeof(data));
    createFile(IN_PATH, data, sizeof(data));
    assert(compressFile(IN_PATH, OUT_PATH) == true);
    long compressedSize = getFileSize(OUT_PATH);
    assert(compressedSize > 8);
    assert(compressedSize < 200);
    cleanup();
}

/* Входной и выходной пути совпадают: compressFile должна успешно отработать,
так как текущая реализация читает весь входной файл в память до открытия
выходного файла на запись. */
static void testSamePath(void)
{
    cleanup();
    const char* text = "same path in and out, should still work correctly";
    createFile(IN_PATH, text, strlen(text));
    assert(compressFile(IN_PATH, IN_PATH) == true);
    assert(getFileSize(IN_PATH) > 8);
    cleanup();
}

int main(void)
{
    testNullArgs();
    testMissingInput();
    testEmptyFile();
    testNormalText();
    testRepeatedBytes();
    testSamePath();

    printf("All compress tests passed.\n");
    return 0;
}
