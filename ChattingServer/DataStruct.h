#pragma once

#include <vector>
#include <Windows.h>

// ������ �̺�Ʈ
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
