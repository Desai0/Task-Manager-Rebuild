// Taskm.cpp
#include "Taskm.h"
#include <TlHelp32.h>
#include <tchar.h>
#include <iostream>
#include <fstream>
#include <Psapi.h>
#include <vector> // ��� ���������� �������� PID
#include <set>    // ��� �������� ������ ������� PID
#define BYTES_IN_MB (1024.0 * 1024.0)

// �����������: �������������
Taskm::Taskm() : firstRun(true), processorCount(0) {
    // 1. �������� ���������� �����������
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    processorCount = sysInfo.dwNumberOfProcessors;
    if (processorCount <= 0) {
        processorCount = 1; // �������� �������
        std::cerr << "[Taskm] Warning: Could not get processor count, defaulting to 1." << std::endl;
    }

    // 2. �������� ��������� ��������� �����
    FILETIME ftime;
    GetSystemTimeAsFileTime(&ftime);
    memcpy(&lastSystemTime, &ftime, sizeof(FILETIME));

    // 3. �������������� ������ ��� ������ CPU
    initialize_total_cpu_query();

    std::cout << "[Taskm] Initialized. Processor Count: " << processorCount << std::endl;
}

// ����������: ������� PDH
Taskm::~Taskm() {
    if (cpuQuery) {
        PdhCloseQuery(cpuQuery);
    }
}

// ������������� PDH ������� ��� ������ CPU
void Taskm::initialize_total_cpu_query() {
    if (cpuQuery) { // ������������� ��������� �������������
        PdhCloseQuery(cpuQuery);
        cpuQuery = nullptr;
        cpuTotal = nullptr;
    }
    PDH_STATUS status = PdhOpenQueryW(NULL, NULL, &cpuQuery);
    if (status != ERROR_SUCCESS) {
        std::cerr << "[Taskm] Error: PdhOpenQueryW failed with status " << std::hex << status << std::endl;
        return; // �������, ���� �� ������� ������� ������
    }
    // ���������� L"\\Processor Information(_Total)\\% Processor Time" ��� ������������� � ������� ��������
    // ���� �� ��������, ����� ����������� L"\\Processor(_Total)\\% Processor Time"
    status = PdhAddEnglishCounterA(cpuQuery, "\\Processor(_Total)\\% Processor Time", NULL, &cpuTotal);
    if (status != ERROR_SUCCESS) {
        std::cerr << "[Taskm] Error: PdhAddEnglishCounterA failed for Processor Time with status 0x" << std::hex << status << std::endl;
        // ����������� �������� �������, ���� ������ �� ������
        status = PdhAddCounterW(cpuQuery, L"\\Processor(_Total)\\% Processor Time", NULL, &cpuTotal);
        if (status != ERROR_SUCCESS) {
            std::cerr << "[Taskm] Error: PdhAddCounterW fallback also failed with status " << std::hex << status << std::endl;
            PdhCloseQuery(cpuQuery); // ��������� ������, ���� ������� �������� �� �������
            cpuQuery = nullptr;
            return;
        }
        else {
            std::cout << "[Taskm] Warning: Using fallback counter name '\\Processor(_Total)\\% Processor Time'." << std::endl;
        }
    }
    // ������ ������ ���� ������, ����� ���������������� �������
    status = PdhCollectQueryData(cpuQuery);
    if (status != ERROR_SUCCESS) {
        std::cerr << "[Taskm] Error: Initial PdhCollectQueryData failed with status " << std::hex << status << std::endl;
        // �� ����������� ��������� ������ �����, �� ������� ����� ���� �� �����
    }
    std::cout << "[Taskm] Total CPU query initialized." << std::endl;
}


// ��������� ��������� ������ ��������
void Taskm::initialize_cpu_times(Task& task) {
    HANDLE procHandle = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, task.pid);
    if (procHandle == NULL) {
        // �� ������� ������� - ��������� ������� ��������
        return;
    }
    FILETIME ftimeCreate, ftimeExit, fKernel, fUser;
    if (GetProcessTimes(procHandle, &ftimeCreate, &ftimeExit, &fKernel, &fUser)) {
        memcpy(&task.lastKernelTime, &fKernel, sizeof(FILETIME));
        memcpy(&task.lastUserTime, &fUser, sizeof(FILETIME));
    }
    CloseHandle(procHandle);
}

