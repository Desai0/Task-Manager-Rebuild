// Taskm.cpp
#include "Taskm.h"
#include <TlHelp32.h>
#include <tchar.h>
#include <iostream>
#include <fstream>
#include <Psapi.h>
#include <vector> // Для временного хранения PID
#include <set>    // Для удобного поиска текущих PID
#define BYTES_IN_MB (1024.0 * 1024.0)

// Конструктор: Инициализация
Taskm::Taskm() : firstRun(true), processorCount(0) {
    // 1. Получаем количество процессоров
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    processorCount = sysInfo.dwNumberOfProcessors;
    if (processorCount <= 0) {
        processorCount = 1; // Запасной вариант
        std::cerr << "[Taskm] Warning: Could not get processor count, defaulting to 1." << std::endl;
    }

    // 2. Получаем начальное системное время
    FILETIME ftime;
    GetSystemTimeAsFileTime(&ftime);
    memcpy(&lastSystemTime, &ftime, sizeof(FILETIME));

    // 3. Инициализируем запрос для общего CPU
    initialize_total_cpu_query();

    std::cout << "[Taskm] Initialized. Processor Count: " << processorCount << std::endl;
}

// Деструктор: Очистка PDH
Taskm::~Taskm() {
    if (cpuQuery) {
        PdhCloseQuery(cpuQuery);
    }
}

// Инициализация PDH запроса для общего CPU
void Taskm::initialize_total_cpu_query() {
    if (cpuQuery) { // Предотвращаем повторную инициализацию
        PdhCloseQuery(cpuQuery);
        cpuQuery = nullptr;
        cpuTotal = nullptr;
    }
    PDH_STATUS status = PdhOpenQueryW(NULL, NULL, &cpuQuery);
    if (status != ERROR_SUCCESS) {
        std::cerr << "[Taskm] Error: PdhOpenQueryW failed with status " << std::hex << status << std::endl;
        return; // Выходим, если не удалось открыть запрос
    }
    // Используем L"\\Processor Information(_Total)\\% Processor Time" для совместимости с разными локалями
    // Если не работает, можно попробовать L"\\Processor(_Total)\\% Processor Time"
    status = PdhAddEnglishCounterA(cpuQuery, "\\Processor(_Total)\\% Processor Time", NULL, &cpuTotal);
    if (status != ERROR_SUCCESS) {
        std::cerr << "[Taskm] Error: PdhAddEnglishCounterA failed for Processor Time with status 0x" << std::hex << status << std::endl;
        // Попробовать запасной вариант, если первый не удался
        status = PdhAddCounterW(cpuQuery, L"\\Processor(_Total)\\% Processor Time", NULL, &cpuTotal);
        if (status != ERROR_SUCCESS) {
            std::cerr << "[Taskm] Error: PdhAddCounterW fallback also failed with status " << std::hex << status << std::endl;
            PdhCloseQuery(cpuQuery); // Закрываем запрос, если счетчик добавить не удалось
            cpuQuery = nullptr;
            return;
        }
        else {
            std::cout << "[Taskm] Warning: Using fallback counter name '\\Processor(_Total)\\% Processor Time'." << std::endl;
        }
    }
    // Делаем первый сбор данных, чтобы инициализировать счетчик
    status = PdhCollectQueryData(cpuQuery);
    if (status != ERROR_SUCCESS) {
        std::cerr << "[Taskm] Error: Initial PdhCollectQueryData failed with status " << std::hex << status << std::endl;
        // Не обязательно закрывать запрос здесь, но счетчик может быть не готов
    }
    std::cout << "[Taskm] Total CPU query initialized." << std::endl;
}


