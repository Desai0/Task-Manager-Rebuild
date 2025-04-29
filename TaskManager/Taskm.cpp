// Taskm.cpp
#include "Taskm.h" // ����������� �������� ��������� ������

#include <TlHelp32.h> // �������� �����, �.�. ����� ��� update
#include <tchar.h>    // �������� ����� (���� �����)
#include <iostream>   // �������� �����, �.�. ����� ��� print
#include <fstream>    // �������� �����, �.�. ����� ��� save_json
#include <Psapi.h>    // �������� �����, �.�. ����� ��� update (GetProcessMemoryInfo)
#include "json.hpp"


// Method returning process handle, needed for cpu usage tracking, probably
HANDLE Task::get_handle()
{
    HANDLE hand = OpenProcess(PROCESS_ALL_ACCESS, FALSE, this->pid);
    return hand;
}



// ���������� ������ update
TASKM_ERROR Taskm::update()
{
    // ������� ������ ������ ����� �����������
    taskList.clear();

    Taskm::CPU_total_init();

    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) { // ��������� ��������� CreateToolhelp32Snapshot
        // ����� �������� ����������� ������ WSAGetLastError()
        return TASKM_GENERIC_ERROR; // ��� ������ ���������� ��� ������
    }


    PROCESSENTRY32 entry;
    entry.dwSize = sizeof(PROCESSENTRY32);

    if (!Process32First(snap, &entry))
    {
        CloseHandle(snap); // �� �������� ������� ����� ��� ������
        return FIRSTPROCESS_ERROR;
    }

    do
    {
        Task task;

        // Memory
        // ����������� ���������� ����������� �����
        HANDLE procHandle = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, entry.th32ProcessID);
        if (procHandle != NULL) // ��������� ���������� OpenProcess
        {
            PROCESS_MEMORY_COUNTERS_EX mem = {}; // �������������� ������
            mem.cb = sizeof(mem); // ����� ������� ������ ��������� ��� ��������� ������ Windows
            if (GetProcessMemoryInfo(procHandle, (PROCESS_MEMORY_COUNTERS*)&mem, sizeof(mem)))
            {
                SIZE_T physMem = mem.WorkingSetSize;
                // ���������� ������� � ��������� ������
                task.memory = static_cast<long double>(physMem) / MB_SIZE;
            }
            else {
                // �� ������� �������� ���������� � ������ (��������, ��� ���������� ���������)
                task.memory = 0; // ��� ������ �������� �� ���������
            }
            CloseHandle(procHandle);
        }
        else {
            // �� ������� ������� ������� (������������ ����?)
            task.memory = 0; // ��� ������ �������� �� ���������
        }


        // CPU - Placeholder (�� ����������� � ����� ����)



        // Process Identification
        task.name = Taskm::wchar_to_string(entry.szExeFile);
        task.pid = entry.th32ProcessID;
        task.pidParent = entry.th32ParentProcessID;

        taskList.push_back(task);

    } while (Process32Next(snap, &entry));

    // Actual CPU part, will redo later, maybe

    if (!taskList[0].init)
    {
        Taskm::init_taskList();
    }
    Sleep(1000);
    for (Task& task : taskList)
    {
        Taskm::update_cpuUsage(task);
    }

    CloseHandle(snap);
    return TASKM_OK;
}

// ���������� ������ print
TASKM_ERROR Taskm::print()
{
    if (taskList.empty())
    {
        // ����� �� ������� �������, � ������ ������ �� ��������
        /*std::cout << "������ ����� ����." << std::endl;
        return TASKM_EMPTY_ERROR;*/ // ��� TASKM_OK, � ����������� �� ���������
    }
    else
    {
        for (const auto& task : taskList) // ���������� const& ��� �������������
        {
            std::cout << "Name: " << task.name << "; PID: " << task.pid << "; Parent: " << task.pidParent << "; RAM: " << task.memory << " MB" << "; CPU: " << task.cpuUsage << std::endl;
        }
        return TASKM_OK;
    }
}

// ���������� ������ save_json
TASKM_ERROR Taskm::save_json()
{
    if (taskList.empty())
    {
        return TASKM_EMPTY_ERROR;
    }

    nlohmann::json jsonFile;
    jsonFile["processes"] = nlohmann::json::array(); // �������������� ��� ������
    
    for (const auto& task : taskList) // ���������� const&
    {
        nlohmann::json taskObject =
        {
            {"pid", task.pid},
            {"pidParent", task.pidParent},
            {"name", task.name},
            {"memory", task.memory},
            {"cpu", task.cpuUsage}
        };
        jsonFile["processes"].push_back(taskObject);
    }


    std::ofstream file("output.json");
    if (!file.is_open()) { // ���������, �������� �� ����
        std::cerr << "������: �� ������� ������� output.json ��� ������." << std::endl;
        return TASKM_GENERIC_ERROR; // ��� ����������� ������ �����
    }
    file << jsonFile.dump(4); // dump(4) ��� ��������� ������ � ���������
    file.close(); // ���� ofstream ��������� ��� � �����������, ���� ������� �� ��������

    return TASKM_OK; // ���������� OK ����� �������� ������
}

TASKM_ERROR Taskm::save_json_totals()
{
    if (taskList.empty())
    {
        return TASKM_EMPTY_ERROR;
    }

    nlohmann::json jsonFile;
    jsonFile["totals"] = nlohmann::json::object();

    jsonFile["totals"] =
    {
        {"CPU", Taskm::CPU_total_get()}
    };

    std::ofstream file("totals_output.json");
    if (!file.is_open()) { // ���������, �������� �� ����
        std::cerr << "������: �� ������� ������� output.json ��� ������." << std::endl;
        return TASKM_GENERIC_ERROR; // ��� ����������� ������ �����
    }
    file << jsonFile.dump(4); // dump(4) ��� ��������� ������ � ���������
    file.close(); // ���� ofstream ��������� ��� � �����������, ���� ������� �� ��������

    return TASKM_OK; // ���������� OK ����� �������� ������
}

