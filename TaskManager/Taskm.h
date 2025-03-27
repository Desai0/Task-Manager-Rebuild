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
	std::string name;
	DWORD pid;
	DWORD pidParent;
	SIZE_T memory;
};

struct TaskList {
	std::vector<Task> taskList;

	Task operator[] (int idx)
	{
		return taskList[idx];
	}

	void push_back(Task t)
	{
		taskList.push_back(t);
	}

	int size() { return taskList.size(); }
	bool empty() { return taskList.empty(); }
};

class Taskm
{
private:
    std::vector<Task> taskList;
    static std::string wchar_to_string(WCHAR* wch); // Объявление статичного метода

public:
    Taskm() = default;

    TASKM_ERROR update(); 
    TASKM_ERROR print();  
    TASKM_ERROR save_json(); 
    std::vector<Task> get(); 


    Taskm(Taskm const&) = delete;
    void operator= (Taskm const&) = delete;
};