// Получение начальных времен процесса
void Taskm::initialize_cpu_times(Task& task) {
    HANDLE procHandle = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, task.pid);
    if (procHandle == NULL) {
        // Не удалось открыть - оставляем времена нулевыми
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

        // Проверяем, что и интервал времени, и количество процессоров валидны
        if (systemTimeDelta > 0 && processorCount > 0) {
            // Рассчитываем % CPU:
            // (Время процесса / (Общее время интервала * кол-во ядер)) * 100
            // Это эквивалентно: (Время процесса / Общее время интервала) / кол-во ядер * 100
            task.cpuUsage = (static_cast<double>(processTimeDelta) * 100.0) /
                (static_cast<double>(systemTimeDelta) * processorCount); // <--- ДОБАВЛЕНО ДЕЛЕНИЕ НА processorCount
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

    // Ограничиваем значение (на всякий случай)
    if (task.cpuUsage < 0.0) {
        task.cpuUsage = 0.0;
    }
    // Теперь максимальное значение должно быть близко к 100%
    //if (task.cpuUsage > 100.0) {
    //    // Можно оставить как есть или ограничить 100. Небольшие превышения из-за точности возможны.
    //    // task.cpuUsage = 100.0;
    //}
}


// Обновленная реализация метода update()
TASKM_ERROR Taskm::update() {
    // 1. Получаем текущее системное время
    FILETIME ftimeNow;
    ULARGE_INTEGER currentTime;
    GetSystemTimeAsFileTime(&ftimeNow);
    memcpy(&currentTime, &ftimeNow, sizeof(FILETIME));

    
    // 2. Рассчитываем прошедшее системное время
    ULONGLONG systemTimeDelta = currentTime.QuadPart - lastSystemTime.QuadPart;

    // Если это первый запуск или интервал слишком мал, пропускаем расчет CPU в этот раз
    bool skipCpuCalculation = firstRun || (systemTimeDelta < 100000); // 100000 = 10ms (примерный порог)

    // 3. Получаем текущий список процессов
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) {
        return TASKM_GENERIC_ERROR;
    }

    PROCESSENTRY32 entry;
    entry.dwSize = sizeof(PROCESSENTRY32);
    std::set<DWORD> currentPids; // Храним PID текущих процессов

    if (!Process32First(snap, &entry)) {
        CloseHandle(snap);
        return FIRSTPROCESS_ERROR;
    }

    // --- Цикл обновления/добавления ---
    do {
        currentPids.insert(entry.th32ProcessID); // Добавляем PID в сет текущих

        // Ищем процесс в нашей карте
        auto it = processMap.find(entry.th32ProcessID);

        if (it != processMap.end()) {
            // --- Процесс уже существует ---
            Task& existingTask = it->second; // Получаем ссылку на существующую задачу

            // Обновляем данные, которые могут измениться (память, имя - хотя имя редко)
            existingTask.pidParent = entry.th32ParentProcessID;
            // Имя обычно не меняется, можно не обновлять для производительности
            // existingTask.name = Taskm::wchar_to_string(entry.szExeFile);

           // Получаем память
            HANDLE procHandleMem = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, entry.th32ProcessID);
            if (procHandleMem != NULL) {
                PROCESS_MEMORY_COUNTERS_EX mem = {};
                mem.cb = sizeof(mem);
                if (GetProcessMemoryInfo(procHandleMem, (PROCESS_MEMORY_COUNTERS*)&mem, sizeof(mem))) {
                    existingTask.memory = static_cast<long double>(mem.PrivateUsage) / BYTES_IN_MB;
                }
                else {
                    existingTask.memory = 0.0; // Ошибка получения памяти
                }
                CloseHandle(procHandleMem);
            }
            else {
                existingTask.memory = 0.0; // Не удалось открыть для памяти
            }

            // Рассчитываем CPU (если не первый запуск и интервал достаточен)
            if (!skipCpuCalculation) {
                calculate_task_cpu(existingTask, systemTimeDelta);
            }
            // В первый раз CPU останется 0.0

        }
        else {
            // --- Новый процесс ---
            Task newTask;
            newTask.pid = entry.th32ProcessID;
            newTask.pidParent = entry.th32ParentProcessID;
            newTask.name = Taskm::wchar_to_string(entry.szExeFile);
            newTask.cpuUsage = 0.0; // Начальное значение CPU

            // Получаем память для нового процесса
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

            // Получаем начальные времена CPU для нового процесса
            initialize_cpu_times(newTask);

            // Добавляем новый процесс в карту
            processMap.insert({ newTask.pid, newTask });
        }

    } while (Process32Next(snap, &entry));

    CloseHandle(snap);

    // --- Цикл удаления завершенных процессов ---
    for (auto it = processMap.begin(); it != processMap.end(); /* нет инкремента здесь */) {
        // Если PID из нашей карты НЕ найден в сете текущих PID...
        if (currentPids.find(it->first) == currentPids.end()) {
            // ...значит процесс завершился, удаляем его из карты
            it = processMap.erase(it); // erase возвращает итератор на следующий элемент
        }
        else {
            // Иначе переходим к следующему элементу карты
            ++it;
        }
    }

    // 4. Обновляем время последнего замера системы для следующего вызова
    lastSystemTime = currentTime;
    firstRun = false; // Снимаем флаг первого запуска

    return TASKM_OK;
}


