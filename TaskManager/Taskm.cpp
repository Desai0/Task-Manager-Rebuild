// Taskm.cpp
#include "Taskm.h" // ����������� �������� ��������� ������

#include <TlHelp32.h> // �������� �����, �.�. ����� ��� update
#include <tchar.h>    // �������� ����� (���� �����)
#include <iostream>   // �������� �����, �.�. ����� ��� print
#include <fstream>    // �������� �����, �.�. ����� ��� save_json
#include <Psapi.h>    // �������� �����, �.�. ����� ��� update (GetProcessMemoryInfo)
#include "json.hpp"

// ���������� ������ update
TASKM_ERROR Taskm::update()
{
    // ������� ������ ������ ����� �����������
    taskList.clear();

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

    CloseHandle(snap);
    return TASKM_OK;
}

// ���������� ������ print
TASKM_ERROR Taskm::print()
{
    if (taskList.empty())
    {
        // ����� �� ������� �������, � ������ ������ �� ��������
        std::cout << "������ ����� ����." << std::endl;
        return TASKM_EMPTY_ERROR; // ��� TASKM_OK, � ����������� �� ���������
    }
    else
    {
        for (const auto& task : taskList) // ���������� const& ��� �������������
        {
            std::cout << "Name: " << task.name << "; PID: " << task.pid << "; Parent: " << task.pidParent << "; RAM: " << task.memory << " MB" << std::endl;
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
            {"memory", task.memory}
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

// ���������� ������ get
std::vector<Task> Taskm::get()
{
    return taskList; // ���������� �����
    // ���� ����� ������ ������ ��� ������, ����� �������:
    // const std::vector<Task>& Taskm::get() const { return taskList; }
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