#include "../src/bitStream.h"
#undef NDEBUG
#include <assert.h>
#include <stdio.h>

// Вспомогательная функция: записывает массив битов во временный файл, выполняет flush и перематывает файл в начало для последующего чтения.
static FILE* writeBits(const int* bits, size_t count)
{
    FILE* file = tmpfile();
    assert(file != NULL);

    BitWriter writer;
    bitWriterInit(&writer, file);
    for (size_t i = 0; i < count; i++) {
        assert(bitWriterWriteBit(&writer, bits[i]) == 0);
    }
    assert(bitWriterFlush(&writer) == 0);

    rewind(file);
    return file;
}

// Запись и чтение полного байта (8 бит)
static void testWriteReadFullByte(void)
{
    int bits[] = { 1, 0, 1, 1, 0, 1, 0, 0 };
    FILE* file = writeBits(bits, 8);

    BitReader reader;
    bitReaderInit(&reader, file);
    for (int i = 0; i < 8; i++) {
        assert(bitReaderReadBit(&reader) == bits[i]);
    }
    // После 8 битов должен быть конец файла
    assert(bitReaderReadBit(&reader) == -1);

    fclose(file);
}

// Запись неполного байта: 3 значащих бита + дополняющие нули
static void testWriteReadPartialByte(void)
{
    int bits[] = { 1, 0, 1 };
    FILE* file = writeBits(bits, 3);

    BitReader reader;
    bitReaderInit(&reader, file);
    for (int i = 0; i < 3; i++) {
        assert(bitReaderReadBit(&reader) == bits[i]);
    }
    // После значащих битов идут дополняющие нули
    for (int i = 0; i < 5; i++) {
        assert(bitReaderReadBit(&reader) == 0);
    }
    // После полного байта — конец файла
    assert(bitReaderReadBit(&reader) == -1);

    fclose(file);
}

// Запись нескольких байтов + неполный байт (17 битов)
static void testWriteReadMultipleBytes(void)
{
    int bits[] = { 1, 0, 1, 1, 0, 1, 0, 0, 1, 1, 1, 0, 0, 0, 1, 1, 1 };
    FILE* file = writeBits(bits, 17);

    BitReader reader;
    bitReaderInit(&reader, file);
    for (int i = 0; i < 17; i++) {
        assert(bitReaderReadBit(&reader) == bits[i]);
    }

    fclose(file);
}

// Чтение из пустого файла сразу возвращает -1
static void testReadFromEmptyFile(void)
{
    FILE* file = tmpfile();
    assert(file != NULL);

    BitReader reader;
    bitReaderInit(&reader, file);
    assert(bitReaderReadBit(&reader) == -1);

    fclose(file);
}

// После записи 3 битов и flush файл должен содержать ровно 1 байт
static void testFileSizeAfterFlush(void)
{
    int bits[] = { 1, 0, 1 };
    FILE* file = tmpfile();
    assert(file != NULL);

    BitWriter writer;
    bitWriterInit(&writer, file);
    for (int i = 0; i < 3; i++) {
        assert(bitWriterWriteBit(&writer, bits[i]) == 0);
    }
    assert(bitWriterFlush(&writer) == 0);

    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    assert(size == 1);

    fclose(file);
}

// Запись ровно 16 битов: flush не должен добавлять лишний байт
static void testFileSizeFullBytes(void)
{
    FILE* file = tmpfile();
    assert(file != NULL);

    BitWriter writer;
    bitWriterInit(&writer, file);
    for (int i = 0; i < 16; i++) {
        assert(bitWriterWriteBit(&writer, i % 2) == 0);
    }
    assert(bitWriterFlush(&writer) == 0);

    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    assert(size == 2);

    fclose(file);
}

// Flush на пустой буфер не должен ничего записывать
static void testFlushEmptyBuffer(void)
{
    FILE* file = tmpfile();
    assert(file != NULL);

    BitWriter writer;
    bitWriterInit(&writer, file);
    assert(bitWriterFlush(&writer) == 0);

    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    assert(size == 0);

    fclose(file);
}

// Чередующиеся биты 10101010
static void testAlternatingBits(void)
{
    int bits[] = { 1, 0, 1, 0, 1, 0, 1, 0 };
    FILE* file = writeBits(bits, 8);

    BitReader reader;
    bitReaderInit(&reader, file);
    for (int i = 0; i < 8; i++) {
        int expected = (i % 2 == 0) ? 1 : 0;
        assert(bitReaderReadBit(&reader) == expected);
    }

    fclose(file);
}

// Все нули и все единицы
static void testAllZerosAndAllOnes(void)
{
    int zeros[8] = { 0 };
    FILE* file1 = writeBits(zeros, 8);
    BitReader reader1;
    bitReaderInit(&reader1, file1);
    for (int i = 0; i < 8; i++) {
        assert(bitReaderReadBit(&reader1) == 0);
    }
    fclose(file1);

    int ones[8] = { 1, 1, 1, 1, 1, 1, 1, 1 };
    FILE* file2 = writeBits(ones, 8);
    BitReader reader2;
    bitReaderInit(&reader2, file2);
    for (int i = 0; i < 8; i++) {
        assert(bitReaderReadBit(&reader2) == 1);
    }
    fclose(file2);
}

int main(void)
{
    testWriteReadFullByte();
    testWriteReadPartialByte();
    testWriteReadMultipleBytes();
    testReadFromEmptyFile();
    testFileSizeAfterFlush();
    testFileSizeFullBytes();
    testFlushEmptyBuffer();
    testAlternatingBits();
    testAllZerosAndAllOnes();
    printf("All bitStream tests passed.\n");
    return 0;
}
