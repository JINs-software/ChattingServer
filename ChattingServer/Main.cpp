#include "ChattingServer.h"
#include <conio.h>
#include <time.h>

int main() {
	//ChattingServer(const char* serverIP, UINT16 serverPort,
	//	DWORD numOfIocpConcurrentThrd, UINT16 numOfWorkerThreads,
	//	UINT16 maxOfConnections, bool beNagle = true,
	//	UINT32 sessionSendBuffSize = CHAT_SERV_SESSION_SEND_BUFF_SIZE, UINT32 sessionRecvBuffSize = CHAT_SERV_SESSION_RECV_BUFF_SIZE
	//)
	ChattingServer chatserver(NULL, 12001, 0, 4, CHAT_SERV_LIMIT_ACCEPTANCE);

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
			else if (ctr == 'c' || ctr == 'C') {
				chatserver.MemAllocLog();
				DebugBreak();
			}
#endif
			else if (ctr == 'd' || ctr == 'D') {
				chatserver.SessionReleaseLog();
				DebugBreak();
			}
			else if (ctr == 'p' || ctr == 'P') {
				chatserver.SessionReleaseLog();
#if defined(PLAYER_CREATE_RELEASE_LOG)
				chatserver.PlayerFileLog();
#endif
				DebugBreak();
			}
		}

		clock_t now = clock();
		if (now - ct > 100) {
			chatserver.ConsoleLog();
			ct = now;
		}
	}

	chatserver.Stop();
}