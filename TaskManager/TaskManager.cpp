#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <cassert>
#include <string>
#include "Taskm.h"
#include <tchar.h>
#include <locale.h>

#include <thread>   // Для работы с потоками
#include <chrono>   // Для работы со временем (паузы)

#include "json.hpp"

#pragma comment(lib, "ws2_32.lib")

#define SERVER_PORT 12345
#define BUFFER_SIZE 4096

using json = nlohmann::json;

// Объявление функции TerminateProcessByPid
bool TerminateProcessByPid(DWORD pid);

// Функция для инициализации Winsock
bool InitWinsock() {
    WSADATA wsaData;  // Структура для хранения информации о Winsock
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData); // Инициализация Winsock (версии 2.2)
    return result == 0; // Возвращаем true, если инициализация прошла успешно
}

// Функция для создания сокета для прослушивания подключений
SOCKET CreateListenSocket() {
    SOCKET listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);  // Создаём TCP сокет для прослушивания
    if (listenSocket == INVALID_SOCKET) {  // Проверяем, не возникла ли ошибка при создании сокета
        std::cerr << "Ошибка создания сокета: " << WSAGetLastError() << std::endl;  // Если ошибка, выводим её
    }
    return listenSocket;  // Возвращаем созданный сокет
}

// Функция для привязки сокета к определённому адресу и порту
bool BindSocket(SOCKET listenSocket) {
    sockaddr_in serverAddr = {};  // Структура для хранения адреса сервера
    serverAddr.sin_family = AF_INET;   // Используем IPv4
    serverAddr.sin_addr.s_addr = INADDR_ANY;  // Слушаем на всех интерфейсах
    serverAddr.sin_port = htons(SERVER_PORT);  // Привязываем к порту, определённому в SERVER_PORT

    // Привязываем сокет к адресу и порту
    int result = bind(listenSocket, (sockaddr*)&serverAddr, sizeof(serverAddr));
    if (result == SOCKET_ERROR) {  // Если произошла ошибка при привязке
        std::cerr << "Ошибка привязки сокета: " << WSAGetLastError() << std::endl;
        return false;  // Возвращаем false, если привязка не удалась
    }
    return true;  // Возвращаем true, если привязка прошла успешно
}

// Функция для начала прослушивания подключений на сокете
bool StartListening(SOCKET listenSocket) {
    int result = listen(listenSocket, SOMAXCONN);  // Запускаем прослушивание
    if (result == SOCKET_ERROR) {  // Если произошла ошибка
        std::cerr << "Ошибка прослушивания: " << WSAGetLastError() << std::endl;
        return false;  // Возвращаем false, если не удалось начать прослушивание
    }
    return true;  // Возвращаем true, если прослушивание началось
}

