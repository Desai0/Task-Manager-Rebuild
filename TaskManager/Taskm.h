#pragma once

#include <Windows.h>
#include <vector>
#include <Psapi.h>
#include <string>

#include <TCHAR.h>
#include <Pdh.h>
#pragma comment(lib, "pdh.lib")


#include "json.hpp"

#define MB_SIZE 1000000.0

enum TASKM_ERROR {
	TASKM_OK,
	FIRSTPROCESS_ERROR,
	TASKM_EMPTY_ERROR,

	TASKM_GENERIC_ERROR
};

struct Task {
	
	// Output fields
	std::string name = "Empty";
	DWORD pid = 0;
	DWORD pidParent = 0;
	SIZE_T memory = 0;
	double cpuUsage = 0;

	// Cpu tracking fields
	ULARGE_INTEGER CPUlast, CPUlastSys, CPUlastUser;
	int processors = 0;
	bool init = false;


	// Methods
	HANDLE get_handle();
};

class Taskm
{
private:
    std::vector<Task> taskList;

	//CPU TOTAL
	PDH_HQUERY cpuQuery;
	PDH_HCOUNTER cpuTotal;

    static std::string wchar_to_string(WCHAR* wch); // Объявление статичного метода
    void init_taskList();
    void update_cpuUsage(Task& task);

public:
    Taskm() = default;

    TASKM_ERROR update();
    TASKM_ERROR print();
    TASKM_ERROR save_json(); // Можно оставить, если сохранение в файл еще нужно
    std::vector<Task> get();
    nlohmann::json get_json_object(); // <-- НОВЫЙ МЕТОД

	// totals
	TASKM_ERROR save_json_totals();
	nlohmann::json get_json_totals();

	//CPU Total
	void CPU_total_init();
	double CPU_total_get();

    Taskm(Taskm const&) = delete;
    void operator= (Taskm const&) = delete;
};