#include "../src/huffmanTree.h"
#include "../src/priorityQueue.h"

#undef NDEBUG
#include <assert.h>
#include <stdio.h>

// Создание и освобождение очереди с разной начальной ёмкостью
static void testCreateDestroy(void)
{
    PriorityQueue* pq = pqCreate(0);
    assert(pq != NULL);
    assert(pqSize(pq) == 0);
    pqFree(pq);

    pq = pqCreate(1);
    assert(pq != NULL);
    assert(pqSize(pq) == 0);
    pqFree(pq);

    pq = pqCreate(10);
    assert(pq != NULL);
    assert(pqSize(pq) == 0);
    pqFree(pq);

    pqFree(NULL);
}

// Вставка и извлечение одного элемента
static void testPushPopSingle(void)
{
    PriorityQueue* pq = pqCreate(4);
    assert(pq != NULL);

    Node* node = createLeaf('a', 5);
    assert(node != NULL);
    assert(pqPush(pq, node) == true);
    assert(pqSize(pq) == 1);

    Node* popped = pqPop(pq);
    assert(popped == node);
    assert(getFrequency(popped) == 5);
    assert(getSymbol(popped) == 'a');
    assert(pqSize(pq) == 0);

    freeNode(node);
    pqFree(pq);
}

// Извлечение из пустой очереди возвращает NULL
static void testEmptyPop(void)
{
    PriorityQueue* pq = pqCreate(4);
    assert(pq != NULL);
    assert(pqPop(pq) == NULL);
    pqFree(pq);
}

// Извлечение происходит в порядке возрастания частоты
static void testOrdering(void)
{
    PriorityQueue* pq = pqCreate(4);

    Node* n1 = createLeaf('a', 5);
    Node* n2 = createLeaf('b', 1);
    Node* n3 = createLeaf('c', 3);

    assert(pqPush(pq, n1));
    assert(pqPush(pq, n2));
    assert(pqPush(pq, n3));
    assert(pqSize(pq) == 3);

    assert(getFrequency(pqPop(pq)) == 1);
    assert(getFrequency(pqPop(pq)) == 3);
    assert(getFrequency(pqPop(pq)) == 5);
    assert(pqSize(pq) == 0);

    freeNode(n1);
    freeNode(n2);
    freeNode(n3);
    pqFree(pq);
}

// Размер корректно изменяется при вставках и извлечениях
static void testSize(void)
{
    PriorityQueue* pq = pqCreate(4);
    assert(pqSize(pq) == 0);

    Node* nodes[5];
    for (int i = 0; i < 5; i++) {
        nodes[i] = createLeaf((unsigned char)('a' + i), (unsigned long)(i + 1));
        assert(pqPush(pq, nodes[i]));
        assert(pqSize(pq) == (size_t)(i + 1));
    }

    for (int i = 0; i < 5; i++) {
        pqPop(pq);
        assert(pqSize(pq) == (size_t)(4 - i));
    }

    for (int i = 0; i < 5; i++) {
        freeNode(nodes[i]);
    }
    pqFree(pq);
}

// Вставка большего количества элементов, чем начальная ёмкость
static void testReallocation(void)
{
    PriorityQueue* pq = pqCreate(1);
    assert(pq != NULL);

    Node* nodes[100];
    for (int i = 0; i < 100; i++) {
        nodes[i] = createLeaf((unsigned char)(i % 256), (unsigned long)(100 - i));
        assert(pqPush(pq, nodes[i]));
    }
    assert(pqSize(pq) == 100);

    unsigned long prevFrequency = 0;
    for (int i = 0; i < 100; i++) {
        Node* node = pqPop(pq);
        assert(node != NULL);
        assert(getFrequency(node) >= prevFrequency);
        prevFrequency = getFrequency(node);
    }

    for (int i = 0; i < 100; i++) {
        freeNode(nodes[i]);
    }
    pqFree(pq);
}

// Некорректные аргументы обрабатываются без падения
static void testInvalidArgs(void)
{
    PriorityQueue* pq = pqCreate(4);
    Node* node = createLeaf('a', 5);

    assert(pqPush(NULL, node) == false);
    assert(pqPush(pq, NULL) == false);
    assert(pqSize(NULL) == 0);
    assert(pqPop(NULL) == NULL);

    freeNode(node);
    pqFree(pq);
}