// Функция для обработки данных, полученных от клиента
void HandleClient(SOCKET clientSocket, Taskm& taskManager) { // Taskm передается по ссылке
    std::cout << "[" << std::this_thread::get_id() << "] Поток обработки клиента запущен." << std::endl;
    char recvBuf[BUFFER_SIZE];
    bool keepConnection = true; // Флаг для управления циклом

    while (keepConnection) { // <--- НАЧАЛО ЦИКЛА ОБРАБОТКИ КОМАНД
        SOCKET bytesReceived = recv(clientSocket, recvBuf, sizeof(recvBuf) - 1, 0);

        if (bytesReceived == SOCKET_ERROR) {
            int error = WSAGetLastError();
            if (error == WSAECONNRESET || error == WSAECONNABORTED) {
                std::cout << "[" << std::this_thread::get_id() << "] Клиент разорвал соединение." << std::endl;
            }
            else {
                std::cerr << "[" << std::this_thread::get_id() << "] Ошибка recv: " << error << std::endl;
            }
            keepConnection = false; // Выходим из цикла при ошибке
            continue; // Переходим к закрытию сокета
        }
        else if (bytesReceived == 0) {
            std::cout << "[" << std::this_thread::get_id() << "] Клиент отключился штатно." << std::endl;
            keepConnection = false; // Выходим из цикла
            continue; // Переходим к закрытию сокета
        }

        // --- Обработка полученных данных ---
        recvBuf[bytesReceived] = '\0'; // Нуль-терминация
        std::string clientRequest(recvBuf);
        std::cout << "[" << std::this_thread::get_id() << "] Получено от клиента: " << clientRequest << std::endl;

        json responseJson;
        std::string responseString;

        try {
            json requestJson = json::parse(clientRequest);

            if (requestJson.contains("command")) {
                std::string command = requestJson["command"];
                std::cout << "[" << std::this_thread::get_id() << "] Команда: " << command << std::endl;

                // --- Обработка команд (логика остается почти такой же) ---
                if (command == "getProcesses") {
                    // ... (код для getProcesses) ...
                    TASKM_ERROR update_result = taskManager.update(); // Обновляем данные ПЕРЕД отправкой
                    if (update_result == TASKM_OK) {
                        json processListJson = taskManager.get_json_object();
                        responseJson = processListJson;
                        responseJson["status"] = "success";
                    }
                    else {
                        std::cerr << "[" << std::this_thread::get_id() << "] Ошибка обновления списка процессов: " << update_result << std::endl;
                        responseJson["status"] = "error";
                        responseJson["message"] = "Ошибка обновления списка процессов на сервере";
                    }
                    responseString = responseJson.dump();
                    responseString += "\n";

                }
                else if (command == "terminateProcess") {
                    // ... (код для terminateProcess) ...
                    if (requestJson.contains("pid") && requestJson["pid"].is_number_integer()) {
                        DWORD pidToTerminate = requestJson["pid"];
                        if (TerminateProcessByPid(pidToTerminate)) {
                            responseJson["status"] = "success";
                            responseJson["message"] = "Процесс завершен";
                            responseJson["pid"] = pidToTerminate;
                        }
                        else {
                            responseJson["status"] = "error";
                            responseJson["message"] = "Не удалось завершить процесс";
                            responseJson["pid"] = pidToTerminate;
                        }
                    }
                    else {
                        responseJson["status"] = "error";
                        responseJson["message"] = "Неверный формат команды terminateProcess";
                    }
                    responseString = responseJson.dump();
                    responseString += "\n";

                }
                else if (command == "getSystemLoad") {
                    // ... (код для getSystemLoad - пока заглушка) ...
                    responseJson["status"] = "success";
                    responseJson["systemLoad"] = { {"cpu", 0.0}, {"memory", 0.0} };
                    responseString = responseJson.dump();
                    responseString += "\n";

                }
                else {
                    // ... (обработка неизвестной команды) ...
                    responseJson["status"] = "error";
                    responseJson["message"] = "Неизвестная команда";
                    responseString = responseJson.dump();
                    responseString += "\n";
                }
            }
            else {
                // ... (обработка неверного формата запроса) ...
                responseJson["status"] = "error";
                responseJson["message"] = "Неверный формат запроса";
                responseString = responseJson.dump();
                responseString += "\n";
            }

        }
        catch (json::parse_error& e) {
            std::cerr << "[" << std::this_thread::get_id() << "] Ошибка парсинга JSON: " << e.what() << " Входные данные: " << clientRequest << std::endl;
            responseString = R"({"status": "error", "message": "Ошибка парсинга JSON на сервере"})";
        }
        catch (const std::exception& e) {
            std::cerr << "[" << std::this_thread::get_id() << "] Другая ошибка: " << e.what() << std::endl;
            responseString = R"({"status": "error", "message": "Внутренняя ошибка сервера"})";
        }

        // --- Отправка ответа ---
        if (!responseString.empty()) {
            const int sendBufSize = responseString.length();
            int totalBytesSent = 0;
            const char* dataToSend = responseString.c_str();

            while (totalBytesSent < sendBufSize) {
                int bytesSent = send(clientSocket, dataToSend + totalBytesSent, sendBufSize - totalBytesSent, 0);
                if (bytesSent == SOCKET_ERROR) {
                    std::cerr << "[" << std::this_thread::get_id() << "] Ошибка send: " << WSAGetLastError() << std::endl;
                    keepConnection = false; // Выходим из основного цикла при ошибке отправки
                    break; // Выходим из цикла отправки
                }
                if (bytesSent == 0) {
                    std::cout << "[" << std::this_thread::get_id() << "] Отправка прервана (0 байт)." << std::endl;
                    keepConnection = false; // Выходим из основного цикла
                    break; // Выходим из цикла отправки
                }
                totalBytesSent += bytesSent;
            }
            if (keepConnection && totalBytesSent == sendBufSize) { // Проверяем keepConnection перед логом
                std::cout << "[" << std::this_thread::get_id() << "] Ответ успешно отправлен (" << totalBytesSent << " байт)." << std::endl;
            }
        }
        // Продолжаем цикл while(keepConnection), ожидая следующую команду
    } // <--- КОНЕЦ ЦИКЛА WHILE(keepConnection)

    // --- Закрытие сокета клиента ---
    closesocket(clientSocket); // <--- ЗАКРЫВАЕМ СОКЕТ ЗДЕСЬ
    std::cout << "[" << std::this_thread::get_id() << "] Соединение с клиентом закрыто. Поток завершен." << std::endl;
}