// --- Остальные методы (print, save_json, get_json_object и т.д.) ---

// Метод print (теперь итерирует по карте)
TASKM_ERROR Taskm::print() {
    if (processMap.empty()) {
        std::cout << "Process map is empty." << std::endl;
        return TASKM_EMPTY_ERROR; // Или TASKM_OK
    }
    else {
        for (const auto& pair : processMap) {
            const Task& task = pair.second;
            std::cout << "Name: " << task.name << "; PID: " << task.pid << "; Parent: " << task.pidParent << "; RAM: " << task.memory << " MB" << "; CPU: " << task.cpuUsage << "%" << std::endl;
        }
        return TASKM_OK;
    }
}

// Метод get_json_object (теперь итерирует по карте)
nlohmann::json Taskm::get_json_object() {
    nlohmann::json jsonResponse;
    jsonResponse["processes"] = nlohmann::json::array();

    if (processMap.empty()) {
        return jsonResponse;
    }

    std::cout << "--- Preparing JSON Object --- Map size: " << processMap.size() << std::endl; // Лог размера карты
    std::set<DWORD> pidsInJson; // Сет для проверки дубликатов PID в JSON

    // --- Логирование для конкретной группы (например, Яндекс Музыка) ---
    double loggedGroupTotalMemory = 0;
    int loggedGroupCount = 0;
    const std::string debugGroupName = "Яндекс Музыка.exe"; // Имя процесса для детального лога
    // --------------------------------------------------------------------

    for (const auto& pair : processMap) {
        const Task& task = pair.second;

        // Проверка на дубликаты PID перед добавлением в JSON
        if (pidsInJson.count(task.pid)) {
            std::cerr << "!!!!!! ERROR: Duplicate PID found in processMap before JSON creation: " << task.pid << std::endl;
            continue; // Пропускаем добавление дубликата
        }
        pidsInJson.insert(task.pid);

        // --- Логирование для отлаживаемой группы ---
        if (task.name == debugGroupName) {
            std::cout << "  [JSON Prep] PID: " << task.pid
                << ", Name: " << task.name
                << ", Memory MB: " << task.memory // Выводим значение как есть
                << std::endl;
            loggedGroupTotalMemory += task.memory;
            loggedGroupCount++;
        }
        // --------------------------------------------

        nlohmann::json taskObject = {
            {"pid", task.pid},
            {"pidParent", task.pidParent},
            {"name", task.name},
            {"memory", task.memory}, // Отправляем как есть
            {"cpu", task.cpuUsage}
        };
        jsonResponse["processes"].push_back(taskObject);
    }

    // --- Выводим итоговую сумму для отлаживаемой группы ---
    if (loggedGroupCount > 0) {
        std::cout << "  [JSON Prep] Total Memory for '" << debugGroupName << "' (" << loggedGroupCount << " processes) before sending: "
            << loggedGroupTotalMemory << " MB" << std::endl;
    }
    std::cout << "--- Finished Preparing JSON Object --- Processes in JSON: " << jsonResponse["processes"].size() << std::endl; // Лог количества в JSON

    return jsonResponse;
}


// Метод save_json (теперь использует get_json_object)
TASKM_ERROR Taskm::save_json() {
    nlohmann::json jsonFile = get_json_object(); // Получаем готовый JSON

    // Проверяем, есть ли процессы после вызова get_json_object
    if (jsonFile.is_null() || !jsonFile.contains("processes") || jsonFile["processes"].empty()) {
        std::cerr << "Warning: Attempting to save empty process list to output.json." << std::endl;
        // Можно вернуть TASKM_EMPTY_ERROR или просто создать пустой файл
    }

    std::ofstream file("output.json");
    if (!file.is_open()) {
        std::cerr << "Error: Could not open output.json for writing." << std::endl;
        return TASKM_GENERIC_ERROR;
    }
    file << jsonFile.dump(4); // dump(4) для красивого вывода
    file.close();

    return TASKM_OK;
}

