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

#include <iomanip>
#include <ctime>

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
void HandleClient(SOCKET clientSocket, Taskm& taskManager) {
    std::cout << "[" << std::this_thread::get_id() << "] Поток обработки клиента запущен." << std::endl;
    char recvBuf[BUFFER_SIZE];
    bool keepConnection = true;
    std::string messageBuffer = ""; // <--- БУФЕР ДЛЯ НАКОПЛЕНИЯ ДАННЫХ КЛИЕНТА

    while (keepConnection) {
        int bytesReceived = recv(clientSocket, recvBuf, sizeof(recvBuf), 0); // Убрали -1, будем использовать bytesReceived

        if (bytesReceived == SOCKET_ERROR) {
            // ... (обработка ошибок recv как была) ...
            keepConnection = false;
            continue;
        }
        else if (bytesReceived == 0) {
            // ... (обработка отключения как была) ...
            keepConnection = false;
            continue;
        }

        // Добавляем полученные байты в буфер
        messageBuffer.append(recvBuf, bytesReceived);
        // std::cout << "[" << std::this_thread::get_id() << "] Bytes received: " << bytesReceived << ", Buffer now: " << messageBuffer << std::endl; // Отладка

        size_t newlinePos;
        // Обрабатываем все полные сообщения в буфере
        while ((newlinePos = messageBuffer.find('\n')) != std::string::npos) {
            std::string completeMessage = messageBuffer.substr(0, newlinePos); // Извлекаем сообщение до \n
            messageBuffer.erase(0, newlinePos + 1); // Удаляем сообщение и \n из буфера

            // std::cout << "[" << std::this_thread::get_id() << "] Processing message: " << completeMessage << std::endl; // Отладка

            // Пропускаем пустые сообщения, если они вдруг образовались
            if (completeMessage.empty()) continue;

            json responseJson;
            std::string responseString;

            // --- ОБРАБОТКА ИЗВЛЕЧЕННОГО СООБЩЕНИЯ ---
            try {
                json requestJson = json::parse(completeMessage); // Парсим ТОЛЬКО извлеченное сообщение

                if (requestJson.contains("command")) {
                    std::string command = requestJson["command"];
                    std::cout << "[" << std::this_thread::get_id() << "] Команда: " << command << std::endl;

                    // --- Обработка команд (логика та же) ---
                    if (command == "getProcesses") {
                        TASKM_ERROR update_result = taskManager.update();
                        if (update_result == TASKM_OK) {
                            json processListJson = taskManager.get_json_object();
                            responseJson = processListJson;
                            responseJson["status"] = "success";
                        }
                        else { /*...*/ responseJson["status"] = "error"; responseJson["message"] = "..."; }
                        responseString = responseJson.dump() + "\n";

                    }
                    else if (command == "terminateProcess") {
                        if (requestJson.contains("pid") && requestJson["pid"].is_number_integer()) {
                            DWORD pidToTerminate = requestJson["pid"];

                            auto time_before = std::chrono::high_resolution_clock::now();
                            bool success = TerminateProcessByPid(pidToTerminate);
                            // ЛОГ ВРЕМЕНИ ПОСЛЕ ВЫЗОВА ЗАВЕРШЕНИЯ
                            auto time_after = std::chrono::high_resolution_clock::now();
                            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(time_after - time_before);
                            std::cout << "    -> TerminateProcessByPid завершился за " << duration.count() << " мс. Результат: " << (success ? "успех" : "неудача") << std::endl;

                            if (TerminateProcessByPid(pidToTerminate)) { /*...*/ responseJson["status"] = "success"; responseJson["message"] = "..."; responseJson["pid"] = pidToTerminate; }
                            else { /*...*/ responseJson["status"] = "error"; responseJson["message"] = "..."; responseJson["pid"] = pidToTerminate; }
                        }
                        else { /*...*/ responseJson["status"] = "error"; responseJson["message"] = "..."; }
                        responseString = responseJson.dump() + "\n";

                    }
                    else if (command == "ping") {
                        responseJson["status"] = "pong";
                        responseString = responseJson.dump() + "\n";

                    }
                    else if (command == "getSystemLoad") {
                        responseJson["status"] = "success"; // Заглушка
                        responseJson["systemLoad"] = { {"cpu", 0.0}, {"memory", 0.0} };
                        responseString = responseJson.dump() + "\n";
                    }
                    else {
                        responseJson["status"] = "error"; responseJson["message"] = "Неизвестная команда";
                        responseString = responseJson.dump() + "\n";
                    }
                }
                else {
                    responseJson["status"] = "error"; responseJson["message"] = "Неверный формат запроса";
                    responseString = responseJson.dump() + "\n";
                }

            }
            catch (json::parse_error& e) {
                std::cerr << "[" << std::this_thread::get_id() << "] Ошибка парсинга JSON от клиента: " << e.what() << std::endl;
                std::cerr << "Проблемное сообщение: " << completeMessage << std::endl;
                responseString = R"({"status": "error", "message": "Ошибка парсинга JSON на сервере"})" "\n"; // Добавляем \n и сюда
            }
            catch (const std::exception& e) {
                std::cerr << "[" << std::this_thread::get_id() << "] Другая ошибка при обработке запроса: " << e.what() << std::endl;
                responseString = R"({"status": "error", "message": "Внутренняя ошибка сервера"})" "\n"; // Добавляем \n
            }
            // --- КОНЕЦ ОБРАБОТКИ ИЗВЛЕЧЕННОГО СООБЩЕНИЯ ---

            // --- Отправка ответа ---
            if (!responseString.empty()) {
                const int sendBufSize = responseString.length();
                int totalBytesSent = 0;
                const char* dataToSend = responseString.c_str();

                while (totalBytesSent < sendBufSize) {
                    int bytesSent = send(clientSocket, dataToSend + totalBytesSent, sendBufSize - totalBytesSent, 0);
                    if (bytesSent == SOCKET_ERROR) { /* ... обработка ошибки send ... */ keepConnection = false; break; }
                    if (bytesSent == 0) { /* ... */ keepConnection = false; break; }
                    totalBytesSent += bytesSent;
                }
                if (keepConnection && totalBytesSent == sendBufSize) {
                    // Успешно отправили, можно логировать
                    // std::cout << "[" << std::this_thread::get_id() << "] Ответ успешно отправлен (" << totalBytesSent << " байт)." << std::endl;
                }
            }
            // Цикл while для поиска \n продолжится, если в буфере остались данные
        } // Конец while ((newlinePos = ...))
        // Данные без \n или остаток данных после последнего \n остаются в messageBuffer
    } // Конец while (keepConnection)

    closesocket(clientSocket);
    std::cout << "[" << std::this_thread::get_id() << "] Соединение с клиентом закрыто. Поток завершен." << std::endl;
}



