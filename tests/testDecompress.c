#include "compress.h"
#include "decompress.h"

#undef NDEBUG
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define IN_PATH "/tmp/huff_test_in"
#define ARCHIVE_PATH "/tmp/huff_test_archive"
#define OUT_PATH "/tmp/huff_test_out"

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

// Сравнивает содержимое двух файлов побайтово. Возвращает true, если файлы идентичны.
static bool filesEqual(const char* path1, const char* path2)
{
    FILE* f1 = fopen(path1, "rb");
    FILE* f2 = fopen(path2, "rb");
    if (!f1 || !f2) {
        if (f1)
            fclose(f1);
        if (f2)
            fclose(f2);
        return false;
    }

    bool equal = true;
    for (;;) {
        int c1 = fgetc(f1);
        int c2 = fgetc(f2);
        if (c1 != c2) {
            equal = false;
            break;
        }
        if (c1 == EOF)
            break;
    }

    fclose(f1);
    fclose(f2);
    return equal;
}

// Обрезает файл до newSize байт
static void truncateFile(const char* path, size_t newSize)
{
    long currentSizeLong = getFileSize(path);
    assert(currentSizeLong > 0);

    size_t currentSize = (size_t)currentSizeLong;
    assert(newSize <= currentSize);

    uint8_t* content = malloc(currentSize);
    assert(content != NULL);

    FILE* f = fopen(path, "rb");
    assert(f != NULL);
    assert(fread(content, 1, currentSize, f) == currentSize);
    fclose(f);

    f = fopen(path, "wb");
    assert(f != NULL);
    assert(fwrite(content, 1, newSize, f) == newSize);
    fclose(f);

    free(content);
}

// Неверные аргументы
static void testNullArgs(void)
{
    cleanup();
    assert(decompressFile(NULL, OUT_PATH) == false);
    assert(decompressFile(ARCHIVE_PATH, NULL) == false);
}

// Входной архив не существует
static void testMissingInput(void)
{
    cleanup();
    assert(decompressFile("/tmp/this_file_does_not_exist_123", OUT_PATH) == false);
}

// Пустой архив (8 байт нулей): разжимается в пустой файл
static void testEmptyArchive(void)
{
    cleanup();
    createFile(IN_PATH, NULL, 0);
    assert(compressFile(IN_PATH, ARCHIVE_PATH) == true);
    assert(getFileSize(ARCHIVE_PATH) == 8);

    assert(decompressFile(ARCHIVE_PATH, OUT_PATH) == true);
    assert(getFileSize(OUT_PATH) == 0);
    cleanup();
}

// Обрезанный заголовок: архив короче 8 байт
static void testTruncatedHeader(void)
{
    cleanup();
    uint8_t garbage[4] = { 0, 1, 2, 3 };
    createFile(ARCHIVE_PATH, garbage, sizeof(garbage));
    assert(decompressFile(ARCHIVE_PATH, OUT_PATH) == false);
    cleanup();
}

// Заголовок без данных: дерево прочитать невозможно
static void testCorruptedArchive(void)
{
    cleanup();
    uint8_t header[8] = { 100, 0, 0, 0, 0, 0, 0, 0 };
    createFile(ARCHIVE_PATH, header, sizeof(header));
    assert(decompressFile(ARCHIVE_PATH, OUT_PATH) == false);
    cleanup();
}

// Обрезанные данные: валидный архив урезан наполовину
static void testTruncatedData(void)
{
    cleanup();
    char data[1000];
    for (int i = 0; i < 1000; i++) {
        data[i] = (i % 2 == 0) ? 'A' : 'B';
    }
    createFile(IN_PATH, data, sizeof(data));
    assert(compressFile(IN_PATH, ARCHIVE_PATH) == true);

    long archiveSize = getFileSize(ARCHIVE_PATH);
    assert(archiveSize > 8);

    truncateFile(ARCHIVE_PATH, (size_t)(archiveSize / 2));

    assert(decompressFile(ARCHIVE_PATH, OUT_PATH) == false);
    cleanup();
}

// Roundtrip: обычный текст
static void testRoundtripText(void)
{
    cleanup();
    const char* text = "hello world, this is a simple huffman decompression test";
    createFile(IN_PATH, text, strlen(text));

    assert(compressFile(IN_PATH, ARCHIVE_PATH) == true);
    assert(decompressFile(ARCHIVE_PATH, OUT_PATH) == true);
    assert(filesEqual(IN_PATH, OUT_PATH));
    cleanup();
}

// Roundtrip: повторяющиеся байты (вырожденный случай)
static void testRoundtripRepeatedBytes(void)
{
    cleanup();
    char data[1000];
    memset(data, 'B', sizeof(data));
    createFile(IN_PATH, data, sizeof(data));

    assert(compressFile(IN_PATH, ARCHIVE_PATH) == true);
    assert(decompressFile(ARCHIVE_PATH, OUT_PATH) == true);
    assert(filesEqual(IN_PATH, OUT_PATH));
    cleanup();
}

// Выходной путь в несуществующей директории
static void testOutputDirectoryDoesNotExist(void)
{
    cleanup();
    createFile(IN_PATH, "test", 4);
    assert(compressFile(IN_PATH, ARCHIVE_PATH) == true);
    assert(decompressFile(ARCHIVE_PATH, "/nonexistent/dir/out.bin") == false);
    cleanup();
}

int main(void)
{
    testNullArgs();
    testMissingInput();
    testEmptyArchive();
    testTruncatedHeader();
    testCorruptedArchive();
    testTruncatedData();
    testRoundtripText();
    testRoundtripRepeatedBytes();
    testOutputDirectoryDoesNotExist();
    printf("All decompress tests passed.\n");
    return 0;
}