// --- Общие методы загрузки (без изменений, но проверим CPU_total_get) ---

// Получение общей загрузки CPU (%)
double Taskm::CPU_total_get() {
    if (!cpuQuery || !cpuTotal) {
        std::cerr << "[Taskm] Error: Total CPU query not initialized before calling CPU_total_get()." << std::endl;
        // Попробовать инициализировать здесь? Или просто вернуть ошибку.
        initialize_total_cpu_query(); // Попытка инициализации
        if (!cpuQuery || !cpuTotal) return -1.0; // Если все еще не инициализировано
    }

    PDH_FMT_COUNTERVALUE counterVal;
    PDH_STATUS status = PdhCollectQueryData(cpuQuery); // Собрать свежие данные
    if (status != ERROR_SUCCESS) {
        std::cerr << "[Taskm] Error: PdhCollectQueryData failed in CPU_total_get() with status " << std::hex << status << std::endl;
        // Данные могут быть старыми или невалидными
        // return -1.0; // Вернуть ошибку или пытаться получить старое значение?
    }

    // Получаем отформатированное значение
    status = PdhGetFormattedCounterValue(cpuTotal, PDH_FMT_DOUBLE, NULL, &counterVal);
    if (status != ERROR_SUCCESS) {
        std::cerr << "[Taskm] Error: PdhGetFormattedCounterValue failed in CPU_total_get() with status " << std::hex << status << std::endl;
        return -1.0; // Ошибка получения значения
    }

    // PDH иногда возвращает > 100% на короткое время, можно ограничить
    double cpuLoad = counterVal.doubleValue;
    if (cpuLoad < 0.0) return 0.0; // Не должно быть отрицательным
    // if (cpuLoad > 100.0) return 100.0; // Опционально ограничить 100%
    return cpuLoad;
}

// Получение % загрузки памяти (без изменений)
double Taskm::get_system_memory_load_percent() {
    MEMORYSTATUSEX statex;
    statex.dwLength = sizeof(statex);
    if (GlobalMemoryStatusEx(&statex)) {
        return static_cast<double>(statex.dwMemoryLoad);
    }
    else {
        std::cerr << "[Taskm] Error GlobalMemoryStatusEx: " << GetLastError() << std::endl;
        return -1.0; // Индикация ошибки
    }
}

// Конвертация WCHAR* в std::string (без изменений)
std::string Taskm::wchar_to_string(WCHAR* wch) {
    if (wch == nullptr) return "";
    std::wstring wstr(wch);
    int bufferSize = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (bufferSize == 0) return "";
    std::string narrowString(bufferSize, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &narrowString[0], bufferSize, nullptr, nullptr);
    // Удаляем нулевой терминатор, который WideCharToMultiByte может добавить в конец строки std::string
    if (!narrowString.empty() && narrowString.back() == '\0') {
        narrowString.pop_back();
    }
    return narrowString;
}

// --- Метод get_handle() больше не нужен, если только для внешнего использования ---
// HANDLE Task::get_handle() { ... }

// --- Методы init_taskList() и update_cpuUsage(Task&) больше не нужны ---
// void Taskm::init_taskList() { ... }
// void Taskm::update_cpuUsage(Task & task) { ... }


// --- Методы save_json_totals и get_json_totals (без изменений, используют новые get-методы) ---
TASKM_ERROR Taskm::save_json_totals() {
    //Memory
    MEMORYSTATUSEX mem;
    mem.dwLength = sizeof(MEMORYSTATUSEX);
    GlobalMemoryStatusEx(&mem); // Можно заменить на get_system_memory_load_percent(), если нужен только %

    nlohmann::json jsonFile;
    jsonFile["totals"] = nlohmann::json::object();

    jsonFile["totals"] = {
        {"CPU_Percent", CPU_total_get()}, // Используем % общей загрузки CPU
        {"RAM_Percent", get_system_memory_load_percent()}, // Используем % общей загрузки RAM
        // Если нужно общее кол-во RAM в MB:
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