#include "bitStream.h"
#include "huffmanTree.h"

#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include <string.h>

// ---Вспомогательные функции---

// Рекурсивное сравнение структуры двух деревьев
static bool treesEqual(const Node* a, const Node* b)
{
    if (a == NULL && b == NULL)
        return true;
    if (a == NULL || b == NULL)
        return false;
    if (isLeaf(a) != isLeaf(b))
        return false;
    if (isLeaf(a)) {
        return getSymbol(a) == getSymbol(b);
    }
    return treesEqual(getLeft(a), getLeft(b)) && treesEqual(getRight(a), getRight(b));
}

// Проверяет, является ли код символа prefix префиксом кода символа full
static bool isPrefixOf(const CodeTable* table, uint8_t prefix, uint8_t full)
{
    uint16_t prefixLen = getCodeLength(table, prefix);
    uint16_t fullLen = getCodeLength(table, full);
    if (prefixLen == 0 || fullLen == 0 || prefixLen > fullLen) {
        return false;
    }
    for (uint16_t i = 0; i < prefixLen; i++) {
        if (getCodeBit(table, prefix, i) != getCodeBit(table, full, i)) {
            return false;
        }
    }
    return true;
}

// Заполняет таблицу частот классическим примером и строит дерево
static Node* buildClassicTree(void)
{
    FrequencyTable freq = { 0 };
    freq['a'] = 5;
    freq['b'] = 9;
    freq['c'] = 12;
    freq['d'] = 13;
    freq['e'] = 16;
    freq['f'] = 45;
    return huffmanBuildTree(freq);
}

// ---Тесты узлов---

static void testCreateLeaf(void)
{
    Node* leaf = createLeaf('A', 42);
    assert(leaf != NULL);
    assert(isLeaf(leaf) == true);
    assert(getSymbol(leaf) == 'A');
    assert(getFrequency(leaf) == 42);
    assert(getLeft(leaf) == NULL);
    assert(getRight(leaf) == NULL);
    assert(getHeapIndex(leaf) == (size_t)-1);
    freeNode(leaf);
}

static void testCreateInternal(void)
{
    Node* left = createLeaf('x', 3);
    Node* right = createLeaf('y', 7);
    Node* parent = createInternal(left, right);

    assert(parent != NULL);
    assert(isLeaf(parent) == false);
    assert(getFrequency(parent) == 10);
    assert(getLeft(parent) == left);
    assert(getRight(parent) == right);
    assert(getHeapIndex(parent) == (size_t)-1);

    freeNode(parent);
}

static void testCreateInternalWithNull(void)
{
    Node* leaf = createLeaf('x', 3);
    assert(createInternal(NULL, leaf) == NULL);
    assert(createInternal(leaf, NULL) == NULL);
    assert(createInternal(NULL, NULL) == NULL);
    freeNode(leaf);
}

static void testFreeNodeNull(void)
{
    freeNode(NULL);
}

static void testIncrementFrequency(void)
{
    Node* leaf = createLeaf('A', 5);
    assert(getFrequency(leaf) == 5);
    incrementFrequency(leaf);
    assert(getFrequency(leaf) == 6);
    incrementFrequency(leaf);
    assert(getFrequency(leaf) == 7);
    freeNode(leaf);
}

static void testHeapIndex(void)
{
    Node* leaf = createLeaf('A', 1);
    assert(getHeapIndex(leaf) == (size_t)-1);
    setHeapIndex(leaf, 7);
    assert(getHeapIndex(leaf) == 7);
    setHeapIndex(leaf, (size_t)-1);
    assert(getHeapIndex(leaf) == (size_t)-1);
    freeNode(leaf);
}

// ---Тесты построения дерева---

static void testBuildTreeEmptyFrequencies(void)
{
    FrequencyTable freq = { 0 };
    Node* root = huffmanBuildTree(freq);
    assert(root == NULL);
}

