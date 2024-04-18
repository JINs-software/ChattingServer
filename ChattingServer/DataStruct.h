#pragma once

#include <vector>
#include <Windows.h>

// 스레드 이벤트
struct stThreadEvent {
	HANDLE recvEvent;
	UINT64 sessionID;
};

struct stSessionMessageQ {

};

struct stRecvInfo {
	UINT64 sessionID;
	size_t recvPacketSize;
};
