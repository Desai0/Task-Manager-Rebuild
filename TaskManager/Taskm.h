// Taskm.h
#pragma once

#include <windows.h>
#include <vector>
#include <string>
#include <map> // <--- �������� ��� processMap
#include "json.hpp" // ���� ������������ �����
#include <Pdh.h>   // <--- �������� ��� PDH

// Define MB_SIZE ����� ��� � Taskm.cpp ��������� (�� �� ������ �������)
#define BYTES_IN_MB (1024.0 * 1024.0)

// ���� ������ (������)
typedef enum {
    TASKM_OK = 0,
    TASKM_GENERIC_ERROR,
    FIRSTPROCESS_ERROR,
    TASKM_EMPTY_ERROR
    // ... ������ ������
} TASKM_ERROR;

// ��������� ��� �������� ������ � ��������
struct Task {
    DWORD pid = 0;
    DWORD pidParent = 0;
    std::string name = "";
    long double memory = 0.0; // � ��
    double cpuUsage = 0.0;    // � ���������

    // --- ������ ��� ������� CPU ---
    ULARGE_INTEGER lastKernelTime = {}; // ���������� ����� ����
    ULARGE_INTEGER lastUserTime = {};   // ���������� ����� ������������
    // task.processors ������ �� ����� �����
    // task.CPUlast (����� ��������� �����) ������ �� ����� �����
    // task.init ������ �� �����

    // ����������� �� ���������
    Task() = default;
};

class Taskm {
private:
    // --- ��������� ������ ��������� ����� ������������ ---
    std::map<DWORD, Task> processMap;

    // --- ������ ��� ������� ��������� CPU ---
    ULARGE_INTEGER lastSystemTime = {}; // ����� ��������� ����� ����������� ������
    int processorCount = 0;          // ���������� ���������� �����������
    bool firstRun = true;            // ���� ������� ������� update

    // --- ������ ��� ������ CPU (PDH) ---
    PDH_HQUERY cpuQuery = nullptr;
    PDH_HCOUNTER cpuTotal = nullptr;

    // --- ��������������� ������� ---
    void initialize_cpu_times(Task& task); // �������� ��������� ������� ��������
    void calculate_task_cpu(Task& task, ULONGLONG systemTimeDelta); // ���������� CPU ��� ������

    // ��������� ����� ����������� (���������)
    static std::string wchar_to_string(WCHAR* wch);

public:
    // --- ����������� � ���������� ---
    Taskm();  // ��������� ����������� ��� �������������
    ~Taskm(); // ��������� ���������� ��� ������� PDH

    // --- �������� ������ ---
    TASKM_ERROR update(); // �������� ������ � ������ ���������
    TASKM_ERROR print();
    TASKM_ERROR save_json();
    TASKM_ERROR save_json_totals();
    nlohmann::json get_json_object(); // �������� JSON �� ������� ���������
    nlohmann::json get_json_totals();

    // --- ������ ��� ����� �������� (���������) ---
    double CPU_total_get(); // ������������ �� CPU_total_init � initialize_total_cpu_query
    void initialize_total_cpu_query(); // ������������� PDH ��� ������ CPU
    double get_system_memory_load_percent(); // �������� % �������� RAM
};