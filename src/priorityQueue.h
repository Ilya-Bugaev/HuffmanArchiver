#pragma once

#include "huffmanTree.h"
#include <stdbool.h>
#include <stddef.h>

typedef struct PriorityQueue PriorityQueue;

// Создаёт пустую очередь с начальной ёмкостью initialCapacity (0 трактуется как 1)
PriorityQueue* pqCreate(size_t initialCapacity);

// Освобождает очередь (внутренний массив и структуру)
void pqFree(PriorityQueue* queue);

/* Добавляет узел в очередь, при необходимости увеличивая ёмкость.
Возвращает false при ошибке выделения памяти или некорректных аргументах. */
bool pqPush(PriorityQueue* queue, Node* node);

// Извлекает и возвращает узел с минимальной частотой. NULL, если очередь пуста
Node* pqPop(PriorityQueue* queue);

size_t pqSize(const PriorityQueue* queue);
