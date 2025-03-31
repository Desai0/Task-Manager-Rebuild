#pragma once
#include <windows.h>
#include <psapi.h>
#include <vector>
#include <algorithm>
#include <iomanip>  // ��� std::setw � std::fixed

// ��������� ��� �������� ���������� � ��������
struct ProcessInfo {
    DWORD pid;
    std::string name;
    double cpuUsage;
};

// ������� ��� �������������� FILETIME � ULARGE_INTEGER
ULARGE_INTEGER FileTimeToULargeInteger(const FILETIME& ft) {
    ULARGE_INTEGER uli;
    uli.LowPart = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;
    return uli;
}


