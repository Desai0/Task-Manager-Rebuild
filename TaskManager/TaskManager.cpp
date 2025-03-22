#include <windows.h>
#include "file_service.h"
#include <tchar.h>

VOID WINAPI ServiceMain(DWORD argc, LPTSTR* argv);
VOID WINAPI ControlHandler(DWORD request);

SERVICE_STATUS ServiceStatus;
SERVICE_STATUS_HANDLE hStatus;

#include <string>
#include <locale.h>

#define BUFFER_SIZE 4096
#define PIPE_NAME L"\\\\.\\pipe\\file_service_pipe"

FileService fileService; // Глобальный экземпляр сервиса

// Функция для обработки команд через Named Pipe
std::string ProcessCommand(const std::string& command) {
    // Разбор команды из формата "command:param1,param2,..."
    size_t colonPos = command.find(':');
    std::string cmd = command.substr(0, colonPos);
    std::string params = (colonPos != std::string::npos) ? command.substr(colonPos + 1) : "";

    if (cmd == "openFile") {
        bool result = fileService.openFile(params);
        return result ? "true" : "false";
    }
    else if (cmd == "closeFile") {
        bool result = fileService.closeFile();
        return result ? "true" : "false";
    }
    else if (cmd == "readFile") {
        return fileService.readFile();
    }
    else if (cmd == "writeFile") {
        bool result = fileService.writeFile(params);
        return result ? "true" : "false";
    }
    else if (cmd == "countCharacters") {
        int count = fileService.countCharacters();
        return std::to_string(count);
    }
    else if (cmd == "deleteFile") {
        bool result = fileService.deleteFile();
        return result ? "true" : "false";
    }
    // --- Добавлены новые команды ---
    else if (cmd == "findString") {
        bool result = fileService.findString(params);
        return result ? "true" : "false";
    }
    else if (cmd == "findCharacters") {
        int count = fileService.findCharacters(params);
        return std::to_string(count);
    }
    else if (cmd == "countWords") {
        int count = fileService.countWords();
        return std::to_string(count);
    }
    else if (cmd == "getFileSize") {
        long long size = fileService.getFileSize();
        return std::to_string(size);
    }
    else if (cmd == "clearFileContent") {
        bool result = fileService.clearFileContent();
        return result ? "true" : "false";
    }
    else {
        return "Error: Unknown command";
    }

    return "Error, uknown command";
}

VOID WINAPI ServiceMain(DWORD argc, LPTSTR* argv) {
    ServiceStatus.dwServiceType = SERVICE_WIN32;
    ServiceStatus.dwCurrentState = SERVICE_START_PENDING;
    ServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
    ServiceStatus.dwWin32ExitCode = 0;
    ServiceStatus.dwServiceSpecificExitCode = 0;
    ServiceStatus.dwCheckPoint = 0;
    ServiceStatus.dwWaitHint = 0;

    hStatus = RegisterServiceCtrlHandler(L"file_service", (LPHANDLER_FUNCTION)ControlHandler);
    if (hStatus == NULL) return;

    ServiceStatus.dwCurrentState = SERVICE_RUNNING;
    SetServiceStatus(hStatus, &ServiceStatus);

    while (ServiceStatus.dwCurrentState == SERVICE_RUNNING) {
        // Создание экземпляра именованного канала
        HANDLE hPipe = CreateNamedPipe(
            PIPE_NAME,                    // имя канала
            PIPE_ACCESS_DUPLEX,           // двунаправленный доступ
            PIPE_TYPE_MESSAGE |           // передача сообщений
            PIPE_READMODE_MESSAGE |       // чтение сообщений
            PIPE_WAIT,                    // блокирующий режим
            PIPE_UNLIMITED_INSTANCES,     // максимальное количество экземпляров (в однопоточном варианте это не так важно, но оставим)
            BUFFER_SIZE,                  // размер выходного буфера
            BUFFER_SIZE,                  // размер входного буфера
            0,                            // тайм-аут по умолчанию
            NULL                          // атрибуты безопасности по умолчанию
        );

        if (hPipe == INVALID_HANDLE_VALUE) {
            // Ошибка создания канала
            Sleep(1000);
            continue;
        }

        // Ожидание подключения клиента
        if (ConnectNamedPipe(hPipe, NULL) || GetLastError() == ERROR_PIPE_CONNECTED) {
            char buffer[BUFFER_SIZE];
            DWORD bytesRead, bytesWritten;

            // Обработка клиента в текущем потоке
            while (ServiceStatus.dwCurrentState == SERVICE_RUNNING) {
                // Чтение запроса от клиента
                if (!ReadFile(hPipe, buffer, BUFFER_SIZE, &bytesRead, NULL) || bytesRead == 0) {
                    break; // Клиент отключился или произошла ошибка
                }

                // Обработка команды
                buffer[bytesRead] = '\0'; // Добавляем завершающий нуль
                std::string response = ProcessCommand(buffer);

                // Отправка ответа клиенту
                if (!WriteFile(hPipe, response.c_str(), response.length() + 1, &bytesWritten, NULL)) {
                    break; // Ошибка при отправке ответа
                }
            }

            // Закрытие соединения после обработки клиента или остановки сервиса
            FlushFileBuffers(hPipe);
            DisconnectNamedPipe(hPipe);
            CloseHandle(hPipe);
        }
        else {
            // Ошибка при подключении клиента
            CloseHandle(hPipe);
        }
    }

    ServiceStatus.dwCurrentState = SERVICE_STOPPED;
    SetServiceStatus(hStatus, &ServiceStatus);
}

VOID WINAPI ControlHandler(DWORD request) {
    switch (request) {
    case SERVICE_CONTROL_STOP:
    case SERVICE_CONTROL_SHUTDOWN:
        ServiceStatus.dwCurrentState = SERVICE_STOP_PENDING;
        SetServiceStatus(hStatus, &ServiceStatus);
        ServiceStatus.dwCurrentState = SERVICE_STOPPED;
        break;
    default:
        break;
    }
    SetServiceStatus(hStatus, &ServiceStatus);
}

int main() {
    setlocale(0, "");
    SERVICE_TABLE_ENTRY ServiceTable[] = {
        {const_cast<LPWSTR>(L"file_service"), (LPSERVICE_MAIN_FUNCTION)ServiceMain},
        {NULL, NULL}
    };
    if (!StartServiceCtrlDispatcher(ServiceTable)) {
        return GetLastError();
    }
    return 0;
}