// Taskm.h
#pragma once

#include <windows.h>
#include <vector>
#include <string>
#include <map> // <--- Добавить для processMap
#include "json.hpp" // Если используется здесь
#include <Pdh.h>   // <--- Добавить для PDH

// Define MB_SIZE здесь или в Taskm.cpp глобально (но не внутри функции)
#define BYTES_IN_MB (1024.0 * 1024.0)

// Коды ошибок (пример)
typedef enum {
    TASKM_OK = 0,
    TASKM_GENERIC_ERROR,
    FIRSTPROCESS_ERROR,
    TASKM_EMPTY_ERROR
    // ... другие ошибки
} TASKM_ERROR;

// Структура для хранения данных о процессе
struct Task {
    DWORD pid = 0;
    DWORD pidParent = 0;
    std::string name = "";
    long double memory = 0.0; // В МБ
    double cpuUsage = 0.0;    // В процентах

    // --- Данные для расчета CPU ---
    ULARGE_INTEGER lastKernelTime = {}; // Предыдущее время ядра
    ULARGE_INTEGER lastUserTime = {};   // Предыдущее время пользователя
    // task.processors больше не нужен здесь
    // task.CPUlast (общее системное время) больше не нужен здесь
    // task.init больше не нужен

    // Конструктор по умолчанию
    Task() = default;
};

class Taskm {
private:
    // --- Хранилище данных процессов между обновлениями ---
    std::map<DWORD, Task> processMap;

    // --- Данные для расчета интервала CPU ---
    ULARGE_INTEGER lastSystemTime = {}; // Общее системное время предыдущего замера
    int processorCount = 0;          // Количество логических процессоров
    bool firstRun = true;            // Флаг первого запуска update

    // --- Данные для общего CPU (PDH) ---
    PDH_HQUERY cpuQuery = nullptr;
    PDH_HCOUNTER cpuTotal = nullptr;

    // --- Вспомогательные функции ---
    void initialize_cpu_times(Task& task); // Получить начальные времена процесса
    void calculate_task_cpu(Task& task, ULONGLONG systemTimeDelta); // Рассчитать CPU для задачи

    // Статичный метод конвертации (оставляем)
    static std::string wchar_to_string(WCHAR* wch);

public:
    // --- Конструктор и Деструктор ---
    Taskm();  // Добавляем конструктор для инициализации
    ~Taskm(); // Добавляем деструктор для очистки PDH

    // --- Основные методы ---
    TASKM_ERROR update(); // Обновить список и данные процессов
    TASKM_ERROR print();
    TASKM_ERROR save_json();
    TASKM_ERROR save_json_totals();
    nlohmann::json get_json_object(); // Получить JSON со списком процессов
    nlohmann::json get_json_totals();

    // --- Методы для общей загрузки (оставляем) ---
    double CPU_total_get(); // Переименовал из CPU_total_init в initialize_total_cpu_query
    void initialize_total_cpu_query(); // Инициализация PDH для общего CPU
    double get_system_memory_load_percent(); // Получить % загрузки RAM
};