static void testBuildTreeSingleSymbol(void)
{
    FrequencyTable freq = { 0 };
    freq['z'] = 100;

    Node* root = huffmanBuildTree(freq);
    assert(root != NULL);
    assert(isLeaf(root) == true);
    assert(getSymbol(root) == 'z');
    assert(getFrequency(root) == 100);

    huffmanFreeTree(root);
}

static void testBuildTreeClassicExample(void)
{
    Node* root = buildClassicTree();
    assert(root != NULL);
    assert(isLeaf(root) == false);
    assert(getFrequency(root) == 100);
    huffmanFreeTree(root);
}

static void testBuildTreeTwoSymbols(void)
{
    FrequencyTable freq = { 0 };
    freq['a'] = 1;
    freq['b'] = 2;

    Node* root = huffmanBuildTree(freq);
    assert(root != NULL);
    assert(isLeaf(root) == false);
    assert(getFrequency(root) == 3);
    // Левый потомок — узел с меньшей частотой
    assert(isLeaf(getLeft(root)) == true);
    assert(getSymbol(getLeft(root)) == 'a');
    assert(getFrequency(getLeft(root)) == 1);
    assert(isLeaf(getRight(root)) == true);
    assert(getSymbol(getRight(root)) == 'b');
    assert(getFrequency(getRight(root)) == 2);

    huffmanFreeTree(root);
}

static void testFreeTreeNull(void)
{
    huffmanFreeTree(NULL);
}

// ---Тесты таблицы кодов---

static void testBuildCodeTableNull(void)
{
    assert(huffmanBuildCodeTable(NULL) == NULL);
}

static void testFreeCodeTableNull(void)
{
    huffmanFreeCodeTable(NULL);
}

static void testBuildCodeTableSingleLeaf(void)
{
    FrequencyTable freq = { 0 };
    freq['q'] = 50;

    Node* root = huffmanBuildTree(freq);
    assert(root != NULL);

    CodeTable* table = huffmanBuildCodeTable(root);
    assert(table != NULL);

    // Вырожденный случай: код длиной 1 бит, значение 0
    assert(getCodeLength(table, 'q') == 1);
    assert(getCodeBit(table, 'q', 0) == 0);

    // Все остальные символы отсутствуют
    for (int i = 0; i < 256; i++) {
        if (i != 'q') {
            assert(getCodeLength(table, (uint8_t)i) == 0);
        }
    }

    huffmanFreeCodeTable(table);
    huffmanFreeTree(root);
}

static void testBuildCodeTableTwoSymbols(void)
{
    FrequencyTable freq = { 0 };
    freq['a'] = 1;
    freq['b'] = 2;

    Node* root = huffmanBuildTree(freq);
    assert(root != NULL);

    CodeTable* table = huffmanBuildCodeTable(root);
    assert(table != NULL);

    // Оба символа имеют длину кода 1
    assert(getCodeLength(table, 'a') == 1);
    assert(getCodeLength(table, 'b') == 1);

    // Коды различны: один 0, другой 1
    int codeA = getCodeBit(table, 'a', 0);
    int codeB = getCodeBit(table, 'b', 0);
    assert(codeA != codeB);
    assert(codeA == 0 || codeA == 1);
    assert(codeB == 0 || codeB == 1);

    huffmanFreeCodeTable(table);
    huffmanFreeTree(root);
}

static void testBuildCodeTableClassicExample(void)
{
    Node* root = buildClassicTree();
    assert(root != NULL);

    CodeTable* table = huffmanBuildCodeTable(root);
    assert(table != NULL);

    /* Ожидаемые длины кодов для классического примера:
    f(45): 1 бит; c(12), d(13), e(16): 3 бита; a(5), b(9): 4 бита */
    assert(getCodeLength(table, 'f') == 1);
    assert(getCodeLength(table, 'c') == 3);
    assert(getCodeLength(table, 'd') == 3);
    assert(getCodeLength(table, 'e') == 3);
    assert(getCodeLength(table, 'a') == 4);
    assert(getCodeLength(table, 'b') == 4);

    // Все остальные символы отсутствуют
    for (int i = 0; i < 256; i++) {
        if (i != 'a' && i != 'b' && i != 'c' && i != 'd' && i != 'e' && i != 'f') {
            assert(getCodeLength(table, (uint8_t)i) == 0);
        }
    }

    huffmanFreeCodeTable(table);
    huffmanFreeTree(root);
}