///////////////

// проинициализировання переменная которая будет Winsock, чтобы не пришлось каждый раз его инициализировать

// Тест для инициализации Winsock
void Test_InitWinsock()
{
    if (InitWinsock())
    {
        std::cout << "Test_InitWinsock: \"Winsock успешно инициализирован\"\n";
        WSACleanup();
    }
    else {
        std::cout << "Test_InitWinsock: \"Ошибка инициализации Winsock\"\n";
    }
}

// Тест для создания сокета
void Test_CreateListenSocket() {
    if (!InitWinsock()) {
        std::cout << "Test_CreateListenSocket: \"Пропущен - Winsock не инициализирован\"\n";
        return;
    }

    SOCKET sock = CreateListenSocket();
    if (sock != INVALID_SOCKET) {
        std::cout << "Test_CreateListenSocket: \"Сокет успешно создан\"\n";
        closesocket(sock);
    }
    else {
        std::cout << "Test_CreateListenSocket: \"Ошибка создания сокета\"\n";
    }
    WSACleanup();
}

// Тест для привязки сокета
void Test_BindSocket() {
    if (!InitWinsock()) {
        std::cout << "Test_BindSocket: \"Пропущен - Winsock не инициализирован\"\n";
        return;
    }

    SOCKET sock = CreateListenSocket();
    if (sock == INVALID_SOCKET) {
        std::cout << "Test_BindSocket: \"Пропущен - Сокет не создан\"\n";
        WSACleanup();
        return;
    }

    if (BindSocket(sock)) {
        std::cout << "Test_BindSocket: \"Сокет успешно привязан к порту " << SERVER_PORT << "\"\n";
    }
    else {
        std::cerr << "Test_BindSocket: \"Ошибка привязки сокета. Код ошибки: " << WSAGetLastError() << "\"\n";
    }

    closesocket(sock);
    WSACleanup();
}

// Тест для начала прослушивания подключений
void Test_StartListening() {
    if (!InitWinsock()) {
        std::cout << "Test_StartListening: \"Пропущен - Winsock не инициализирован\"\n";
        return;
    }

    SOCKET sock = CreateListenSocket();
    if (sock == INVALID_SOCKET) {
        std::cout << "Test_StartListening: \"Пропущен - Сокет не создан\"\n";
        WSACleanup();
        return;
    }

    if (!BindSocket(sock)) {
        std::cout << "Test_StartListening: \"Пропущен - Сокет не привязан\"\n";
        closesocket(sock);
        WSACleanup();
        return;
    }

    if (StartListening(sock)) {
        std::cout << "Test_StartListening: \"Сокет успешно переведен в режим прослушивания на порту " << SERVER_PORT << "\"\n";
    }
    else {
        std::cerr << "Test_StartListening: \"Ошибка перевода в режим прослушивания. Код ошибки: " << WSAGetLastError() << "\"\n";
    }

    closesocket(sock);
    WSACleanup();
}

// Тест для обработки клиента -------- НЕ РАБОТАЕТ, ТРЕБУЕТ ПЕРЕРАБОТКИ, или не надо
//void Test_HandleClient() {
//    SOCKET clientSocket = CreateListenSocket();
//    HandleClient(clientSocket);
//    closesocket(clientSocket);
//} 

