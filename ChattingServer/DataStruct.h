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

	bool sendDelayed = false;
};
class AccountObjectPool {
	struct stAccountNode {
		stAccoutInfo account;
		stAccountNode* next = NULL;
	};
	UINT			m_ObjectCapacity;
	stAccountNode*	m_FreeFront;

	INT				m_AllocatedObjectCnt;
public:
	AccountObjectPool(UINT objectCapacity) 
		: m_ObjectCapacity(objectCapacity), m_FreeFront(NULL), m_AllocatedObjectCnt(0)
	{
		if (m_ObjectCapacity > 0) {
			m_FreeFront = (stAccountNode*)calloc(m_ObjectCapacity, sizeof(stAccountNode));
			if (m_FreeFront == NULL) {
				DebugBreak();
			}

			stAccountNode* nodePtr = (stAccountNode*)(m_FreeFront);
			for (size_t idx = 0; idx < m_ObjectCapacity; idx++) {
				nodePtr->next = nodePtr + 1;
				nodePtr += 1;
			}
			nodePtr -= 1;
			nodePtr->next = NULL;	// 맨 마지막 꼬리는 NULL
		}
	}

	inline INT GetAllocatedObjectCnt() {
		return m_AllocatedObjectCnt;
	}

	stAccoutInfo* Alloc() {
		stAccoutInfo* ret = NULL;
		if (m_FreeFront != NULL) {
			ret = &m_FreeFront->account;
			m_FreeFront = m_FreeFront->next;

			m_AllocatedObjectCnt++;
		}
		return ret;
	}
	void Free(stAccoutInfo* freePtr) {
		m_AllocatedObjectCnt--;

		stAccountNode* nodePtr = (stAccountNode*)freePtr;
		nodePtr->next = m_FreeFront;
		m_FreeFront = nodePtr;
	}
};

// 스레드 이벤트
struct stThreadEvent {
	HANDLE recvEvent;
	UINT64 sessionID;
};

struct stSessionMessageQ {

};

struct stRecvInfo {
	UINT64 sessionID;
	size_t recvMsgCnt;
};
