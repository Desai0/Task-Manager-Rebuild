#pragma once

#include <Windows.h>
#include <TlHelp32.h>
#include <tchar.h>
#include <vector>
#include <Psapi.h>
#include <string>
#include <iostream>

#define MB_SIZE 1000000


enum TASKM_ERROR
{
	TASKM_OK,
	FIRSTPROCESS_ERROR,

	TASKM_GENERIC_ERROR
};

struct Task
{
	std::string name;
	DWORD pid;
	DWORD pidParent;
	SIZE_T memory;

};


// NOT SINGLETON, CAN CAUSE ERRORS AS SERVICE OTHERVISE
class Taskm
{
private:
	std::vector<Task> taskList;

public:

	Taskm() = default;

	TASKM_ERROR update()
	{
		HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

		PROCESSENTRY32 entry;
		entry.dwSize = sizeof(PROCESSENTRY32);

		if (!Process32First(snap, &entry))
		{
			return FIRSTPROCESS_ERROR;
		}

		
		do
		{
			// Creating task struct to fill up
			Task task;


			// Memory
			HANDLE procHandle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, entry.th32ProcessID);
			PROCESS_MEMORY_COUNTERS_EX mem;
			GetProcessMemoryInfo(procHandle, (PROCESS_MEMORY_COUNTERS*)& mem, sizeof(mem));
			SIZE_T physMem = mem.WorkingSetSize;
			long double physMemMB = physMem / MB_SIZE;
			CloseHandle(procHandle);
			
			task.memory = physMemMB;

			//CPU



			// Process Idendification
			task.name = Taskm::wchar_to_string(entry.szExeFile);
			task.pid = entry.th32ProcessID;
			task.pidParent = entry.th32ParentProcessID;

			//_tprintf(TEXT("Name: %s, PID: %d \n"), entry.szExeFile, entry.th32ProcessID);
			/*procModules(entry.th32ProcessID);*/

			//Saving what we did above
			taskList.push_back(task);

		} while (Process32Next(snap, &entry));

		CloseHandle(snap);
		return TASKM_OK;
	}

	TASKM_ERROR print()
	{
		if (taskList.empty())
		{
			return TASKM_GENERIC_ERROR;
		}
		else
		{
			size_t size = taskList.size();

			for (Task task : taskList)
			{
				std::cout << "Name: " << task.name << "; PID: " << task.pid << "; Parent: "  << task.pidParent << "; RAM: " << task.memory << std::endl;

			}
			return TASKM_OK;
		}
	}

	static std::string wchar_to_string(WCHAR* wch)
	{
		std::wstring wstr(wch);
		return std::string(wstr.begin(), wstr.end());
	}


	Taskm(Taskm const&) = delete;
	void operator= (Taskm const&) = delete;
};