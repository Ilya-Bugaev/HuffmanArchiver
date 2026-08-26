#include "huffmanTree.h"
#include "priorityQueue.h"
#include <stdlib.h>
#include <string.h>

struct Node {
    bool leaf;
    uint8_t symbol; // значим только для листьев
    size_t frequency;
    Node* left;
    Node* right;
    size_t heapIndex; // используется только PriorityQueue
};

// --- Узлы дерева ---

Node* createLeaf(uint8_t symbol, size_t frequency)
{
    Node* node = malloc(sizeof(Node));
    if (!node) {
        return NULL;
    }
    node->leaf = true;
    node->symbol = symbol;
    node->frequency = frequency;
    node->left = NULL;
    node->right = NULL;
    node->heapIndex = (size_t)-1;
    return node;
}

Node* createInternal(Node* left, Node* right)
{
    if (!left || !right) {
        return NULL;
    }
    Node* node = malloc(sizeof(Node));
    if (!node) {
        return NULL;
    }
    node->leaf = false;
    node->symbol = 0;
    node->frequency = left->frequency + right->frequency;
    node->left = left;
    node->right = right;
    node->heapIndex = (size_t)-1;
    return node;
}

void freeNode(Node* node)
{
    if (!node) {
        return;
    }
    if (!node->leaf) {
        freeNode(node->left);
        freeNode(node->right);
    }
    free(node);
}

bool isLeaf(const Node* node)
{
    return node->leaf;
}

uint8_t getSymbol(const Node* node)
{
    return node->symbol;
}

Node* getLeft(const Node* node)
{
    return node->left;
}

Node* getRight(const Node* node)
{
    return node->right;
}

size_t getFrequency(const Node* node)
{
    return node->frequency;
}

void incrementFrequency(Node* node)
{
    node->frequency++;
}

size_t getHeapIndex(const Node* node)
{
    return node->heapIndex;
}

void setHeapIndex(Node* node, size_t index)
{
    node->heapIndex = index;
}

// --- Построение дерева ---

/* Освобождает всё, что ещё осталось в очереди, и саму очередь.
Используется при обработке ошибок в huffmanBuildTree. */
static void drainAndFreeQueue(PriorityQueue* queue)
{
    while (pqSize(queue) > 0) {
        freeNode(pqPop(queue));
    }
    pqFree(queue);
}

Node* huffmanBuildTree(const FrequencyTable frequencies)
{
    PriorityQueue* queue = pqCreate(256);
    if (!queue) {
        return NULL;
    }

    size_t distinctSymbols = 0;
    for (int symbol = 0; symbol < 256; symbol++) {
        if (frequencies[symbol] == 0) {
            continue;
        }
        Node* leaf = createLeaf((uint8_t)symbol, frequencies[symbol]);
        if (!leaf || !pqPush(queue, leaf)) {
            freeNode(leaf);
            drainAndFreeQueue(queue);
            return NULL;
        }
        distinctSymbols++;
    }

    if (distinctSymbols == 0) {
        pqFree(queue);
        return NULL;
    }

    if (distinctSymbols == 1) {
        /* Вырожденный случай: единственный уникальный байт во входных данных.
        Слияние не требуется — дерево состоит из одного узла (самого листа).
        Обычный обход не даёт для него кода; явная фиксация 1-битного кода
        для этого случая происходит в huffmanBuildCodeTable. */
        Node* only = pqPop(queue);
        pqFree(queue);
        return only;
    }

    while (pqSize(queue) > 1) {
        Node* first = pqPop(queue);
        Node* second = pqPop(queue);
        Node* parent = createInternal(first, second);
        if (!parent) {
            freeNode(first);
            freeNode(second);
            drainAndFreeQueue(queue);
            return NULL;
        }
        if (!pqPush(queue, parent)) {
            freeNode(parent);
            drainAndFreeQueue(queue);
            return NULL;
        }
    }

    Node* root = pqPop(queue);
    pqFree(queue);
    return root;
}

void huffmanFreeTree(Node* root)
{
    freeNode(root);
}

// --- Генерация таблицы кодов ---

