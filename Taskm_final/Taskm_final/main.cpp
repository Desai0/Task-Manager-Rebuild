#include <winsock2.h>
#include <tchar.h>
#include "Taskm.h"

#define SVCNAME TEXT("Taskm")

// A LOT of code is from:
// https://learn.microsoft.com/en-us/windows/win32/services/svc-cpp


int main()
{
	Taskm task;
	task.update();
	task.print();
}

//SERVICE_STATUS          gSvcStatus;
//SERVICE_STATUS_HANDLE   gSvcStatusHandle;
//HANDLE                  ghSvcStopEvent = NULL;
//
//VOID SvcInstall(void);
//VOID WINAPI SvcCtrlHandler(DWORD);
//VOID WINAPI SvcMain(DWORD, LPTSTR*);
//
//VOID ReportSvcStatus(DWORD, DWORD, DWORD);
//VOID SvcInit(DWORD, LPTSTR*);
//VOID SvcReportEvent(LPTSTR);
//
//
//
//int _cdecl _tmain () {}
//
//
//
//
//
//VOID ReportSvcStatus(DWORD dwCurrentState,
//    DWORD dwWin32ExitCode,
//    DWORD dwWaitHint)
//{
//    static DWORD dwCheckPoint = 1;
//
//    // Fill in the SERVICE_STATUS structure.
//
//    gSvcStatus.dwCurrentState = dwCurrentState;
//    gSvcStatus.dwWin32ExitCode = dwWin32ExitCode;
//    gSvcStatus.dwWaitHint = dwWaitHint;
//
//    if (dwCurrentState == SERVICE_START_PENDING)
//        gSvcStatus.dwControlsAccepted = 0;
//    else gSvcStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;
//
//    if ((dwCurrentState == SERVICE_RUNNING) ||
//        (dwCurrentState == SERVICE_STOPPED))
//        gSvcStatus.dwCheckPoint = 0;
//    else gSvcStatus.dwCheckPoint = dwCheckPoint++;
//
//    // Report the status of the service to the SCM.
//    SetServiceStatus(gSvcStatusHandle, &gSvcStatus);
//}