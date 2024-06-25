#include "ChattingServer.h"
#include <conio.h>
#include <time.h>

int main() {
	std::cout << "Priority Boost: ";
	int prior;
	std::cin >> prior;
	//ABOVE_NORMAL_PRIORITY_CLASS: 높은 우선순위
	//HIGH_PRIORITY_CLASS : 매우 높은 우선순위
	if (prior == 1) {
		HANDLE hProcess = GetCurrentProcess();

		// 프로세스 우선순위를 REALTIME_PRIORITY_CLASS로 설정합니다.
		if (SetPriorityClass(hProcess, ABOVE_NORMAL_PRIORITY_CLASS)) {
			std::cout << "Process priority successfully set to REALTIME_PRIORITY_CLASS." << std::endl;
		}
		else {
			std::cerr << "Failed to set process priority." << std::endl;
		}
	}
	else if (prior == 2) {
		HANDLE hProcess = GetCurrentProcess();

		// 프로세스 우선순위를 REALTIME_PRIORITY_CLASS로 설정합니다.
		if (SetPriorityClass(hProcess, HIGH_PRIORITY_CLASS)) {
			std::cout << "Process priority successfully set to REALTIME_PRIORITY_CLASS." << std::endl;
		}
		else {
			std::cerr << "Failed to set process priority." << std::endl;
		}
	}

	ChattingServer chatserver(
		NULL, CHAT_SERV_PORT, 
		0, IOCP_WORKER_THREAD_CNT, CHAT_SERV_LIMIT_ACCEPTANCE,
		CHAT_TLS_MEM_POOL_DEFAULT_UNIT_CNT, CHAT_TLS_MEM_POOL_DEFAULT_UNIT_CAPACITY, 
		CHAT_SERIAL_BUFFER_SIZE,
		CHAT_SERV_SESSION_RECV_BUFF_SIZE,
		dfPACKET_CODE, dfPACKET_KEY,
		RECV_BUFFERING_MODE
	);

	if (!chatserver.Start()) {
		return 0;
	}

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