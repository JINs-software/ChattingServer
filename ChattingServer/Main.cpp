#include "ChattingServer.h"
#include <conio.h>

int main() {
	//ChattingServer(const char* serverIP, UINT16 serverPort,
	//	DWORD numOfIocpConcurrentThrd, UINT16 numOfWorkerThreads,
	//	UINT16 maxOfConnections, bool beNagle = true,
	//	UINT32 sessionSendBuffSize = CHAT_SERV_SESSION_SEND_BUFF_SIZE, UINT32 sessionRecvBuffSize = CHAT_SERV_SESSION_RECV_BUFF_SIZE
	//)
	ChattingServer chatserver("127.0.0.1", 12001, 1, 1, 0, 0, 0, 0);

	chatserver.Start();

	char ctr;
	while (true) {
		if (_kbhit()) {
			ctr = _getch();
			if (ctr == 's' || ctr == 'S') {
				break;
			}
		}
	}

	chatserver.Stop();
}