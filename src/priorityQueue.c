#include "priorityQueue.h"
#include <stdlib.h>

struct PriorityQueue {
    Node** items;
    size_t size;
    size_t capacity;
};

static void pqSetItem(PriorityQueue* queue, size_t index, Node* node)
{
    queue->items[index] = node;
    setHeapIndex(node, index);
}

static void pqSwap(PriorityQueue* queue, size_t i, size_t j)
{
    Node* tmp = queue->items[i];
    pqSetItem(queue, i, queue->items[j]);
    pqSetItem(queue, j, tmp);
}

static void siftUp(PriorityQueue* queue, size_t index)
{
    while (index > 0) {
        size_t parent = (index - 1) / 2;
        if (getFrequency(queue->items[parent]) <= getFrequency(queue->items[index])) {
            break;
        }
        pqSwap(queue, parent, index);
        index = parent;
    }
}

static void siftDown(PriorityQueue* queue, size_t index)
{
    for (;;) {
        size_t smallest = index;
        size_t left = 2 * index + 1;
        size_t right = 2 * index + 2;

        if (left < queue->size && getFrequency(queue->items[left]) < getFrequency(queue->items[smallest])) {
            smallest = left;
        }
        if (right < queue->size && getFrequency(queue->items[right]) < getFrequency(queue->items[smallest])) {
            smallest = right;
        }
        if (smallest == index) {
            break;
        }
        pqSwap(queue, index, smallest);
        index = smallest;
    }
}

PriorityQueue* pqCreate(size_t initialCapacity)
{
    PriorityQueue* queue = malloc(sizeof(PriorityQueue));
    if (!queue) {
        return NULL;
    }
    if (initialCapacity == 0) {
        initialCapacity = 1;
    }
    queue->items = malloc(initialCapacity * sizeof(Node*));
    if (!queue->items) {
        free(queue);
        return NULL;
    }
    queue->size = 0;
    queue->capacity = initialCapacity;
    return queue;
}

void pqFree(PriorityQueue* queue)
{
    if (!queue) {
        return;
    }
    free(queue->items);
    free(queue);
}

bool pqPush(PriorityQueue* queue, Node* node)
{
    if (!queue || !node) {
        return false;
    }
    if (queue->size == queue->capacity) {
        size_t newCapacity = queue->capacity * 2;
        Node** newItems = realloc(queue->items, newCapacity * sizeof(Node*));
        if (!newItems) {
            return false;
        }
        queue->items = newItems;
        queue->capacity = newCapacity;
    }
    pqSetItem(queue, queue->size, node);
    queue->size++;
    siftUp(queue, queue->size - 1);
    return true;
}

Node* pqPop(PriorityQueue* queue)
{
    if (!queue || queue->size == 0) {
        return NULL;
    }
    Node* min = queue->items[0];
    queue->size--;
    if (queue->size > 0) {
        pqSetItem(queue, 0, queue->items[queue->size]);
        siftDown(queue, 0);
    }
    setHeapIndex(min, (size_t)-1);
    return min;
}

size_t pqSize(const PriorityQueue* queue)
{
    return queue ? queue->size : 0;
}

bool pqIncreaseFrequency(PriorityQueue* queue, Node* node)
{
    if (!queue || !node) {
        return false;
    }
    size_t index = getHeapIndex(node);
    if (index >= queue->size || queue->items[index] != node) {
        return false;
    }
    incrementFrequency(node);
    siftDown(queue, index);
    return true;
}