nlohmann::json Taskm::get_json_object()
{
    if (taskList.empty())
    {
        // ���������� ������ ������ ��� null, ����� ������� �� ���������� ������
        return nlohmann::json::object(); // ������ JSON ������ {}
        // ��� return nullptr; // ���� ���������� ��� ����� ��������� �� null
    }

    nlohmann::json jsonFile;
    jsonFile["processes"] = nlohmann::json::array(); // �������������� ��� ������

    for (const auto& task : taskList)
    {
        // ��������� �������� ����� ����������� � JSON ��� �������
        double cpuRounded = round(task.cpuUsage * 100.0) / 100.0; // �� 2 ������
        double memRounded = round(task.memory * 100.0) / 100.0;   // �� 2 ������

        nlohmann::json taskObject =
        {
            {"pid", task.pid},
            {"pidParent", task.pidParent}, // ���������, ���� ����� � GUI
            {"name", task.name},
            {"memory", memRounded}, // ���������� ����������� ��������
            {"cpu", cpuRounded}     // ���������� ����������� ��������
        };
        jsonFile["processes"].push_back(taskObject);
    }
    // ���������� ������� JSON ������
    return jsonFile;
}

nlohmann::json Taskm::get_json_totals()
{
    nlohmann::json jsonFile;
    jsonFile["totals"] = nlohmann::json::object();

    jsonFile["totals"] =
    {
        {"CPU", Taskm::CPU_total_get()}
    };

    return jsonFile;
}
// ���������� ������ get
//std::vector<Task> Taskm::get()
//{
//    return taskList; // ���������� �����
//    // ���� ����� ������ ������ ��� ������, ����� �������:
//    // const std::vector<Task>& Taskm::get() const { return taskList; }
//}


// init_taskList realisation, used to set some Task fields needed for cpu tracking

// Could be done on Task init, but probaly would be slower
// However this variant probably takes more memory

// Code is from StackOverflow
// TODO: Linkie here

void Taskm::init_taskList()
{
    SYSTEM_INFO sysInfo;
    FILETIME ftime, fsys, fuser;
    HANDLE procHandle;

    GetSystemInfo(&sysInfo);
    GetSystemTimeAsFileTime(&ftime);

    for (Task & task : Taskm::taskList)
    {
        procHandle = task.get_handle();
        GetProcessTimes(procHandle, &ftime, &ftime, &fsys, &fuser);

        task.processors = sysInfo.dwNumberOfProcessors;

        memcpy(&task.CPUlast, &ftime, sizeof(FILETIME));
        memcpy(&task.CPUlastSys, &fsys, sizeof(FILETIME));
        memcpy(&task.CPUlastUser, &fuser, sizeof(FILETIME));

        CloseHandle(procHandle);
    }
}

// Updates a single task struct with cpu usage
// probably have to be reworked
// has a problem with integer limits
void Taskm::update_cpuUsage(Task & task)
{
    FILETIME ftime, fsys, fuser;
    ULARGE_INTEGER now, sys, user;
    HANDLE procHandle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, task.pid);

    GetSystemTimeAsFileTime(&ftime);
    memcpy(&now, &ftime, sizeof(FILETIME));

    GetProcessTimes(procHandle, &ftime, &ftime, &fsys, &fuser);
    memcpy(&sys, &fsys, sizeof(FILETIME));
    memcpy(&user, &fuser, sizeof(FILETIME));

    task.cpuUsage =
        (
            (static_cast<double>(sys.QuadPart) - task.CPUlastSys.QuadPart) +
            (user.QuadPart - task.CPUlastUser.QuadPart)
            )
        /
        (now.QuadPart - task.CPUlast.QuadPart)
        /
        task.processors;

    task.CPUlast = now;
    task.CPUlastSys = sys;
    task.CPUlastUser = user;
}

//CPU total

void Taskm::CPU_total_init() {
    PdhOpenQueryW(NULL, NULL, &cpuQuery);
    // You can also use L"\\Processor(*)\\% Processor Time" and get individual CPU values with PdhGetFormattedCounterArray()
    PdhAddEnglishCounter(cpuQuery, L"\\Processor(_Total)\\% Processor Time", NULL, &cpuTotal);
    PdhCollectQueryData(cpuQuery);
}

double Taskm::CPU_total_get()
{
    PDH_FMT_COUNTERVALUE counterVal;

    PdhCollectQueryData(cpuQuery);
    PdhGetFormattedCounterValue(cpuTotal, PDH_FMT_DOUBLE, NULL, &counterVal);
    return counterVal.doubleValue;
}

// ���������� ���������� ������ wchar_to_string
std::string Taskm::wchar_to_string(WCHAR* wch)
{
    if (wch == nullptr) return ""; // ������� �������� �� null
    std::wstring wstr(wch);
    // ��� ���������� ����������� � �������������� ������, �������� � ��-ASCII ���������,
    // ����� ������������ ������� Windows API ��� ����������� �������� C++11/17
    // ������� ������� (����� ������ �������):
    // return std::string(wstr.begin(), wstr.end());

    // ����� �������� ������� � �������������� Windows API:
    int bufferSize = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (bufferSize == 0) return ""; // ������ �����������
    std::string narrowString(bufferSize, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &narrowString[0], bufferSize, nullptr, nullptr);
    narrowString.pop_back(); // ������� ������� ����������, ����������� WideCharToMultiByte
    return narrowString;
}