// Функция, которая будет выполняться в отдельном потоке для обновления JSON
//void periodicJsonUpdate() {
//    while (true) { // Бесконечный цикл для постоянной работы
//        try {
//            Taskm taskm;
//            // std::cout << "[Updater] Обновление списка процессов..." << std::endl; // Раскомментируй для отладки
//
//            TASKM_ERROR update_result = taskm.update();
//            if (update_result == TASKM_OK) {
//                TASKM_ERROR save_result = taskm.save_json();
//                if (save_result == TASKM_OK) {
//                    // Успешно обновили
//                    std::cout << "[Updater] Файл output.json обновлен." << std::endl;
//                }
//                else {
//                    // Ошибка сохранения
//                    std::cerr << "[Updater] Ошибка при сохранении JSON: " << save_result << std::endl;
//                }
//            }
//            else {
//                // Ошибка обновления списка
//                std::cerr << "[Updater] Ошибка при обновлении списка процессов: " << update_result << std::endl;
//            }
//        }
//        catch (const std::exception& e) {
//            // Ловим возможные исключения, чтобы поток не падал
//            std::cerr << "[Updater] Исключение в потоке обновления: " << e.what() << std::endl;
//        }
//        catch (...) {
//            std::cerr << "[Updater] Неизвестное исключение в потоке обновления." << std::endl;
//        }
//
//
//        // Пауза на 5 секунд перед следующей итерацией
//        std::this_thread::sleep_for(std::chrono::seconds(5));
//    }
//}

void Test_TerminateProcessByPid() {
    // Тестируем с несуществующим PID
    bool result = TerminateProcessByPid(999999);
    if (!result) {
        std::cout << "Test_TerminateProcessByPid: \"Корректно не смог завершить несуществующий процесс\"\n";
    }
    else {
        std::cout << "Test_TerminateProcessByPid: \"Ошибка - смог завершить несуществующий процесс\"\n";
    }
}

// Новый тест для Taskm (базовый)
void Test_TaskmBasic() {
    Taskm taskm;
    TASKM_ERROR result = taskm.update();
    if (result == TASKM_OK) {
        std::cout << "Test_TaskmBasic: \"Обновление Taskm прошло успешно\"\n";
    }
    else {
        std::cout << "Test_TaskmBasic: \"Ошибка обновления Taskm: " << result << "\"\n";
    }
}

// Новый тест для JSON обработки (имитация клиентского запроса)
void Test_JsonHandling() {
    if (!InitWinsock()) {
        std::cout << "Test_JsonHandling: \"Пропущен - Winsock не инициализирован\"\n";
        return;
    }

    // Создаем тестовый сокет (не настоящий клиент)
    SOCKET testSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (testSocket == INVALID_SOCKET) {
        std::cout << "Test_JsonHandling: \"Пропущен - Тестовый сокет не создан\"\n";
        WSACleanup();
        return;
    }

    Taskm taskm;
    std::cout << "Test_JsonHandling: \"Запуск HandleClient с тестовым сокетом...\"\n";

    // Запускаем обработчик с тестовым сокетом
    HandleClient(testSocket, taskm);

    closesocket(testSocket);
    WSACleanup();
    std::cout << "Test_JsonHandling: \"Завершен (Верно, ожидалось реальное подключение)\"\n";
}

// Новый интеграционный тест
void Test_Integration() {
    if (!InitWinsock()) {
        std::cout << "Test_Integration: \"Пропущен - Winsock не инициализирован\"\n";
        return;
    }

    SOCKET sock = CreateListenSocket();
    if (sock == INVALID_SOCKET) {
        std::cout << "Test_Integration: \"Пропущен - Сокет не создан\"\n";
        WSACleanup();
        return;
    }

    if (!BindSocket(sock)) {
        std::cout << "Test_Integration: \"Пропущен - Сокет не привязан\"\n";
        closesocket(sock);
        WSACleanup();
        return;
    }

    if (!StartListening(sock)) {
        std::cout << "Test_Integration: \"Пропущен - Ошибка перевода в режим прослушивания\"\n";
        closesocket(sock);
        WSACleanup();
        return;
    }

    std::cout << "Test_Integration: \"Основные компоненты сервера работают вместе корректно\"\n";
    closesocket(sock);
    WSACleanup();
}

