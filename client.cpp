#include <windows.h>
#include <iostream>
#include <cstring>

static const size_t MAX_BUFFER_LEN = 1024;
static const char* TERMINATOR_KEY = ":q";
static const char* PIPE_PATH = "\\\\.\\pipe\\lab4"; 

static void ReportErrorAndExit(const char* msg);

int main()
{
    // 1) Дожидаемся, пока канал создастся сервером
    BOOL waitOk = WaitNamedPipeA(PIPE_PATH, NMPWAIT_WAIT_FOREVER);
    if (!waitOk) {
        ReportErrorAndExit("WaitNamedPipe() failed");
    }

    // 2) Открываем канал в режиме чтения с OVERLAPPED
    HANDLE hPipe = CreateFileA(PIPE_PATH, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);

    if (hPipe == INVALID_HANDLE_VALUE) {
        ReportErrorAndExit("CreateFileA() failed");
    }
    std::cout << "[Client] Connected to pipe. Starting receive-loop...\n";

    // 3) Настраиваем OVERLAPPED IO
    OVERLAPPED ovlp;
    ZeroMemory(&ovlp, sizeof(ovlp));
    ovlp.hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (ovlp.hEvent == NULL) {
        CloseHandle(hPipe);
        ReportErrorAndExit("CreateEvent() failed");
    }

    char recvBuf[MAX_BUFFER_LEN];
    DWORD bytesRead = 0;

    // 4) Основной цикл: асинхронно ждём сообщений от сервера
    while (true)
    {
        ZeroMemory(recvBuf, sizeof(recvBuf));

        // Запускаем асинхронное чтение из именованного канала
        BOOL readStarted = ReadFile(hPipe, recvBuf, (DWORD)(MAX_BUFFER_LEN - 1), NULL, &ovlp);
        if (!readStarted) {
            DWORD err = GetLastError();
            if (err != ERROR_IO_PENDING) {
                CloseHandle(ovlp.hEvent);
                CloseHandle(hPipe);
                ReportErrorAndExit("ReadFile() failed");
            }
        }

        // Ждём, пока реально прочитаются данные (или канал закроется)
        WaitForSingleObject(ovlp.hEvent, INFINITE);

        // Выясняем, сколько байт прочитано
        BOOL ok = GetOverlappedResult(hPipe, &ovlp, &bytesRead, FALSE);
        if (!ok) {
            CloseHandle(ovlp.hEvent);
            CloseHandle(hPipe);
            ReportErrorAndExit("GetOverlappedResult() failed");
        }

        // Если прочитали 0 байт — сервер закрыл соединение или прислал пустое
        if (bytesRead == 0) {
            std::cout << "[Client] Server closed the pipe or no more data.\n";
            break;
        }

        // Печатаем полученное сообщение
        std::cout << "[Client] Received: " << recvBuf << "\n";

        // Если получили «:q», выходим
        if (std::strcmp(recvBuf, TERMINATOR_KEY) == 0) {
            break;
        }
    }

    // 5) Завершаем работу: закрываем событие и дескриптор канала
    CloseHandle(ovlp.hEvent);
    CloseHandle(hPipe);

    std::cout << "[Client] Exiting.\n";
    return 0;
}

static void ReportErrorAndExit(const char* msg)
{
    DWORD code = GetLastError();
    std::cerr << "[Client][ERROR] " << msg << " (Error code " << code << ")\n";
    ExitProcess((UINT)code);
}