static void testCodePrefixProperty(void)
{
    Node* root = buildClassicTree();
    assert(root != NULL);

    CodeTable* table = huffmanBuildCodeTable(root);
    assert(table != NULL);

    // Ни один код не должен быть префиксом другого
    for (int i = 0; i < 256; i++) {
        if (getCodeLength(table, (uint8_t)i) == 0)
            continue;
        for (int j = 0; j < 256; j++) {
            if (i == j)
                continue;
            if (getCodeLength(table, (uint8_t)j) == 0)
                continue;
            assert(!isPrefixOf(table, (uint8_t)i, (uint8_t)j));
        }
    }

    huffmanFreeCodeTable(table);
    huffmanFreeTree(root);
}

static void testCodeTableOptimality(void)
{
    Node* root = buildClassicTree();
    assert(root != NULL);

    CodeTable* table = huffmanBuildCodeTable(root);
    assert(table != NULL);

    // Суммарная длина закодированного сообщения должна быть оптимальной.
    // Для частот 5,9,12,13,16,45 оптимальная сумма = 224 бита.
    FrequencyTable freq = { 0 };
    freq['a'] = 5;
    freq['b'] = 9;
    freq['c'] = 12;
    freq['d'] = 13;
    freq['e'] = 16;
    freq['f'] = 45;

    unsigned long totalBits = 0;
    for (int i = 0; i < 256; i++) {
        if (freq[i] > 0) {
            totalBits += freq[i] * getCodeLength(table, (uint8_t)i);
        }
    }
    assert(totalBits == 224);

    huffmanFreeCodeTable(table);
    huffmanFreeTree(root);
}

// ---Тесты сериализации---

static void testWriteTreeNull(void)
{
    Node* leaf = createLeaf('A', 1);
    assert(leaf != NULL);

    FILE* file = tmpfile();
    assert(file != NULL);
    BitWriter writer;
    bitWriterInit(&writer, file);

    assert(huffmanWriteTree(NULL, &writer) == false);
    assert(huffmanWriteTree(leaf, NULL) == false);

    freeNode(leaf);
    fclose(file);
}

static void testReadTreeNull(void)
{
    assert(huffmanReadTree(NULL) == NULL);
}

static void testReadTreeFromEmptyStream(void)
{
    FILE* file = tmpfile();
    assert(file != NULL);

    BitReader reader;
    bitReaderInit(&reader, file);
    Node* restored = huffmanReadTree(&reader);
    assert(restored == NULL);

    fclose(file);
}

static void testSerializeSingleLeaf(void)
{
    Node* leaf = createLeaf('A', 42);
    assert(leaf != NULL);

    FILE* file = tmpfile();
    assert(file != NULL);

    BitWriter writer;
    bitWriterInit(&writer, file);
    assert(huffmanWriteTree(leaf, &writer) == true);
    assert(bitWriterFlush(&writer) == 0);

    rewind(file);

    BitReader reader;
    bitReaderInit(&reader, file);
    Node* restored = huffmanReadTree(&reader);
    assert(restored != NULL);
    assert(isLeaf(restored) == true);
    assert(getSymbol(restored) == 'A');

    freeNode(leaf);
    freeNode(restored);
    fclose(file);
}