/* Максимальная длина кода Хаффмана, которую мы готовы обработать.
Для алфавита из 256 символов этого с большим запасом достаточно на практике
(в вырожденном фибоначчиевом случае теоретический предел — 255 бит).
Это внутренний предел реализации, наружу через huffmanTree.h не выставляется. */
#define HUFFMAN_MAX_CODE_LENGTH 256

typedef struct {
    uint16_t length;
    uint8_t bits[HUFFMAN_MAX_CODE_LENGTH];
} HuffmanCode;

struct CodeTable {
    HuffmanCode codes[256];
};

static void collectCodes(const Node* node, HuffmanCode* codes, uint8_t* path, uint16_t depth, bool* success)
{
    if (!*success) {
        return;
    }

    if (node->leaf) {
        codes[node->symbol].length = depth;
        if (depth > 0) {
            memcpy(codes[node->symbol].bits, path, depth);
        }
        return;
    }

    if (depth >= HUFFMAN_MAX_CODE_LENGTH) {
        *success = false;
        return;
    }

    path[depth] = 0;
    collectCodes(node->left, codes, path, (uint16_t)(depth + 1), success);
    path[depth] = 1;
    collectCodes(node->right, codes, path, (uint16_t)(depth + 1), success);
}

CodeTable* huffmanBuildCodeTable(const Node* root)
{
    if (!root) {
        return NULL;
    }

    CodeTable* table = malloc(sizeof(CodeTable));
    if (!table) {
        return NULL;
    }

    for (int symbol = 0; symbol < 256; symbol++) {
        table->codes[symbol].length = 0;
    }

    if (root->leaf) {
        /* Вырожденный случай: дерево из одного листа (единственный уникальный
        байт во входных данных). Путь от корня до листа имеет нулевую длину,
        поэтому обычный обход не порождает кода — явно фиксируем код длиной
        1 бит (значение 0), чтобы у символа был корректный код. */
        table->codes[root->symbol].length = 1;
        table->codes[root->symbol].bits[0] = 0;
        return table;
    }

    uint8_t path[HUFFMAN_MAX_CODE_LENGTH];
    bool success = true;
    collectCodes(root, table->codes, path, 0, &success);
    if (!success) {
        free(table);
        return NULL;
    }
    return table;
}

void huffmanFreeCodeTable(CodeTable* table)
{
    free(table);
}

uint16_t getCodeLength(const CodeTable* table, uint8_t symbol)
{
    return table->codes[symbol].length;
}

int getCodeBit(const CodeTable* table, uint8_t symbol, uint16_t index)
{
    return table->codes[symbol].bits[index];
}

// --- Сериализация дерева ---

bool huffmanWriteTree(const Node* root, BitWriter* writer)
{
    if (!root || !writer) {
        return false;
    }

    if (root->leaf) {
        if (bitWriterWriteBit(writer, 1) != 0) {
            return false;
        }
        for (int i = 7; i >= 0; i--) {
            if (bitWriterWriteBit(writer, (root->symbol >> i) & 1) != 0) {
                return false;
            }
        }
        return true;
    }

    if (bitWriterWriteBit(writer, 0) != 0) {
        return false;
    }
    return huffmanWriteTree(root->left, writer) && huffmanWriteTree(root->right, writer);
}

Node* huffmanReadTree(BitReader* reader)
{
    if (!reader) {
        return NULL;
    }

    int flagBit = bitReaderReadBit(reader);
    if (flagBit < 0) {
        return NULL;
    }

    if (flagBit == 1) {
        uint8_t symbol = 0;
        for (int i = 0; i < 8; i++) {
            int bit = bitReaderReadBit(reader);
            if (bit < 0) {
                return NULL;
            }
            symbol = (uint8_t)((symbol << 1) | (bit & 1));
        }
        return createLeaf(symbol, 0);
    }

    Node* left = huffmanReadTree(reader);
    if (!left) {
        return NULL;
    }
    Node* right = huffmanReadTree(reader);
    if (!right) {
        freeNode(left);
        return NULL;
    }
    Node* node = createInternal(left, right);
    if (!node) {
        freeNode(left);
        freeNode(right);
        return NULL;
    }
    return node;
}