///////////////

// проинициализировання переменная которая будет Winsock, чтобы не пришлось каждый раз его инициализировать

// Тест для инициализации Winsock
void Test_InitWinsock()
{
    if (InitWinsock())
    {
        std::cout << "Test_InitWinsock: \"Winsock успешно инициализирован\"\n" << std::endl;
        WSACleanup();
    }
    else {
        std::cout << "Test_InitWinsock: \"Ошибка инициализации Winsock\"\n" << std::endl;
    }
}

// Тест для создания сокета
void Test_CreateListenSocket() {
    if (!InitWinsock()) {
        std::cout << "Test_CreateListenSocket: \"Пропущен - Winsock не инициализирован\"\n" << std::endl;
        return;
    }

    SOCKET sock = CreateListenSocket();
    if (sock != INVALID_SOCKET) {
        std::cout << "Test_CreateListenSocket: \"Сокет успешно создан\"\n" << std::endl;
        closesocket(sock);
    }
    else {
        std::cout << "Test_CreateListenSocket: \"Ошибка создания сокета\"\n" << std::endl;
    }
    WSACleanup();
}

// Тест для привязки сокета
void Test_BindSocket() {
    if (!InitWinsock()) {
        std::cout << "Test_BindSocket: \"Пропущен - Winsock не инициализирован\"\n" << std::endl;
        return;
    }

    SOCKET sock = CreateListenSocket();
    if (sock == INVALID_SOCKET) {
        std::cout << "Test_BindSocket: \"Пропущен - Сокет не создан\"\n" << std::endl;
        WSACleanup();
        return;
    }

    if (BindSocket(sock)) {
        std::cout << "Test_BindSocket: \"Сокет успешно привязан к порту " << SERVER_PORT << "\"\n" << std::endl;
    }
    else {
        std::cerr << "Test_BindSocket: \"Ошибка привязки сокета. Код ошибки: " << WSAGetLastError() << "\"\n" << std::endl;
    }

    closesocket(sock);
    WSACleanup();
}

