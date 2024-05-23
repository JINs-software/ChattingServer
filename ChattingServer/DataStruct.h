#pragma once

#include <vector>
#include <Windows.h>

// Account Info
struct stAccoutInfo {
	INT64 AccountNo;
	WCHAR ID[20];			// null 포함
	WCHAR Nickname[20];		// null 포함
	char sessinKey[64];		// 인증 토큰

	WORD X;
	WORD Y;

	stAccoutInfo() {
		memset(this, 0, sizeof(stAccoutInfo));
	}
};

// 스레드 이벤트
struct stThreadEvent {
	HANDLE recvEvent;
	UINT64 sessionID;
};

struct stRecvInfo {
	UINT64 sessionID;
	size_t recvMsgCnt;
};