// Add the missing declaration for tKernelTime at the beginning of the calculate_task_cpu method.
void Taskm::calculate_task_cpu(Task& task, ULONGLONG systemTimeDelta) {
    HANDLE procHandle = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, task.pid);
    if (procHandle == NULL) {
        task.cpuUsage = 0.0;
        return;
    }

    FILETIME ftimeCreate, ftimeExit, fKernel, fUser;
    if (GetProcessTimes(procHandle, &ftimeCreate, &ftimeExit, &fKernel, &fUser)) {
        ULARGE_INTEGER currentKernelTime, currentUserTime;
        memcpy(&currentKernelTime, &fKernel, sizeof(FILETIME));
        memcpy(&currentUserTime, &fUser, sizeof(FILETIME));

        ULONGLONG processTimeDelta = (currentKernelTime.QuadPart - task.lastKernelTime.QuadPart) +
            (currentUserTime.QuadPart - task.lastUserTime.QuadPart);

        // ���������, ��� � �������� �������, � ���������� ����������� �������
        if (systemTimeDelta > 0 && processorCount > 0) {
            // ������������ % CPU:
            // (����� �������� / (����� ����� ��������� * ���-�� ����)) * 100
            // ��� ������������: (����� �������� / ����� ����� ���������) / ���-�� ���� * 100
            task.cpuUsage = (static_cast<double>(processTimeDelta) * 100.0) /
                (static_cast<double>(systemTimeDelta) * processorCount); // <--- ��������� ������� �� processorCount
        }
        else {
            task.cpuUsage = 0.0;
        }

        task.lastKernelTime = currentKernelTime;
        task.lastUserTime = currentUserTime;

    }
    else {
        task.cpuUsage = 0.0;
    }
    CloseHandle(procHandle);

    // ������������ �������� (�� ������ ������)
    if (task.cpuUsage < 0.0) {
        task.cpuUsage = 0.0;
    }
    // ������ ������������ �������� ������ ���� ������ � 100%
    //if (task.cpuUsage > 100.0) {
    //    // ����� �������� ��� ���� ��� ���������� 100. ��������� ���������� ��-�� �������� ��������.
    //    // task.cpuUsage = 100.0;
    //}
}


