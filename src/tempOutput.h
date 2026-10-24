#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

/* Создаёт новый временный файл в каталоге outputPath.
При успехе *outFile открыт на запись, а tempPath содержит путь.
Файл назначения при этом не открывается и не изменяется. */
bool createTempOutput(const char* outputPath, char* tempPath, size_t tempPathSize, FILE** outFile);

/* Подменяет outputPath готовым временным файлом. Временный файл уже должен быть закрыт.
При ошибке временный файл удаляется, а outputPath остаётся прежним. */
bool commitTempOutput(const char* tempPath, const char* outputPath);

/* Удаляет временный файл. outputPath не трогает. */
void discardTempOutput(const char* tempPath);
