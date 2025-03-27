#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <cassert>
#include <string>
#include "Taskm.h"
#include <tchar.h>
#include <locale.h>

#pragma comment(lib, "ws2_32.lib")

#define SERVER_PORT 12345
#define BUFFER_SIZE 512


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
void HandleClient(SOCKET clientSocket) {
    char recvBuf[BUFFER_SIZE];  // Буфер для хранения данных, полученных от клиента

    // Получаем данные от клиента
    int bytesReceived = recv(clientSocket, recvBuf, sizeof(recvBuf) - 1, 0);
    if (bytesReceived > 0) {  // Если данные были получены
        recvBuf[bytesReceived] = '\0';  // Добавляем завершающий ноль к строке
        std::cout << "Получено от клиента: " << recvBuf << std::endl;  // Выводим полученные данные

        // Формируем ответ для клиента
        std::string response = "Сервер получил: " + std::string(recvBuf);

        // Отправляем ответ обратно клиенту
        int bytesSent = send(clientSocket, response.c_str(), response.length(), 0);
        if (bytesSent == SOCKET_ERROR) {  // Если ошибка при отправке данных
            std::cerr << "Ошибка отправки данных клиенту: " << WSAGetLastError() << std::endl;
        }
    }
    else if (bytesReceived == 0) {  // Если клиент отключился
        std::cout << "Клиент отключился." << std::endl;
    }
    else {  // Если произошла ошибка при получении данных
        std::cerr << "Ошибка при получении данных от клиента: " << WSAGetLastError() << std::endl;
    }
}

//// Главная функция сервера
int main() {
    setlocale(LC_ALL, "");

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

    std::cout << "Сервер запущен и прослушивает порт " << SERVER_PORT << "..." << std::endl;

    // Главный цикл для принятия и обработки клиентских соединений
    while (true) {
        sockaddr_in clientAddr = {};  // Структура для хранения данных о клиенте
        int clientAddrLen = sizeof(clientAddr);

        // Принятие входящего соединения
        SOCKET clientSocket = accept(listenSocket, (sockaddr*)&clientAddr, &clientAddrLen);
        if (clientSocket == INVALID_SOCKET) {  // Если ошибка при принятии соединения
            std::cerr << "Ошибка accept: " << WSAGetLastError() << std::endl;
            continue;  // Продолжаем прослушивание
        }

        std::cout << "Клиент подключился." << std::endl;

        // Обработка общения с клиентом
        HandleClient(clientSocket);

        // Закрытие сокета клиента после обработки общения
        closesocket(clientSocket);
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