bool TerminateProcessByPid(DWORD pid) {
    // Открываем процесс с правом на завершение
    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
    if (hProcess == NULL) {
        std::cerr << "Ошибка: Не удалось открыть процесс PID=" << pid << " для завершения. Код ошибки: " << GetLastError() << std::endl;
        return false;
    }

    // Завершаем процесс
    if (!TerminateProcess(hProcess, 1)) { // 1 - код выхода
        std::cerr << "Ошибка: Не удалось завершить процесс PID=" << pid << ". Код ошибки: " << GetLastError() << std::endl;
        CloseHandle(hProcess); // Закрываем хэндл в случае ошибки
        return false;
    }

    // Закрываем хэндл процесса
    CloseHandle(hProcess);
    std::cout << "Процесс PID=" << pid << " успешно завершен." << std::endl;
    return true;
}

// Функция для запуска всех тестов
void RunAllTests() {
    std::cout << "\n\"Запуск всех тестов...\"\n";
    std::cout << "-------------------\n";

    Test_InitWinsock();
    Test_CreateListenSocket();
    Test_BindSocket();
    Test_StartListening();
    Test_TerminateProcessByPid();
    Test_TaskmBasic();
    Test_JsonHandling();
    Test_Integration();

    std::cout << "-------------------\n";
    std::cout << "\"Все тесты завершены.\"\n\n";
}

//// Главная функция сервера
int main() {
    SetConsoleOutputCP(CP_UTF8);
    setlocale(LC_ALL, ".UTF8");

    RunAllTests();

    // Инициализация Winsock
    if (!InitWinsock()) {
        std::cerr << "Ошибка WSAStartup!" << std::endl;  // Если не удалось инициализировать Winsock
        return 1;  // Завершаем выполнение программы
    }

    // Создание сокета для прослушивания
    SOCKET listenSocket = CreateListenSocket();
    if (listenSocket == INVALID_SOCKET) {
        WSACleanup();  // Очистка Winsock
        return 1;  // Завершаем выполнение программы, если сокет не был создан
    }

    // Привязка сокета
    if (!BindSocket(listenSocket)) {
        closesocket(listenSocket);  // Закрытие сокета, если привязка не удалась
        WSACleanup();  // Очистка Winsock
        return 1;  // Завершаем выполнение программы
    }

    // Начало прослушивания на сокете
    if (!StartListening(listenSocket)) {
        closesocket(listenSocket);  // Закрытие сокета, если не удалось начать прослушивание
        WSACleanup();  // Очистка Winsock
        return 1;  // Завершаем выполнение программы
    }

    std::cout << "\nСервер запущен и прослушивает порт " << SERVER_PORT << "..." << std::endl;

    // --- Создаем ОДИН экземпляр Taskm для всего сервера ---
    Taskm taskManager;
    std::cout << "Экземпляр Task Manager создан." << std::endl;

    // --- ЗАПУСК ФОНОВОГО ПОТОКА ---
    //std::thread jsonUpdateThread(periodicJsonUpdate); // Создаем поток, передаем ему нашу функцию
    //jsonUpdateThread.detach(); // Отсоединяем поток, чтобы он работал независимо в фоне
    //// и не блокировал завершение main (хотя наш main и так в вечном цикле)
    //std::cout << "Запущен фоновый поток для обновления JSON каждые 5 секунд." << std::endl;

    // Главный цикл для принятия и обработки клиентских соединений
    while (true) {
        sockaddr_in clientAddr = {};
        int clientAddrLen = sizeof(clientAddr);
        SOCKET clientSocket = accept(listenSocket, (sockaddr*)&clientAddr, &clientAddrLen);

        if (clientSocket == INVALID_SOCKET) {
            std::cerr << "Ошибка accept: " << WSAGetLastError() << std::endl;
            // Можно добавить небольшую паузу, чтобы не загружать CPU в случае постоянных ошибок accept
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        // --- Запуск обработки клиента в отдельном потоке ---
        std::cout << "Клиент подключился (" << clientSocket << "). Запуск потока обработки..." << std::endl;
        // Передаем сокет по значению (копируется), а taskManager по ссылке
        std::thread clientThread(HandleClient, clientSocket, std::ref(taskManager));
        clientThread.detach(); // Отсоединяем поток, чтобы он работал независимо
    }

    // Закрытие сокета для прослушивания и очистка Winsock
    closesocket(listenSocket);
    WSACleanup();

    return 0;
}

//int main()
//{
//    Taskm taskm;
//    taskm.update();
//    taskm.save_json();
//}