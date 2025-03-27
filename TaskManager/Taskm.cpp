// Taskm.cpp
#include "Taskm.h" // Обязательно включить заголовок класса

#include <TlHelp32.h> // Включаем здесь, т.к. нужно для update
#include <tchar.h>    // Включаем здесь (если нужно)
#include <iostream>   // Включаем здесь, т.к. нужно для print
#include <fstream>    // Включаем здесь, т.к. нужно для save_json
#include <Psapi.h>    // Включаем здесь, т.к. нужно для update (GetProcessMemoryInfo)
#include "json.hpp"

// Реализация метода update
TASKM_ERROR Taskm::update()
{
    // Очищаем старый список перед обновлением
    taskList.clear();

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

    CloseHandle(snap);
    return TASKM_OK;
}

// Реализация метода print
TASKM_ERROR Taskm::print()
{
    if (taskList.empty())
    {
        // Можно не считать ошибкой, а просто ничего не выводить
        std::cout << "Список задач пуст." << std::endl;
        return TASKM_EMPTY_ERROR; // Или TASKM_OK, в зависимости от семантики
    }
    else
    {
        for (const auto& task : taskList) // Используем const& для эффективности
        {
            std::cout << "Name: " << task.name << "; PID: " << task.pid << "; Parent: " << task.pidParent << "; RAM: " << task.memory << " MB" << std::endl;
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
            {"memory", task.memory}
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

// Реализация метода get
std::vector<Task> Taskm::get()
{
    return taskList; // Возвращает копию
    // Если нужна только ссылка для чтения, можно сделать:
    // const std::vector<Task>& Taskm::get() const { return taskList; }
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