#include <windows.h>
#include <iostream>
#include <string>

static const size_t MAX_MESSAGE_LEN = 1024;
static const char* TERMINATOR_CMD = ":q";
static const char* PIPE_PATH = "\\\\.\\pipe\\lab4"; 

static void FatalError(const char* msg);
static void RunServerLoop(HANDLE hPipe);

int main()
{
    // 1) Создаём именованный канал на запись
    HANDLE hPipe = CreateNamedPipeA(
        PIPE_PATH,
        PIPE_ACCESS_OUTBOUND | FILE_FLAG_OVERLAPPED,
        PIPE_TYPE_MESSAGE | PIPE_WAIT,
        1,                      // Макс. число одновременных клиентов (1)
        0,                      // Размер буфера “out” (сетевой буфер)
        0,                      // Размер буфера “in”  (не используется)
        0,                      // Таймаут по умолчанию (ms)
        NULL                    // Security attrs
    );
    if (hPipe == INVALID_HANDLE_VALUE) {
        FatalError("Cannot create named pipe");
    }

    std::cout << "[Server] Pipe created, waiting for client to connect...\n";

    // 2) Даем клиенту возможность подключиться
    BOOL connected = ConnectNamedPipe(hPipe, NULL);
    if (!connected) {
        CloseHandle(hPipe);
        FatalError("ConnectNamedPipe() failed");
    }

    std::cout << "[Server] Client is connected. Starting send-loop.\n";

    // 3) Основной цикл: читаем строки с stdin и пишем в канал
    RunServerLoop(hPipe);

    // 4) Отключаем канал и закрываем хендлы
    DisconnectNamedPipe(hPipe);
    CloseHandle(hPipe);

    std::cout << "[Server] Pipe closed. Exiting.\n";
    return 0;
}

static void RunServerLoop(HANDLE hPipe)
{
    // Предварительная подготовка OVERLAPPED I/O
    OVERLAPPED ovlp;
    ZeroMemory(&ovlp, sizeof(ovlp));
    ovlp.hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (ovlp.hEvent == NULL) {
        FatalError("CreateEvent() failed");
    }

    char sendBuf[MAX_MESSAGE_LEN];
    std::string inputLine;

    while (true)
    {
        std::cout << "[Server] Enter text ('" << TERMINATOR_CMD << "' to quit): ";
        std::getline(std::cin, inputLine);

        // Ограничиваем длину (о +1 для завершающего '\0')
        if (inputLine.size() + 1 > MAX_MESSAGE_LEN) {
            std::cerr << "[Server] Warning: message exceeds " 
                      << MAX_MESSAGE_LEN << " bytes. Try again.\n";
            continue; 
        }

        // Копируем строку в сырой буфер и ставим нуль-терминатор
        ZeroMemory(sendBuf, sizeof(sendBuf));
        for (size_t i = 0; i < inputLine.size(); ++i) {
            sendBuf[i] = inputLine[i];
        }
        sendBuf[inputLine.size()] = '\0';

        // Запускаем асинхронную запись: размер = strlen(...)+1
        DWORD toWrite = (DWORD)(strlen(sendBuf) + 1);
        BOOL written = WriteFile(hPipe, sendBuf, toWrite, NULL, &ovlp);
        if (!written) {
            // Если возвращено FALSE, но overlapped — надо посмотреть GetLastError()
            DWORD err = GetLastError();
            if (err != ERROR_IO_PENDING) {
                CloseHandle(ovlp.hEvent);
                FatalError("WriteFile() failed");
            }
        }

        // Ждем события, пока асинхронная запись не завершится
        WaitForSingleObject(ovlp.hEvent, INFINITE);

        std::cout << "[Server] Sent: \"" << sendBuf << "\"\n";

        // Если это строка-терминатор, выходим
        if (inputLine == TERMINATOR_CMD) {
            break;
        }
    }

    // Закрываем само событие
    CloseHandle(ovlp.hEvent);
}

static void FatalError(const char* msg)
{
    DWORD code = GetLastError();
    std::cerr << "[Server][ERROR] " << msg << " (Code " << code << ")\n";
    ExitProcess((UINT)code);
}