// ����������� ���������� ������ update()
TASKM_ERROR Taskm::update() {
    // 1. �������� ������� ��������� �����
    FILETIME ftimeNow;
    ULARGE_INTEGER currentTime;
    GetSystemTimeAsFileTime(&ftimeNow);
    memcpy(&currentTime, &ftimeNow, sizeof(FILETIME));

    
    // 2. ������������ ��������� ��������� �����
    ULONGLONG systemTimeDelta = currentTime.QuadPart - lastSystemTime.QuadPart;

    // ���� ��� ������ ������ ��� �������� ������� ���, ���������� ������ CPU � ���� ���
    bool skipCpuCalculation = firstRun || (systemTimeDelta < 100000); // 100000 = 10ms (��������� �����)

    // 3. �������� ������� ������ ���������
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) {
        return TASKM_GENERIC_ERROR;
    }

    PROCESSENTRY32 entry;
    entry.dwSize = sizeof(PROCESSENTRY32);
    std::set<DWORD> currentPids; // ������ PID ������� ���������

    if (!Process32First(snap, &entry)) {
        CloseHandle(snap);
        return FIRSTPROCESS_ERROR;
    }

    // --- ���� ����������/���������� ---
    do {
        currentPids.insert(entry.th32ProcessID); // ��������� PID � ��� �������

        // ���� ������� � ����� �����
        auto it = processMap.find(entry.th32ProcessID);

        if (it != processMap.end()) {
            // --- ������� ��� ���������� ---
            Task& existingTask = it->second; // �������� ������ �� ������������ ������

            // ��������� ������, ������� ����� ���������� (������, ��� - ���� ��� �����)
            existingTask.pidParent = entry.th32ParentProcessID;
            // ��� ������ �� ��������, ����� �� ��������� ��� ������������������
            // existingTask.name = Taskm::wchar_to_string(entry.szExeFile);

           // �������� ������
            HANDLE procHandleMem = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, entry.th32ProcessID);
            if (procHandleMem != NULL) {
                PROCESS_MEMORY_COUNTERS_EX mem = {};
                mem.cb = sizeof(mem);
                if (GetProcessMemoryInfo(procHandleMem, (PROCESS_MEMORY_COUNTERS*)&mem, sizeof(mem))) {
                    existingTask.memory = static_cast<long double>(mem.PrivateUsage) / BYTES_IN_MB;
                }
                else {
                    existingTask.memory = 0.0; // ������ ��������� ������
                }
                CloseHandle(procHandleMem);
            }
            else {
                existingTask.memory = 0.0; // �� ������� ������� ��� ������
            }

            // ������������ CPU (���� �� ������ ������ � �������� ����������)
            if (!skipCpuCalculation) {
                calculate_task_cpu(existingTask, systemTimeDelta);
            }
            // � ������ ��� CPU ��������� 0.0

        }
        else {
            // --- ����� ������� ---
            Task newTask;
            newTask.pid = entry.th32ProcessID;
            newTask.pidParent = entry.th32ParentProcessID;
            newTask.name = Taskm::wchar_to_string(entry.szExeFile);
            newTask.cpuUsage = 0.0; // ��������� �������� CPU

            // �������� ������ ��� ������ ��������
            HANDLE procHandleMem = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, entry.th32ProcessID);
            if (procHandleMem != NULL) {
                PROCESS_MEMORY_COUNTERS_EX mem = {};
                mem.cb = sizeof(mem);
                if (GetProcessMemoryInfo(procHandleMem, (PROCESS_MEMORY_COUNTERS*)&mem, sizeof(mem))) {
                    newTask.memory = static_cast<long double>(mem.PrivateUsage) / BYTES_IN_MB;
                }
                else {
                    newTask.memory = 0.0;
                }
                CloseHandle(procHandleMem);
            }
            else {
                newTask.memory = 0.0;
            }

            // �������� ��������� ������� CPU ��� ������ ��������
            initialize_cpu_times(newTask);

            // ��������� ����� ������� � �����
            processMap.insert({ newTask.pid, newTask });
        }

    } while (Process32Next(snap, &entry));

    CloseHandle(snap);

    // --- ���� �������� ����������� ��������� ---
    for (auto it = processMap.begin(); it != processMap.end(); /* ��� ���������� ����� */) {
        // ���� PID �� ����� ����� �� ������ � ���� ������� PID...
        if (currentPids.find(it->first) == currentPids.end()) {
            // ...������ ������� ����������, ������� ��� �� �����
            it = processMap.erase(it); // erase ���������� �������� �� ��������� �������
        }
        else {
            // ����� ��������� � ���������� �������� �����
            ++it;
        }
    }

    // 4. ��������� ����� ���������� ������ ������� ��� ���������� ������
    lastSystemTime = currentTime;
    firstRun = false; // ������� ���� ������� �������

    return TASKM_OK;
}


// --- ��������� ������ (print, save_json, get_json_object � �.�.) ---

// ����� print (������ ��������� �� �����)
TASKM_ERROR Taskm::print() {
    if (processMap.empty()) {
        std::cout << "Process map is empty." << std::endl;
        return TASKM_EMPTY_ERROR; // ��� TASKM_OK
    }
    else {
        for (const auto& pair : processMap) {
            const Task& task = pair.second;
            std::cout << "Name: " << task.name << "; PID: " << task.pid << "; Parent: " << task.pidParent << "; RAM: " << task.memory << " MB" << "; CPU: " << task.cpuUsage << "%" << std::endl;
        }
        return TASKM_OK;
    }
}