// Узлы с одинаковой частотой извлекаются без ошибок
static void testSameFrequency(void)
{
    PriorityQueue* pq = pqCreate(4);

    Node* a = createLeaf('a', 5);
    Node* b = createLeaf('b', 5);
    Node* c = createLeaf('c', 5);

    assert(pqPush(pq, a));
    assert(pqPush(pq, b));
    assert(pqPush(pq, c));

    // Относительный порядок равных элементов не гарантирован, но все должны извлечься с правильной частотой
    for (int i = 0; i < 3; i++) {
        Node* node = pqPop(pq);
        assert(node != NULL);
        assert(getFrequency(node) == 5);
    }
    assert(pqSize(pq) == 0);

    freeNode(a);
    freeNode(b);
    freeNode(c);
    pqFree(pq);
}

// Увеличение частоты узла в очереди меняет порядок извлечения
static void testIncreaseFrequency(void)
{
    PriorityQueue* pq = pqCreate(4);

    Node* a = createLeaf('a', 1);
    Node* b = createLeaf('b', 2);
    Node* c = createLeaf('c', 3);

    assert(pqPush(pq, a));
    assert(pqPush(pq, b));
    assert(pqPush(pq, c));

    assert(pqIncreaseFrequency(pq, a));
    assert(pqIncreaseFrequency(pq, a));
    assert(pqIncreaseFrequency(pq, a));
    assert(getFrequency(a) == 4);

    assert(getFrequency(pqPop(pq)) == 2);
    assert(getFrequency(pqPop(pq)) == 3);
    assert(getFrequency(pqPop(pq)) == 4);

    freeNode(a);
    freeNode(b);
    freeNode(c);
    pqFree(pq);
}

// Увеличение частоты узла, не принадлежащего очереди, возвращает false
static void testIncreaseFrequencyNotInQueue(void)
{
    PriorityQueue* pq = pqCreate(4);
    Node* node = createLeaf('a', 5);

    assert(pqIncreaseFrequency(pq, node) == false);

    assert(pqPush(pq, node));
    Node* popped = pqPop(pq);
    assert(popped == node);
    assert(pqIncreaseFrequency(pq, node) == false);

    freeNode(node);
    pqFree(pq);
}

// heapIndex устанавливается при вставке и сбрасывается при извлечении
static void testHeapIndex(void)
{
    PriorityQueue* pq = pqCreate(4);
    Node* node = createLeaf('a', 5);

    assert(getHeapIndex(node) == (size_t)-1);

    assert(pqPush(pq, node));
    assert(getHeapIndex(node) < pqSize(pq));

    Node* popped = pqPop(pq);
    assert(popped == node);
    assert(getHeapIndex(node) == (size_t)-1);

    freeNode(node);
    pqFree(pq);
}

// Чередование вставок и извлечений
static void testInterleaved(void)
{
    PriorityQueue* pq = pqCreate(2);

    Node* a = createLeaf('a', 10);
    Node* b = createLeaf('b', 5);
    Node* c = createLeaf('c', 15);

    assert(pqPush(pq, a));
    assert(pqPush(pq, b));

    assert(getFrequency(pqPop(pq)) == 5);

    assert(pqPush(pq, c));

    assert(getFrequency(pqPop(pq)) == 10);
    assert(getFrequency(pqPop(pq)) == 15);

    freeNode(a);
    freeNode(b);
    freeNode(c);
    pqFree(pq);
}

// Большое количество элементов для проверки устойчивости кучи
static void testManyElements(void)
{
    PriorityQueue* pq = pqCreate(4);

    const int count = 1000;
    Node* nodes[1000];
    for (int i = 0; i < count; i++) {
        unsigned long freq = (unsigned long)((i * 7 + 13) % count);
        nodes[i] = createLeaf((unsigned char)(i % 256), freq);
        assert(pqPush(pq, nodes[i]));
    }
    assert(pqSize(pq) == (size_t)count);

    unsigned long prevFrequency = 0;
    int isFirst = 1;
    for (int i = 0; i < count; i++) {
        Node* node = pqPop(pq);
        assert(node != NULL);
        if (!isFirst) {
            assert(getFrequency(node) >= prevFrequency);
        }
        prevFrequency = getFrequency(node);
        isFirst = 0;
    }
    assert(pqSize(pq) == 0);

    for (int i = 0; i < count; i++) {
        freeNode(nodes[i]);
    }
    pqFree(pq);
}

int main(void)
{
    testCreateDestroy();
    testPushPopSingle();
    testEmptyPop();
    testOrdering();
    testSize();
    testReallocation();
    testInvalidArgs();
    testSameFrequency();
    testIncreaseFrequency();
    testIncreaseFrequencyNotInQueue();
    testHeapIndex();
    testInterleaved();
    testManyElements();
    printf("All priorityQueue tests passed.\n");
    return 0;
}
