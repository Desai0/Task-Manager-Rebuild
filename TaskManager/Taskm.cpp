// Taskm.cpp
#include "Taskm.h" // Обязательно включить заголовок класса

#include <TlHelp32.h> // Включаем здесь, т.к. нужно для update
#include <tchar.h>    // Включаем здесь (если нужно)
#include <iostream>   // Включаем здесь, т.к. нужно для print
#include <fstream>    // Включаем здесь, т.к. нужно для save_json
#include <Psapi.h>    // Включаем здесь, т.к. нужно для update (GetProcessMemoryInfo)
#include "json.hpp"


// Method returning process handle, needed for cpu usage tracking, probably
HANDLE Task::get_handle()
{
    HANDLE hand = OpenProcess(PROCESS_ALL_ACCESS, FALSE, this->pid);
    return hand;
}



// Реализация метода update
TASKM_ERROR Taskm::update()
{
    // Очищаем старый список перед обновлением
    taskList.clear();

    Taskm::CPU_total_init();

    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) { // Проверяем результат CreateToolhelp32Snapshot
        // Можно добавить логирование ошибки WSAGetLastError()
        return TASKM_GENERIC_ERROR; // Или другой подходящий код ошибки
    }


    PROCESSENTRY32 entry;
    entry.dwSize = sizeof(PROCESSENTRY32);

    if (!Process32First(snap, &entry))
    {
        CloseHandle(snap); // Не забываем закрыть хэндл при ошибке
        return FIRSTPROCESS_ERROR;
    }

    do
    {
        Task task;

        // Memory
        // Запрашиваем минимально необходимые права
        HANDLE procHandle = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, entry.th32ProcessID);
        if (procHandle != NULL) // Проверяем успешность OpenProcess
        {
            PROCESS_MEMORY_COUNTERS_EX mem = {}; // Инициализируем нулями
            mem.cb = sizeof(mem); // Важно указать размер структуры для некоторых версий Windows
            if (GetProcessMemoryInfo(procHandle, (PROCESS_MEMORY_COUNTERS*)&mem, sizeof(mem)))
            {
                SIZE_T physMem = mem.WorkingSetSize;
                // Используем деление с плавающей точкой
                task.memory = static_cast<long double>(physMem) / MB_SIZE;
            }
            else {
                // Не удалось получить информацию о памяти (например, для защищенных процессов)
                task.memory = 0; // Или другое значение по умолчанию
            }
            CloseHandle(procHandle);
        }
        else {
            // Не удалось открыть процесс (недостаточно прав?)
            task.memory = 0; // Или другое значение по умолчанию
        }


        // CPU - Placeholder (не реализовано в вашем коде)



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

// Реализация метода print
TASKM_ERROR Taskm::print()
{
    if (taskList.empty())
    {
        // Можно не считать ошибкой, а просто ничего не выводить
        /*std::cout << "Список задач пуст." << std::endl;
        return TASKM_EMPTY_ERROR;*/ // Или TASKM_OK, в зависимости от семантики
    }
    else
    {
        for (const auto& task : taskList) // Используем const& для эффективности
        {
            std::cout << "Name: " << task.name << "; PID: " << task.pid << "; Parent: " << task.pidParent << "; RAM: " << task.memory << " MB" << "; CPU: " << task.cpuUsage << std::endl;
        }
        return TASKM_OK;
    }
}

// Реализация метода save_json
TASKM_ERROR Taskm::save_json()
{
    if (taskList.empty())
    {
        return TASKM_EMPTY_ERROR;
    }

    nlohmann::json jsonFile;
    jsonFile["processes"] = nlohmann::json::array(); // Инициализируем как массив
    
    for (const auto& task : taskList) // Используем const&
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
    if (!file.is_open()) { // Проверяем, открылся ли файл
        std::cerr << "Ошибка: Не удалось открыть output.json для записи." << std::endl;
        return TASKM_GENERIC_ERROR; // Или специфичная ошибка файла
    }
    file << jsonFile.dump(4); // dump(4) для красивого вывода с отступами
    file.close(); // Хотя ofstream закроется сам в деструкторе, явно закрыть не помешает

    return TASKM_OK; // Возвращаем OK после успешной записи
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
    if (!file.is_open()) { // Проверяем, открылся ли файл
        std::cerr << "Ошибка: Не удалось открыть output.json для записи." << std::endl;
        return TASKM_GENERIC_ERROR; // Или специфичная ошибка файла
    }
    file << jsonFile.dump(4); // dump(4) для красивого вывода с отступами
    file.close(); // Хотя ofstream закроется сам в деструкторе, явно закрыть не помешает

    return TASKM_OK; // Возвращаем OK после успешной записи
}

nlohmann::json Taskm::get_json_object()
{
    if (taskList.empty())
    {
        // Возвращаем пустой объект или null, чтобы указать на отсутствие данных
        return nlohmann::json::object(); // Пустой JSON объект {}
        // или return nullptr; // если вызывающий код будет проверять на null
    }

    nlohmann::json jsonFile;
    jsonFile["processes"] = nlohmann::json::array(); // Инициализируем как массив

    for (const auto& task : taskList)
    {
        // Округляем значения перед добавлением в JSON для красоты
        double cpuRounded = round(task.cpuUsage * 100.0) / 100.0; // до 2 знаков
        double memRounded = round(task.memory * 100.0) / 100.0;   // до 2 знаков

        nlohmann::json taskObject =
        {
            {"pid", task.pid},
            {"pidParent", task.pidParent}, // Оставляем, если нужно в GUI
            {"name", task.name},
            {"memory", memRounded}, // Используем округленное значение
            {"cpu", cpuRounded}     // Используем округленное значение
        };
        jsonFile["processes"].push_back(taskObject);
    }
    // Возвращаем готовый JSON объект
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
// Реализация метода get
//std::vector<Task> Taskm::get()
//{
//    return taskList; // Возвращает копию
//    // Если нужна только ссылка для чтения, можно сделать:
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

// Реализация статичного метода wchar_to_string
std::string Taskm::wchar_to_string(WCHAR* wch)
{
    if (wch == nullptr) return ""; // Добавим проверку на null
    std::wstring wstr(wch);
    // Для корректной конвертации в мультибайтовую строку, особенно с не-ASCII символами,
    // лучше использовать функции Windows API или стандартные средства C++11/17
    // Простой вариант (может терять символы):
    // return std::string(wstr.begin(), wstr.end());

    // Более надежный вариант с использованием Windows API:
    int bufferSize = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (bufferSize == 0) return ""; // Ошибка конвертации
    std::string narrowString(bufferSize, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &narrowString[0], bufferSize, nullptr, nullptr);
    narrowString.pop_back(); // Удалить нулевой терминатор, добавленный WideCharToMultiByte
    return narrowString;
}