// ����� get_json_object (������ ��������� �� �����)
nlohmann::json Taskm::get_json_object() {
    nlohmann::json jsonResponse;
    jsonResponse["processes"] = nlohmann::json::array();

    if (processMap.empty()) {
        return jsonResponse;
    }

    std::cout << "--- Preparing JSON Object --- Map size: " << processMap.size() << std::endl; // ��� ������� �����
    std::set<DWORD> pidsInJson; // ��� ��� �������� ���������� PID � JSON

    // --- ����������� ��� ���������� ������ (��������, ������ ������) ---
    double loggedGroupTotalMemory = 0;
    int loggedGroupCount = 0;
    const std::string debugGroupName = "������ ������.exe"; // ��� �������� ��� ���������� ����
    // --------------------------------------------------------------------

    for (const auto& pair : processMap) {
        const Task& task = pair.second;

        // �������� �� ��������� PID ����� ����������� � JSON
        if (pidsInJson.count(task.pid)) {
            std::cerr << "!!!!!! ERROR: Duplicate PID found in processMap before JSON creation: " << task.pid << std::endl;
            continue; // ���������� ���������� ���������
        }
        pidsInJson.insert(task.pid);

        // --- ����������� ��� ������������ ������ ---
        if (task.name == debugGroupName) {
            std::cout << "  [JSON Prep] PID: " << task.pid
                << ", Name: " << task.name
                << ", Memory MB: " << task.memory // ������� �������� ��� ����
                << std::endl;
            loggedGroupTotalMemory += task.memory;
            loggedGroupCount++;
        }
        // --------------------------------------------

        nlohmann::json taskObject = {
            {"pid", task.pid},
            {"pidParent", task.pidParent},
            {"name", task.name},
            {"memory", task.memory}, // ���������� ��� ����
            {"cpu", task.cpuUsage}
        };
        jsonResponse["processes"].push_back(taskObject);
    }

    // --- ������� �������� ����� ��� ������������ ������ ---
    if (loggedGroupCount > 0) {
        std::cout << "  [JSON Prep] Total Memory for '" << debugGroupName << "' (" << loggedGroupCount << " processes) before sending: "
            << loggedGroupTotalMemory << " MB" << std::endl;
    }
    std::cout << "--- Finished Preparing JSON Object --- Processes in JSON: " << jsonResponse["processes"].size() << std::endl; // ��� ���������� � JSON

    return jsonResponse;
}


// ����� save_json (������ ���������� get_json_object)
TASKM_ERROR Taskm::save_json() {
    nlohmann::json jsonFile = get_json_object(); // �������� ������� JSON

    // ���������, ���� �� �������� ����� ������ get_json_object
    if (jsonFile.is_null() || !jsonFile.contains("processes") || jsonFile["processes"].empty()) {
        std::cerr << "Warning: Attempting to save empty process list to output.json." << std::endl;
        // ����� ������� TASKM_EMPTY_ERROR ��� ������ ������� ������ ����
    }

    std::ofstream file("output.json");
    if (!file.is_open()) {
        std::cerr << "Error: Could not open output.json for writing." << std::endl;
        return TASKM_GENERIC_ERROR;
    }
    file << jsonFile.dump(4); // dump(4) ��� ��������� ������
    file.close();

    return TASKM_OK;
}

// --- ����� ������ �������� (��� ���������, �� �������� CPU_total_get) ---

// ��������� ����� �������� CPU (%)
double Taskm::CPU_total_get() {
    if (!cpuQuery || !cpuTotal) {
        std::cerr << "[Taskm] Error: Total CPU query not initialized before calling CPU_total_get()." << std::endl;
        // ����������� ���������������� �����? ��� ������ ������� ������.
        initialize_total_cpu_query(); // ������� �������������
        if (!cpuQuery || !cpuTotal) return -1.0; // ���� ��� ��� �� ����������������
    }

    PDH_FMT_COUNTERVALUE counterVal;
    PDH_STATUS status = PdhCollectQueryData(cpuQuery); // ������� ������ ������
    if (status != ERROR_SUCCESS) {
        std::cerr << "[Taskm] Error: PdhCollectQueryData failed in CPU_total_get() with status " << std::hex << status << std::endl;
        // ������ ����� ���� ������� ��� �����������
        // return -1.0; // ������� ������ ��� �������� �������� ������ ��������?
    }

    // �������� ����������������� ��������
    status = PdhGetFormattedCounterValue(cpuTotal, PDH_FMT_DOUBLE, NULL, &counterVal);
    if (status != ERROR_SUCCESS) {
        std::cerr << "[Taskm] Error: PdhGetFormattedCounterValue failed in CPU_total_get() with status " << std::hex << status << std::endl;
        return -1.0; // ������ ��������� ��������
    }

    // PDH ������ ���������� > 100% �� �������� �����, ����� ����������
    double cpuLoad = counterVal.doubleValue;
    if (cpuLoad < 0.0) return 0.0; // �� ������ ���� �������������
    // if (cpuLoad > 100.0) return 100.0; // ����������� ���������� 100%
    return cpuLoad;
}

