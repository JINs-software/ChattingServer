#pragma once

#include <vector>
#include <Windows.h>

// Account Info
struct stAccoutInfo {
	INT64 AccountNo;
	WCHAR ID[20];			// null ����
	WCHAR Nickname[20];		// null ����
	char sessinKey[64];		// ���� ��ū

	WORD X;
	WORD Y;

	stAccoutInfo() {
		memset(this, 0, sizeof(stAccoutInfo));
	}
};

// ������ �̺�Ʈ
struct stThreadEvent {
	HANDLE recvEvent;
	UINT64 sessionID;
};

struct stRecvInfo {
	UINT64 sessionID;
	size_t recvMsgCnt;
};