// Тест для начала прослушивания подключений
void Test_StartListening() {
    if (!InitWinsock()) {
        std::cout << "Test_StartListening: \"Пропущен - Winsock не инициализирован\"\n" << std::endl;
        return;
    }

    SOCKET sock = CreateListenSocket();
    if (sock == INVALID_SOCKET) {
        std::cout << "Test_StartListening: \"Пропущен - Сокет не создан\"\n" << std::endl;
        WSACleanup();
        return;
    }

    if (!BindSocket(sock)) {
        std::cout << "Test_StartListening: \"Пропущен - Сокет не привязан\"\n" << std::endl;
        closesocket(sock);
        WSACleanup();
        return;
    }

    if (StartListening(sock)) {
        std::cout << "Test_StartListening: \"Сокет успешно переведен в режим прослушивания на порту " << SERVER_PORT << "\"\n" << std::endl;
    }
    else {
        std::cerr << "Test_StartListening: \"Ошибка перевода в режим прослушивания. Код ошибки: " << WSAGetLastError() << "\"\n" << std::endl;
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
        std::cout << "Test_TerminateProcessByPid: \"Корректно не смог завершить несуществующий процесс\"\n" << std::endl;
    }
    else {
        std::cout << "Test_TerminateProcessByPid: \"Ошибка - смог завершить несуществующий процесс\"\n" << std::endl;
    }
}

// Новый тест для Taskm (базовый)
void Test_TaskmBasic() {
    Taskm taskm;
    TASKM_ERROR result = taskm.update();
    if (result == TASKM_OK) {
        std::cout << "Test_TaskmBasic: \"Обновление Taskm прошло успешно\"\n" << std::endl;
    }
    else {
        std::cout << "Test_TaskmBasic: \"Ошибка обновления Taskm: " << result << "\"\n" << std::endl;
    }
}

// Новый тест для JSON обработки (имитация клиентского запроса)
void Test_JsonHandling() {
    if (!InitWinsock()) {
        std::cout << "Test_JsonHandling: \"Пропущен - Winsock не инициализирован\"\n" << std::endl;
        return;
    }

    // Создаем тестовый сокет (не настоящий клиент)
    SOCKET testSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (testSocket == INVALID_SOCKET) {
        std::cout << "Test_JsonHandling: \"Пропущен - Тестовый сокет не создан\"\n" << std::endl;
        WSACleanup();
        return;
    }

    Taskm taskm;
    std::cout << "Test_JsonHandling: \"Запуск HandleClient с тестовым сокетом...\"\n" << std::endl;

    // Запускаем обработчик с тестовым сокетом
    HandleClient(testSocket, taskm);

    closesocket(testSocket);
    WSACleanup();
    std::cout << "Test_JsonHandling: \"Завершен (Верно, ожидалось реальное подключение)\"\n" << std::endl;
}

// Новый интеграционный тест
void Test_Integration() {
    if (!InitWinsock()) {
        std::cout << "Test_Integration: \"Пропущен - Winsock не инициализирован\"\n" << std::endl;
        return;
    }

    SOCKET sock = CreateListenSocket();
    if (sock == INVALID_SOCKET) {
        std::cout << "Test_Integration: \"Пропущен - Сокет не создан\"\n" << std::endl;
        WSACleanup();
        return;
    }

    if (!BindSocket(sock)) {
        std::cout << "Test_Integration: \"Пропущен - Сокет не привязан\"\n" << std::endl;
        closesocket(sock);
        WSACleanup();
        return;
    }

    if (!StartListening(sock)) {
        std::cout << "Test_Integration: \"Пропущен - Ошибка перевода в режим прослушивания\"\n" << std::endl;
        closesocket(sock);
        WSACleanup();
        return;
    }

    std::cout << "Test_Integration: \"Основные компоненты сервера работают вместе корректно\"\n" << std::endl;
    closesocket(sock);
    WSACleanup();
}

bool TerminateProcessByPid(DWORD pid) {
    // Открываем процесс с правом на завершение
    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE | PROCESS_QUERY_INFORMATION, FALSE, pid); // Добавили Query Info для GetExitCodeProcess
    if (hProcess == NULL) {
        std::cerr << "    -> Ошибка OpenProcess для PID=" << pid << ". Код: " << GetLastError() << std::endl;
        return false;
    }

    // Пытаемся завершить процесс
    if (!TerminateProcess(hProcess, 1)) { // 1 - код выхода
        DWORD error = GetLastError();
        std::cerr << "    -> Ошибка TerminateProcess для PID=" << pid << ". Код: " << error << std::endl;
        CloseHandle(hProcess);
        // НЕ считаем это успехом, даже если ошибка ERROR_ACCESS_DENIED
        return false;
    }

    // Если TerminateProcess вернул TRUE, это значит, запрос на завершение принят.
    // Для надежности можно подождать немного и проверить код выхода,
    // но это может заблокировать поток. Простой вариант - считать успешным.
    // std::cout << "    -> Запрос на завершение PID=" << pid << " отправлен." << std::endl; // Более корректный лог

    /* // Опциональная проверка с ожиданием (может блокировать!)
    DWORD waitResult = WaitForSingleObject(hProcess, 500); // Ждем до 500 мс
    if (waitResult == WAIT_OBJECT_0) {
         std::cout << "    -> Процесс PID=" << pid << " действительно завершился." << std::endl;
    } else if (waitResult == WAIT_TIMEOUT) {
         std::cout << "    -> Процесс PID=" << pid << " не завершился за 500 мс." << std::endl;
         // Можно считать это частичным успехом или неудачей?
    } else {
         std::cout << "    -> Ошибка ожидания завершения PID=" << pid << ". Код: " << GetLastError() << std::endl;
    }
    */

    CloseHandle(hProcess);
    return true; // Возвращаем true, если TerminateProcess вернул TRUE
}