// ��������� % �������� ������ (��� ���������)
double Taskm::get_system_memory_load_percent() {
    MEMORYSTATUSEX statex;
    statex.dwLength = sizeof(statex);
    if (GlobalMemoryStatusEx(&statex)) {
        return static_cast<double>(statex.dwMemoryLoad);
    }
    else {
        std::cerr << "[Taskm] Error GlobalMemoryStatusEx: " << GetLastError() << std::endl;
        return -1.0; // ��������� ������
    }
}

// ����������� WCHAR* � std::string (��� ���������)
std::string Taskm::wchar_to_string(WCHAR* wch) {
    if (wch == nullptr) return "";
    std::wstring wstr(wch);
    int bufferSize = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (bufferSize == 0) return "";
    std::string narrowString(bufferSize, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &narrowString[0], bufferSize, nullptr, nullptr);
    // ������� ������� ����������, ������� WideCharToMultiByte ����� �������� � ����� ������ std::string
    if (!narrowString.empty() && narrowString.back() == '\0') {
        narrowString.pop_back();
    }
    return narrowString;
}

// --- ����� get_handle() ������ �� �����, ���� ������ ��� �������� ������������� ---
// HANDLE Task::get_handle() { ... }

// --- ������ init_taskList() � update_cpuUsage(Task&) ������ �� ����� ---
// void Taskm::init_taskList() { ... }
// void Taskm::update_cpuUsage(Task & task) { ... }


// --- ������ save_json_totals � get_json_totals (��� ���������, ���������� ����� get-������) ---
TASKM_ERROR Taskm::save_json_totals() {
    //Memory
    MEMORYSTATUSEX mem;
    mem.dwLength = sizeof(MEMORYSTATUSEX);
    GlobalMemoryStatusEx(&mem); // ����� �������� �� get_system_memory_load_percent(), ���� ����� ������ %

    nlohmann::json jsonFile;
    jsonFile["totals"] = nlohmann::json::object();

    jsonFile["totals"] = {
        {"CPU_Percent", CPU_total_get()}, // ���������� % ����� �������� CPU
        {"RAM_Percent", get_system_memory_load_percent()}, // ���������� % ����� �������� RAM
        // ���� ����� ����� ���-�� RAM � MB:
        {"RAM_Total_MB", static_cast<double>(mem.ullTotalPhys) / BYTES_IN_MB }
    };

    std::ofstream file("totals_output.json");
    if (!file.is_open()) {
        std::cerr << "Error: Could not open totals_output.json for writing." << std::endl;
        return TASKM_GENERIC_ERROR;
    }
    file << jsonFile.dump(4);
    file.close();

    return TASKM_OK;
}

nlohmann::json Taskm::get_json_totals() {
    MEMORYSTATUSEX mem;
    mem.dwLength = sizeof(MEMORYSTATUSEX);
    GlobalMemoryStatusEx(&mem);

    nlohmann::json jsonFile;
    jsonFile["totals"] = {
         {"CPU_Percent", CPU_total_get()},
         {"RAM_Percent", get_system_memory_load_percent()},
         {"RAM_Total_MB", static_cast<double>(mem.ullTotalPhys) / BYTES_IN_MB }
    };

    return jsonFile;
}