static void testSerializeClassicExample(void)
{
    Node* root = buildClassicTree();
    assert(root != NULL);

    FILE* file = tmpfile();
    assert(file != NULL);

    BitWriter writer;
    bitWriterInit(&writer, file);
    assert(huffmanWriteTree(root, &writer) == true);
    assert(bitWriterFlush(&writer) == 0);

    rewind(file);

    BitReader reader;
    bitReaderInit(&reader, file);
    Node* restored = huffmanReadTree(&reader);
    assert(restored != NULL);

    // Структура восстановленного дерева должна совпадать с исходным
    assert(treesEqual(root, restored));

    huffmanFreeTree(root);
    huffmanFreeTree(restored);
    fclose(file);
}

static void testSerializeDeserializeCodes(void)
{
    /* Проверяем, что таблица кодов, построенная из восстановленного дерева,
    совпадает с таблицей кодов исходного дерева */
    Node* root = buildClassicTree();
    assert(root != NULL);

    FILE* file = tmpfile();
    assert(file != NULL);

    BitWriter writer;
    bitWriterInit(&writer, file);
    assert(huffmanWriteTree(root, &writer) == true);
    assert(bitWriterFlush(&writer) == 0);

    rewind(file);

    BitReader reader;
    bitReaderInit(&reader, file);
    Node* restored = huffmanReadTree(&reader);
    assert(restored != NULL);

    CodeTable* originalTable = huffmanBuildCodeTable(root);
    CodeTable* restoredTable = huffmanBuildCodeTable(restored);
    assert(originalTable != NULL);
    assert(restoredTable != NULL);

    // Длины кодов должны совпадать для всех символов
    for (int i = 0; i < 256; i++) {
        assert(getCodeLength(originalTable, (uint8_t)i) == getCodeLength(restoredTable, (uint8_t)i));
    }

    huffmanFreeCodeTable(originalTable);
    huffmanFreeCodeTable(restoredTable);
    huffmanFreeTree(root);
    huffmanFreeTree(restored);
    fclose(file);
}

// ---Тесты сериализации с вырожденными случаями---

static void testSerializeAllByteValues(void)
{
    // Дерево из всех 256 возможных байтов с частотой 1
    FrequencyTable freq = { 0 };
    for (int i = 0; i < 256; i++) {
        freq[i] = 1;
    }

    Node* root = huffmanBuildTree(freq);
    assert(root != NULL);

    FILE* file = tmpfile();
    assert(file != NULL);

    BitWriter writer;
    bitWriterInit(&writer, file);
    assert(huffmanWriteTree(root, &writer) == true);
    assert(bitWriterFlush(&writer) == 0);

    rewind(file);

    BitReader reader;
    bitReaderInit(&reader, file);
    Node* restored = huffmanReadTree(&reader);
    assert(restored != NULL);
    assert(treesEqual(root, restored));

    huffmanFreeTree(root);
    huffmanFreeTree(restored);
    fclose(file);
}

int main(void)
{
    // Узлы
    testCreateLeaf();
    testCreateInternal();
    testCreateInternalWithNull();
    testFreeNodeNull();
    testIncrementFrequency();
    testHeapIndex();

    // Построение дерева
    testBuildTreeEmptyFrequencies();
    testBuildTreeSingleSymbol();
    testBuildTreeClassicExample();
    testBuildTreeTwoSymbols();
    testFreeTreeNull();

    // Таблица кодов
    testBuildCodeTableNull();
    testFreeCodeTableNull();
    testBuildCodeTableSingleLeaf();
    testBuildCodeTableTwoSymbols();
    testBuildCodeTableClassicExample();
    testCodePrefixProperty();
    testCodeTableOptimality();

    // Сериализация
    testWriteTreeNull();
    testReadTreeNull();
    testReadTreeFromEmptyStream();
    testSerializeSingleLeaf();
    testSerializeClassicExample();
    testSerializeDeserializeCodes();
    testSerializeAllByteValues();

    printf("All huffmanTree tests passed.\n");
    return 0;
}