// Функция для запуска всех тестов
void RunAllTests() {
    std::cout << "\n\"Запуск всех тестов...\"\n" << std::endl;
    std::cout << "-------------------\n" << std::endl;

    Test_InitWinsock();
    Test_CreateListenSocket();
    Test_BindSocket();
    Test_StartListening();
    Test_TerminateProcessByPid();
    Test_TaskmBasic();
    Test_JsonHandling();
    Test_Integration();

    std::cout << "-------------------\n" << std::endl;
    std::cout << "\"Все тесты завершены.\"\n\n" << std::endl;
}

//// Главная функция сервера
//int main() {
//    SetConsoleOutputCP(CP_UTF8);
//    setlocale(LC_ALL, ".UTF8");
//
//    RunAllTests();
//
//    // Инициализация Winsock
//    if (!InitWinsock()) {
//        std::cerr << "Ошибка WSAStartup!" << std::endl;  // Если не удалось инициализировать Winsock
//        return 1;  // Завершаем выполнение программы
//    }
//
//    // Создание сокета для прослушивания
//    SOCKET listenSocket = CreateListenSocket();
//    if (listenSocket == INVALID_SOCKET) {
//        WSACleanup();  // Очистка Winsock
//        return 1;  // Завершаем выполнение программы, если сокет не был создан
//    }
//
//    // Привязка сокета
//    if (!BindSocket(listenSocket)) {
//        closesocket(listenSocket);  // Закрытие сокета, если привязка не удалась
//        WSACleanup();  // Очистка Winsock
//        return 1;  // Завершаем выполнение программы
//    }
//
//    // Начало прослушивания на сокете
//    if (!StartListening(listenSocket)) {
//        closesocket(listenSocket);  // Закрытие сокета, если не удалось начать прослушивание
//        WSACleanup();  // Очистка Winsock
//        return 1;  // Завершаем выполнение программы
//    }
//
//    std::cout << "\nСервер запущен и прослушивает порт " << SERVER_PORT << "..." << std::endl;
//
//    // --- Создаем ОДИН экземпляр Taskm для всего сервера ---
//    Taskm taskManager;
//    std::cout << "Экземпляр Task Manager создан." << std::endl;
//
//    // --- ЗАПУСК ФОНОВОГО ПОТОКА ---
//    //std::thread jsonUpdateThread(periodicJsonUpdate); // Создаем поток, передаем ему нашу функцию
//    //jsonUpdateThread.detach(); // Отсоединяем поток, чтобы он работал независимо в фоне
//    //// и не блокировал завершение main (хотя наш main и так в вечном цикле)
//    //std::cout << "Запущен фоновый поток для обновления JSON каждые 5 секунд." << std::endl;
//
//    // Главный цикл для принятия и обработки клиентских соединений
//    while (true) {
//        sockaddr_in clientAddr = {};
//        int clientAddrLen = sizeof(clientAddr);
//        SOCKET clientSocket = accept(listenSocket, (sockaddr*)&clientAddr, &clientAddrLen);
//
//        if (clientSocket == INVALID_SOCKET) {
//            std::cerr << "Ошибка accept: " << WSAGetLastError() << std::endl;
//            // Можно добавить небольшую паузу, чтобы не загружать CPU в случае постоянных ошибок accept
//            std::this_thread::sleep_for(std::chrono::milliseconds(100));
//            continue;
//        }
//
//        // --- Запуск обработки клиента в отдельном потоке ---
//        std::cout << "Клиент подключился (" << clientSocket << "). Запуск потока обработки..." << std::endl;
//        // Передаем сокет по значению (копируется), а taskManager по ссылке
//        std::thread clientThread(HandleClient, clientSocket, std::ref(taskManager));
//        clientThread.detach(); // Отсоединяем поток, чтобы он работал независимо
//    }
//
//    // Закрытие сокета для прослушивания и очистка Winsock
//    closesocket(listenSocket);
//    WSACleanup();
//
//    return 0;
//}

int main()
{
    Taskm taskm;
    taskm.update();
    taskm.save_json_totals();
}