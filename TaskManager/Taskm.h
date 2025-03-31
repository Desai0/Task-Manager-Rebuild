#pragma once

#include <Windows.h>
#include <vector>
#include <Psapi.h>
#include <string>

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

// Probably not gonna use that, but maybe, so commented for now
//struct TaskList {
//	std::vector<Task> taskList;
//
//	Task operator[] (int idx)
//	{
//		return taskList[idx];
//	}
//
//	void push_back(Task t)
//	{
//		taskList.push_back(t);
//	}
//
//	int size() { return taskList.size(); }
//	bool empty() { return taskList.empty(); }
//};

class Taskm
{
private:
    std::vector<Task> taskList;
    static std::string wchar_to_string(WCHAR* wch); // Объявление статичного метода
	void init_taskList();
	void update_cpuUsage(Task & task);

public:
	Taskm() = default;

    TASKM_ERROR update(); 
    TASKM_ERROR print();  
    TASKM_ERROR save_json(); 
    std::vector<Task> get(); 


    Taskm(Taskm const&) = delete;
    void operator= (Taskm const&) = delete;
};