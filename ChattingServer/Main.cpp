#include "ChattingServer.h"
#include <conio.h>
#include <time.h>

int main() {
	ChattingServer chatserver(NULL, CHAT_SERV_PORT, 0, IOCP_WORKER_THREAD_CNT, CHAT_SERV_LIMIT_ACCEPTANCE,
		CHAT_TLS_MEM_POOL_DEFAULT_UNIT_CNT, CHAT_TLS_MEM_POOL_DEFAULT_UNIT_CAPACITY, CHAT_SERIAL_BUFFER_SIZE
	);

	chatserver.Start();

	char ctr;
	clock_t ct = 0;
	while (true) {
		if (_kbhit()) {		
			ctr = _getch();
			if (ctr == 's' || ctr == 'S') {
				break;
			}
#if defined(ALLOC_MEM_LOG)
			else if (ctr == 'm' || ctr == 'M') {
				chatserver.MemAllocLog();
				DebugBreak();
			}
#endif
#if defined(SESSION_LOG)
			else if (ctr == 'r' || ctr == 'R') {
				chatserver.SessionReleaseLog();
				DebugBreak();
			}
#endif
			else if (ctr == 'a' || ctr == 'A') {
#if defined(SESSION_LOG)
				chatserver.SessionReleaseLog();
#endif
#if defined(PLAYER_CREATE_RELEASE_LOG)
				chatserver.PlayerFileLog();
#endif
				DebugBreak();
			}
			else if (ctr == 'd' || ctr == 'D') {
				DebugBreak();
			}
		}


		chatserver.ConsoleLog();

		//system("cls");   // 콘솔 창 지우기
		//
		//static size_t logCnt = 0;
		//
		//static COORD coord;
		//coord.X = 0;
		//coord.Y = 0;
		//SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), coord);
		//std::cout << "m_UpdateWorkerBalance: " << chatserver.m_UpdateWorkerBalance << std::endl;

		Sleep(1000);
	}

	chatserver.Stop();
}