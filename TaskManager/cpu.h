#pragma once
#include <windows.h>
#include <psapi.h>
#include <vector>
#include <algorithm>
#include <iomanip>  // Для std::setw и std::fixed

// Структура для хранения информации о процессе
struct ProcessInfo {
    DWORD pid;
    std::string name;
    double cpuUsage;
};

// Функция для преобразования FILETIME в ULARGE_INTEGER
ULARGE_INTEGER FileTimeToULargeInteger(const FILETIME& ft) {
    ULARGE_INTEGER uli;
    uli.LowPart = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;
    return uli;
}


