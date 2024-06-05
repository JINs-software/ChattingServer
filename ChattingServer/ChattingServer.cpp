#include "ChattingServer.h"
#include <fstream>

bool ChattingServer::OnWorkerThreadCreate(HANDLE thHnd)
{
	if (m_WorkerThreadCnt >= IOCP_WORKER_THREAD_CNT) {
		return false;
	}

#if defined(dfPROCESSING_MODE_THREAD_RECV_INFO_QUEUE_EVENT)
	HANDLE recvEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	UINT8 eventIdx = m_WorkerThreadCnt++;
	m_WorkerThreadRecvEvents[eventIdx] = recvEvent;
	DWORD thID = GetThreadId(thHnd);
	thEventIndexMap.insert({ thID, recvEvent });
#elif defined(dfPROCESSING_MODE_THREAD_RECV_INFO_QUEUE_POLLING)
	m_WorkerThreadCnt++;
#endif
	return true;
}

void ChattingServer::OnWorkerThreadCreateDone()
{
	m_ProcessThreadHnd = (HANDLE)_beginthreadex(NULL, 0, ProcessThreadFunc, this, 0, NULL);
}

void ChattingServer::OnWorkerThreadStart() {

#if defined(dfLOCKFREE_QUEUE_SYNCHRONIZATION)
#if defined(dfPROCESSING_MODE_THREAD_RECV_INFO_QUEUE_POLLING)
	static LONG sidx = 0;
	LONG idx = InterlockedIncrement(&sidx);

	if (TlsGetValue(m_TlsRecvQueueIdx) == NULL) {
		LockFreeQueue<stRecvInfo>* recvInfoQueue = new LockFreeQueue<stRecvInfo>();
		m_TlsRecvQueueVec[idx - 1] = recvInfoQueue;
		TlsSetValue(m_TlsRecvQueueIdx, recvInfoQueue);
	}
	else {
		DebugBreak();
	}
#endif
#else
#if defined(dfPROCESSING_MODE_THREAD_RECV_INFO_QUEUE_EVENT)
	if (TlsGetValue(m_RecvEventTlsIndex) == NULL) {
		DWORD thID = GetThreadId(GetCurrentThread());
		HANDLE thEventHnd = thEventIndexMap[thID];
		TlsSetValue(m_RecvEventTlsIndex, thEventHnd);

		m_ThreadEventRecvqMap.insert({ thEventHnd, std::queue<stRecvInfo>() });
		// 임시 동기화 객체
		CRITICAL_SECTION* lockPtr = new CRITICAL_SECTION;
		InitializeCriticalSection(lockPtr);
		m_ThreadEventLockMap.insert({ thEventHnd, lockPtr });
	}
	else {
		DebugBreak();
	}
#elif defined(dfPROCESSING_MODE_THREAD_RECV_INFO_QUEUE_POLLING)
	static LONG sidx = 0;
	LONG idx = InterlockedIncrement(&sidx);

	if (TlsGetValue(m_TlsRecvQueueIdx) == NULL && TlsGetValue(m_TlsRecvQueueLockIdx) == NULL) {
		std::queue<stRecvInfo>* recvQueue = new std::queue<stRecvInfo>;
		CRITICAL_SECTION* lockPtr = new CRITICAL_SECTION;
		InitializeCriticalSection(lockPtr);
		m_TlsRecvQueueVec[idx - 1].first = recvQueue;
		m_TlsRecvQueueVec[idx - 1].second = lockPtr;
		TlsSetValue(m_TlsRecvQueueIdx, recvQueue);
		TlsSetValue(m_TlsRecvQueueLockIdx, lockPtr);
	}
	else {
		DebugBreak();
	}
#endif
#endif
}

bool ChattingServer::OnConnectionRequest()
{
	return CLanServer::OnConnectionRequest();
}

void ChattingServer::OnClientJoin(uint64 sessionID)
{
#if defined(PLAYER_CREATE_RELEASE_LOG)
	stPlayerLog& playerLog = GetPlayerLog();
	playerLog.joinFlag = true;
	playerLog.leaveFlag = false;
	playerLog.sessionID = sessionID;
	playerLog.sessinIdIndex = (uint16)sessionID;
#endif

	// 1. 로그인 대기 세션 셋에 삽입
	{
		std::lock_guard<std::mutex> lockGuard(m_LoginWaitSessionsMtx);
		m_LoginWaitSessions.insert(sessionID);
	}

	
#if defined(dfLOCKFREE_QUEUE_SYNCHRONIZATION)
#if defined(dfPROCESSING_MODE_THREAD_RECV_INFO_QUEUE_POLLING)
	// 2. 세션 별 메시지 큐 생성 및 메시지 큐 관리 자료구조에 삽입
	AcquireSRWLockExclusive(&m_SessionMessageqMapSrwLock);
	m_SessionMessageQueueMap.insert({ sessionID, LockFreeQueue<JBuffer*>() });
	ReleaseSRWLockExclusive(&m_SessionMessageqMapSrwLock);
#endif
#else
	// 2. 세션 별 메시지 큐 생성 및 메시지 큐 관리 자료구조에 삽입
	AcquireSRWLockExclusive(&m_SessionMessageqMapSrwLock);

	m_SessionMessageQueueMap.insert({ sessionID, std::queue<JBuffer*>() });
	// 임시 동기화 객체(메시지 큐 별 동기화 객체 생성)
	CRITICAL_SECTION* lockPtr = new CRITICAL_SECTION;
	if (lockPtr == NULL) {
		DebugBreak();
	}
	InitializeCriticalSection(lockPtr);
	m_SessionMessageQueueLockMap.insert({ sessionID, lockPtr });

	ReleaseSRWLockExclusive(&m_SessionMessageqMapSrwLock);
#endif
}

//void ChattingServer::OnDeleteSendPacket(uint64 sessionID, JBuffer& sendRingBuffer)
//{
//#if defined(SESSION_SENDBUFF_SYNC_TEST)
//	// 송신 버퍼로부터 송신 직렬화 패킷 포인터 디큐잉 -> AcquireSRWLockExclusive
//	AcquireSRWLockExclusive(&session->sendBuffSRWLock);
//#endif
//
//	// 세션 송신 큐에 존재하는 송신 직렬화 버퍼 메모리 반환
//	while (sendRingBuffer.GetUseSize() >= sizeof(JBuffer*)) {
//#if defined(PLAYER_CREATE_RELEASE_LOG)
//		USHORT logIdx =  InterlockedIncrement16((short*)&m_DeletedSendPacketIdx);
//		memset(&m_DeletedSendPacketLog[logIdx], 0, sizeof(stDeletedSendPacket));
//#endif
//		JBuffer* sendPacket;
//		sendRingBuffer >> sendPacket;
//
//		JBuffer copyPacket(sendPacket->GetUseSize());
//		copyPacket.Enqueue(sendPacket->GetDequeueBufferPtr(), sendPacket->GetUseSize());
//
//		stMSG_HDR hdr;
//		copyPacket.Dequeue((BYTE*)&hdr, sizeof(stMSG_HDR));
//		//Encode(hdr.randKey, hdr.len, hdr.checkSum, sendPacket->GetDequeueBufferPtr());
//		Decode(hdr.randKey, hdr.len, hdr.checkSum, copyPacket.GetDequeueBufferPtr());
//		WORD type;
//		copyPacket.Peek(&type);
//		switch (type)
//		{
//		case en_PACKET_CS_CHAT_RES_LOGIN:
//		{
//			MSG_PACKET_CS_CHAT_RES_LOGIN msg;
//			copyPacket.Dequeue((BYTE*)&msg, sizeof(MSG_PACKET_CS_CHAT_RES_LOGIN));
//
//#if defined(PLAYER_CREATE_RELEASE_LOG)
//			m_DeletedSendPacketLog[logIdx].type = en_PACKET_CS_CHAT_RES_LOGIN;
//			m_DeletedSendPacketLog[logIdx].accountNo = msg.AccountNo;
//#endif
//		}
//		break;
//		case en_PACKET_CS_CHAT_RES_SECTOR_MOVE:
//		{
//			MSG_PACKET_CS_CHAT_RES_SECTOR_MOVE msg;
//			copyPacket.Dequeue((BYTE*)&msg, sizeof(MSG_PACKET_CS_CHAT_RES_SECTOR_MOVE));
//
//#if defined(PLAYER_CREATE_RELEASE_LOG)
//			m_DeletedSendPacketLog[logIdx].type = en_PACKET_CS_CHAT_RES_SECTOR_MOVE;
//			m_DeletedSendPacketLog[logIdx].accountNo = msg.AccountNo;
//#endif
//		}
//		break;
//		case en_PACKET_CS_CHAT_RES_MESSAGE:
//		{
//			MSG_PACKET_CS_CHAT_RES_MESSAGE msg;
//			copyPacket.Dequeue((BYTE*)&msg, sizeof(MSG_PACKET_CS_CHAT_RES_MESSAGE));
//
//#if defined(PLAYER_CREATE_RELEASE_LOG)
//			m_DeletedSendPacketLog[logIdx].type = en_PACKET_CS_CHAT_RES_MESSAGE;
//			m_DeletedSendPacketLog[logIdx].accountNo = msg.AccountNo;
//#endif
//		}
//		break;
//		default:
//			break;
//		}
//
//#if defined(ALLOC_MEM_LOG)
//		m_SerialBuffPoolMgr.GetTlsMemPool().FreeMem(sendPacket, to_string(sessionID) + ", FreeMem (DeleteSession)");
//#endif
//	}
//}

void ChattingServer::OnClientLeave(uint64 sessionID)
{
#if defined(PLAYER_CREATE_RELEASE_LOG)
	stPlayerLog& playerLog = GetPlayerLog();
	playerLog.joinFlag = false;
	playerLog.leaveFlag = true;
	playerLog.sessionID = sessionID;
	playerLog.sessinIdIndex = (uint16)sessionID;
#endif

	bool loginWaitSession = false;
	// 로그인 완료 이전의 세션
	{
		std::lock_guard<std::mutex> lockGuard(m_LoginWaitSessionsMtx);
		if (m_LoginWaitSessions.find(sessionID) != m_LoginWaitSessions.end()) {
			m_LoginWaitSessions.erase(sessionID);
			loginWaitSession = true;
		}
	}

#if defined(ALLOC_BY_TLS_MEM_POOL)
	//JBuffer* message = (JBuffer*)m_SerialBuffPoolMgr.GetTlsMemPool().AllocMem(1, to_string(sessionID) + ", OnClientLeave");
	//message->ClearBuffer();
	JBuffer* message = AllocSerialBuff();
#else
	JBuffer* message = new JBuffer();
#endif
	if(loginWaitSession) {
		(*message) << (WORD)en_SESSION_RELEASE_BEFORE_LOGIN;
	}
	else {
		(*message) << (WORD)en_SESSION_RELEASE;
	}

	
#if defined(dfLOCKFREE_QUEUE_SYNCHRONIZATION)
#if defined(dfPROCESSING_MODE_THREAD_RECV_INFO_QUEUE_POLLING)
	// 메시지 큐 & 메시지 큐 동기화 객체 GET
	AcquireSRWLockShared(&m_SessionMessageqMapSrwLock);

	auto msgQueueIter = m_SessionMessageQueueMap.find(sessionID);
	if (msgQueueIter == m_SessionMessageQueueMap.end()) {
		DebugBreak();
	}
	LockFreeQueue<JBuffer*>& sessionMsgQ = msgQueueIter->second;

	ReleaseSRWLockShared(&m_SessionMessageqMapSrwLock);
#elif defined(dfPROCESSING_MODE_THREAD_SINGLE_JOB_QUEUE_POLLING)
	m_MessageLockFreeQueue.Enqueue({ sessionID, message });
#endif
#else
	// 메시지 큐 & 메시지 큐 동기화 객체 GET
	AcquireSRWLockShared(&m_SessionMessageqMapSrwLock);

	auto msgQueueIter = m_SessionMessageQueueMap.find(sessionID);
	auto msgQueueLockIter = m_SessionMessageQueueLockMap.find(sessionID);
	if (msgQueueIter == m_SessionMessageQueueMap.end() || msgQueueLockIter == m_SessionMessageQueueLockMap.end()) {
#if defined(SESSION_LOG)
		SessionReleaseLog();
#endif
#if defined(PLAYER_CREATE_RELEASE_LOG)
		PlayerFileLog();
#endif
		DebugBreak();
	}
	std::queue<JBuffer*>& sessionMsgQ = msgQueueIter->second;
	CRITICAL_SECTION* lockPtr = msgQueueLockIter->second;

	ReleaseSRWLockShared(&m_SessionMessageqMapSrwLock);
#endif

#if defined(dfLOCKFREE_QUEUE_SYNCHRONIZATION)
#if defined(dfPROCESSING_MODE_THREAD_RECV_INFO_QUEUE_POLLING)
	sessionMsgQ.Enqueue(message);

	stRecvInfo recvInfo;
	recvInfo.sessionID = sessionID;
	recvInfo.recvMsgCnt = 1;
	LockFreeQueue<stRecvInfo>* recvInfoQ = (LockFreeQueue<stRecvInfo>*)TlsGetValue(m_TlsRecvQueueIdx);
	recvInfoQ->Enqueue(recvInfo);
#endif
#else
	EnterCriticalSection(lockPtr);								// 임시 동기화 객체
	sessionMsgQ.push(message);
	LeaveCriticalSection(lockPtr);								// 임시 동기화 객체

#if defined(dfPROCESSING_MODE_THREAD_RECV_INFO_QUEUE_EVENT)
	stRecvInfo recvInfo;
	recvInfo.sessionID = sessionID;
	recvInfo.recvMsgCnt = 1;
	HANDLE recvEvent = (HANDLE)TlsGetValue(m_RecvEventTlsIndex);
	std::queue<stRecvInfo>& recvInfoQ = m_ThreadEventRecvqMap[recvEvent];

	lockPtr = m_ThreadEventLockMap[recvEvent];				// 임시 동기화 객체
	EnterCriticalSection(lockPtr);							// 임시 동기화 객체
	recvInfoQ.push(recvInfo);
	LeaveCriticalSection(lockPtr);							// 임시 동기화 객체
	SetEvent(recvEvent);
#elif defined(dfPROCESSING_MODE_THREAD_RECV_INFO_QUEUE_POLLING)
	stRecvInfo recvInfo;
	recvInfo.sessionID = sessionID;
	recvInfo.recvMsgCnt = 1;
	std::queue<stRecvInfo>* recvInfoQ = (std::queue<stRecvInfo>*)TlsGetValue(m_TlsRecvQueueIdx);
	lockPtr = (CRITICAL_SECTION*)TlsGetValue(m_TlsRecvQueueLockIdx);
	EnterCriticalSection(lockPtr);							// 임시 동기화 객체
	recvInfoQ->push(recvInfo);
	LeaveCriticalSection(lockPtr);							// 임시 동기화 객체
#endif
#endif
}

#if defined(ON_RECV_BUFFERING)
void ChattingServer::OnRecv(UINT64 sessionID, std::queue<JBuffer>& bufferedQueue, size_t recvDataLen)
{
	LONG recvMsgCnt = 0;

	while (!bufferedQueue.empty()) {
		JBuffer recvBuff = bufferedQueue.front(); 
		bufferedQueue.pop();

		while (recvBuff.GetUseSize() > 0) {			/// ????		// [TO DO] msgHdr.len까지만 디큐잉 하는 것으로 변경
			WORD type;
			recvBuff.Peek(&type);

			JBuffer* message = AllocSerialBuff();
			if (message == NULL) {
				DebugBreak();
			}

			switch (type)
			{
			case en_PACKET_CS_CHAT_REQ_LOGIN:
			{
				recvBuff.Dequeue(message->GetDequeueBufferPtr(), sizeof(MSG_PACKET_CS_CHAT_REQ_LOGIN));
				message->DirectMoveEnqueueOffset(sizeof(MSG_PACKET_CS_CHAT_REQ_LOGIN));
			}
			break;
			case en_PACKET_CS_CHAT_REQ_SECTOR_MOVE:
			{
				recvBuff.Dequeue(message->GetDequeueBufferPtr(), sizeof(MSG_PACKET_CS_CHAT_REQ_SECTOR_MOVE));
				message->DirectMoveEnqueueOffset(sizeof(MSG_PACKET_CS_CHAT_REQ_SECTOR_MOVE));
			}
			break;
			case en_PACKET_CS_CHAT_REQ_MESSAGE:
			{
				WORD messageLen;
				recvBuff.Peek(sizeof(WORD) + sizeof(INT64), (BYTE*)&messageLen, sizeof(WORD));
				recvBuff.Dequeue(message->GetDequeueBufferPtr(), sizeof(MSG_PACKET_CS_CHAT_REQ_MESSAGE) + messageLen);
				message->DirectMoveEnqueueOffset(sizeof(MSG_PACKET_CS_CHAT_REQ_MESSAGE) + messageLen);
			}
			break;
			case en_PACKET_CS_CHAT_REQ_HEARTBEAT:
			{
				DebugBreak();
			}
			break;
			default:
				DebugBreak();
				break;
			}

#if defined(dfLOCKFREE_QUEUE_SYNCHRONIZATION)
#if defined(dfPROCESSING_MODE_THREAD_RECV_INFO_QUEUE_POLLING)
			// .. 세션 별 메시지 큐 삽입
			// 메시지 큐 GET
			AcquireSRWLockShared(&m_SessionMessageqMapSrwLock);

			auto msgQueueIter = m_SessionMessageQueueMap.find(sessionID);
			if (msgQueueIter == m_SessionMessageQueueMap.end()) {
				DebugBreak();
			}
			LockFreeQueue<JBuffer*>& sessionMsgQ = msgQueueIter->second;
			ReleaseSRWLockShared(&m_SessionMessageqMapSrwLock);

			sessionMsgQ.Enqueue(message);
#elif defined(dfPROCESSING_MODE_THREAD_SINGLE_JOB_QUEUE_POLLING)
			m_MessageLockFreeQueue.Enqueue({ sessionID, message });
#endif
#else
			// .. 세션 별 메시지 큐 삽입
			// 메시지 큐 & 메시지 큐 동기화 객체 GET
			AcquireSRWLockShared(&m_SessionMessageqMapSrwLock);

			auto msgQueueIter = m_SessionMessageQueueMap.find(sessionID);
			auto msgQueueLockIter = m_SessionMessageQueueLockMap.find(sessionID);
			if (msgQueueIter == m_SessionMessageQueueMap.end() || msgQueueLockIter == m_SessionMessageQueueLockMap.end()) {
				DebugBreak();
			}
			std::queue<JBuffer*>& sessionMsgQ = msgQueueIter->second;
			CRITICAL_SECTION* lockPtr = msgQueueLockIter->second;
			ReleaseSRWLockShared(&m_SessionMessageqMapSrwLock);

			EnterCriticalSection(lockPtr);								// 임시 동기화 객체
			sessionMsgQ.push(message);
			LeaveCriticalSection(lockPtr);								// 임시 동기화 객체
#endif

			recvMsgCnt++;
		}
	}

	if (recvMsgCnt > 0) {
#if defined(CALCULATE_TRANSACTION_PER_SECOND)
		InterlockedAdd(&m_CalcTpsItems[RECV_TRANSACTION], recvMsgCnt);
		InterlockedAdd(&m_TotalTransaction[RECV_TRANSACTION], recvMsgCnt);
#endif

#if defined(dfLOCKFREE_QUEUE_SYNCHRONIZATION)
#if defined(dfPROCESSING_MODE_THREAD_RECV_INFO_QUEUE_POLLING)
		stRecvInfo recvInfo;
		recvInfo.sessionID = sessionID;
		recvInfo.recvMsgCnt = recvMsgCnt;
		LockFreeQueue<stRecvInfo>* recvInfoQ = (LockFreeQueue<stRecvInfo>*)TlsGetValue(m_TlsRecvQueueIdx);
		recvInfoQ->Enqueue(recvInfo);
#endif
#else
#if defined(dfPROCESSING_MODE_THREAD_RECV_INFO_QUEUE_EVENT)
		stRecvInfo recvInfo;
		recvInfo.sessionID = sessionID;
		recvInfo.recvMsgCnt = recvMsgCnt;
		HANDLE recvEvent = (HANDLE)TlsGetValue(m_RecvEventTlsIndex);
		std::queue<stRecvInfo>& recvInfoQ = m_ThreadEventRecvqMap[recvEvent];
		CRITICAL_SECTION* lockPtr = m_ThreadEventLockMap[recvEvent];			// 임시 동기화 객체
		EnterCriticalSection(lockPtr);											// 임시 동기화 객체
		recvInfoQ.push(recvInfo);
		LeaveCriticalSection(lockPtr);											// 임시 동기화 객체
		SetEvent(recvEvent);
#elif defined(dfPROCESSING_MODE_THREAD_RECV_INFO_QUEUE_POLLING)
		stRecvInfo recvInfo;
		recvInfo.sessionID = sessionID;
		recvInfo.recvMsgCnt = recvMsgCnt;
		std::queue<stRecvInfo>* recvInfoQ = (std::queue<stRecvInfo>*)TlsGetValue(m_TlsRecvQueueIdx);
		CRITICAL_SECTION* lockPtr = (CRITICAL_SECTION*)TlsGetValue(m_TlsRecvQueueLockIdx);
		EnterCriticalSection(lockPtr);							// 임시 동기화 객체
		recvInfoQ->push(recvInfo);
		LeaveCriticalSection(lockPtr);							// 임시 동기화 객체
#endif
#endif
	}
}
#else
void ChattingServer::OnRecv(uint64 sessionID, JBuffer& recvBuff)
{	
#if defined(PLAYER_CREATE_RELEASE_LOG)
	stPlayerLog& playerLog = GetPlayerLog();
	playerLog.recvFlag = true;
	playerLog.sessionID = sessionID;
	playerLog.sessinIdIndex = (uint16)sessionID;
#endif

	LONG recvMsgCnt = 0;

	while (recvBuff.GetUseSize() > 0) {			/// ????		// [TO DO] msgHdr.len까지만 디큐잉 하는 것으로 변경
		WORD type;
		recvBuff.Peek(&type);

		JBuffer* message = AllocSerialBuff();
		if (message == NULL) {
			DebugBreak();
		}

		switch (type)
		{
		case en_PACKET_CS_CHAT_REQ_LOGIN:
		{
			recvBuff.Dequeue(message->GetDequeueBufferPtr(), sizeof(MSG_PACKET_CS_CHAT_REQ_LOGIN));
			message->DirectMoveEnqueueOffset(sizeof(MSG_PACKET_CS_CHAT_REQ_LOGIN));
		}
		break;
		case en_PACKET_CS_CHAT_REQ_SECTOR_MOVE:
		{
			recvBuff.Dequeue(message->GetDequeueBufferPtr(), sizeof(MSG_PACKET_CS_CHAT_REQ_SECTOR_MOVE));
			message->DirectMoveEnqueueOffset(sizeof(MSG_PACKET_CS_CHAT_REQ_SECTOR_MOVE));
		}
		break;
		case en_PACKET_CS_CHAT_REQ_MESSAGE:
		{
			WORD messageLen;
			recvBuff.Peek(sizeof(WORD) + sizeof(INT64), (BYTE*)&messageLen, sizeof(WORD));
			recvBuff.Dequeue(message->GetDequeueBufferPtr(), sizeof(MSG_PACKET_CS_CHAT_REQ_MESSAGE) + messageLen);
			message->DirectMoveEnqueueOffset(sizeof(MSG_PACKET_CS_CHAT_REQ_MESSAGE) + messageLen);
		}
		break;
		case en_PACKET_CS_CHAT_REQ_HEARTBEAT:
		{
			DebugBreak();
		}
		break;
		default:
			DebugBreak();
			break;
		}

#if defined(dfLOCKFREE_QUEUE_SYNCHRONIZATION)
#if defined(dfPROCESSING_MODE_THREAD_RECV_INFO_QUEUE_POLLING)
		// .. 세션 별 메시지 큐 삽입
		// 메시지 큐 GET
		AcquireSRWLockShared(&m_SessionMessageqMapSrwLock);

		auto msgQueueIter = m_SessionMessageQueueMap.find(sessionID);
		if (msgQueueIter == m_SessionMessageQueueMap.end()) {
			DebugBreak();
		}
		LockFreeQueue<JBuffer*>& sessionMsgQ = msgQueueIter->second;
		ReleaseSRWLockShared(&m_SessionMessageqMapSrwLock);

		sessionMsgQ.Enqueue(message);
#elif defined(dfPROCESSING_MODE_THREAD_SINGLE_JOB_QUEUE_POLLING)
		m_MessageLockFreeQueue.Enqueue({ sessionID, message });
#endif
#else
		// .. 세션 별 메시지 큐 삽입
		// 메시지 큐 & 메시지 큐 동기화 객체 GET
		AcquireSRWLockShared(&m_SessionMessageqMapSrwLock);

		auto msgQueueIter = m_SessionMessageQueueMap.find(sessionID);
		auto msgQueueLockIter = m_SessionMessageQueueLockMap.find(sessionID);
		if (msgQueueIter == m_SessionMessageQueueMap.end() || msgQueueLockIter == m_SessionMessageQueueLockMap.end()) {
			DebugBreak();
		}
		std::queue<JBuffer*>& sessionMsgQ = msgQueueIter->second;
		CRITICAL_SECTION* lockPtr = msgQueueLockIter->second;
		ReleaseSRWLockShared(&m_SessionMessageqMapSrwLock);

		EnterCriticalSection(lockPtr);								// 임시 동기화 객체
		sessionMsgQ.push(message);
		LeaveCriticalSection(lockPtr);								// 임시 동기화 객체
#endif

		recvMsgCnt++;
		}

	if (recvMsgCnt > 0) {
#if defined(CALCULATE_TRANSACTION_PER_SECOND)
		InterlockedAdd(&m_CalcTpsItems[RECV_TRANSACTION], recvMsgCnt);
		InterlockedAdd(&m_TotalTransaction[RECV_TRANSACTION], recvMsgCnt);
#endif

#if defined(dfLOCKFREE_QUEUE_SYNCHRONIZATION)
#if defined(dfPROCESSING_MODE_THREAD_RECV_INFO_QUEUE_POLLING)
		stRecvInfo recvInfo;
		recvInfo.sessionID = sessionID;
		recvInfo.recvMsgCnt = recvMsgCnt;
		LockFreeQueue<stRecvInfo>* recvInfoQ = (LockFreeQueue<stRecvInfo>*)TlsGetValue(m_TlsRecvQueueIdx);
		recvInfoQ->Enqueue(recvInfo);
#endif
#else
#if defined(dfPROCESSING_MODE_THREAD_RECV_INFO_QUEUE_EVENT)
		stRecvInfo recvInfo;
		recvInfo.sessionID = sessionID;
		recvInfo.recvMsgCnt = recvMsgCnt;
		HANDLE recvEvent = (HANDLE)TlsGetValue(m_RecvEventTlsIndex);
		std::queue<stRecvInfo>& recvInfoQ = m_ThreadEventRecvqMap[recvEvent];
		CRITICAL_SECTION* lockPtr = m_ThreadEventLockMap[recvEvent];			// 임시 동기화 객체
		EnterCriticalSection(lockPtr);											// 임시 동기화 객체
		recvInfoQ.push(recvInfo);
		LeaveCriticalSection(lockPtr);											// 임시 동기화 객체
		SetEvent(recvEvent);
#elif defined(dfPROCESSING_MODE_THREAD_RECV_INFO_QUEUE_POLLING)
		stRecvInfo recvInfo;
		recvInfo.sessionID = sessionID;
		recvInfo.recvMsgCnt = recvMsgCnt;
		std::queue<stRecvInfo>* recvInfoQ = (std::queue<stRecvInfo>*)TlsGetValue(m_TlsRecvQueueIdx);
		CRITICAL_SECTION* lockPtr = (CRITICAL_SECTION*)TlsGetValue(m_TlsRecvQueueLockIdx);
		EnterCriticalSection(lockPtr);							// 임시 동기화 객체
		recvInfoQ->push(recvInfo);
		LeaveCriticalSection(lockPtr);							// 임시 동기화 객체
#endif
#endif
	}
}
#endif

void ChattingServer::OnError()
{
}

#if defined(CONNECT_MOINTORING_SERVER)
void ChattingServer::OnEnterJoinServer()
{
	m_MontDataMap.insert({ dfMONITOR_DATA_TYPE_CHAT_SERVER_RUN , {0} });
	m_MontDataMap.insert({ dfMONITOR_DATA_TYPE_CHAT_SERVER_CPU , {0} });
	m_MontDataMap.insert({ dfMONITOR_DATA_TYPE_CHAT_SERVER_MEM , {0} });
	m_MontDataMap.insert({ dfMONITOR_DATA_TYPE_CHAT_SESSION , {0} });
	m_MontDataMap.insert({ dfMONITOR_DATA_TYPE_CHAT_PLAYER , {0} });
	m_MontDataMap.insert({ dfMONITOR_DATA_TYPE_CHAT_UPDATE_TPS , {0} });
	m_MontDataMap.insert({ dfMONITOR_DATA_TYPE_CHAT_PACKET_POOL , {0} });
	m_MontDataMap.insert({ dfMONITOR_DATA_TYPE_CHAT_UPDATEMSG_POOL , {0} });

	m_PerfCounter = new PerformanceCounter();
	m_PerfCounter->SetCpuUsageCounter();
	
	//m_PerfCounter->SetCounter(CHAT_SERVER_MEMORY_USAGE_QUERY, dfQUERY_PROCESS_USER_VMEMORY_USAGE);
	m_PerfCounter->SetProcessCounter(CHAT_SERVER_MEMORY_USAGE_QUERY, dfQUERY_PROCESS_USER_VMEMORY_USAGE, L"ChattingServer");

	m_PerfCountThreadFunc = (HANDLE)_beginthreadex(NULL, 0, PerformanceCountFunc, this, 0, NULL);
}

void ChattingServer::OnLeaveServer()
{
	m_PerfCountStop = true;
}

void ChattingServer::OnRecvFromCLanServer(JBuffer& recvBuff)
{
}
#endif

UINT __stdcall ChattingServer::ProcessThreadFunc(void* arg)
{
	ChattingServer* server = (ChattingServer*)arg;
#if defined(ALLOC_BY_TLS_MEM_POOL)
	// 상위 서버 클래스에서 관리하는 메모리 풀 
	server->m_SerialBuffPoolMgr.AllocTlsMemPool();	// 생성자에서 설정한 Default 값을 따름
													// AllocTlsMemPool()로 부터 반환된 인덱스는 IOCP 작업자 스레드 도입부에서 CLanServer 멤버에 저장되었다 가정
#endif


	while(true) {
#if defined(dfLOCKFREE_QUEUE_SYNCHRONIZATION)
#if defined(dfPROCESSING_MODE_THREAD_RECV_INFO_QUEUE_POLLING)
		for (auto iter : server->m_TlsRecvQueueVec) {
			LockFreeQueue<stRecvInfo>* recvInfoQ = iter;
			size_t recvQueueSize = recvInfoQ->GetSize();
			for (size_t i = 0; i < recvQueueSize; i++) {
				stRecvInfo recvInfo;
				if (!recvInfoQ->Dequeue(recvInfo)) {
					DebugBreak();
				}
				UINT64 sessionID = recvInfo.sessionID;
				size_t recvSize = recvInfo.recvMsgCnt;

				server->ProcessMessage(sessionID, recvSize);
			}
		}
#elif defined(dfPROCESSING_MODE_THREAD_SINGLE_JOB_QUEUE_POLLING)
		if (server->m_MessageLockFreeQueue.GetSize() > 0) {
			std::pair<UINT64, JBuffer*> jobMsg;
			if (!server->m_MessageLockFreeQueue.Dequeue(jobMsg)) {
				DebugBreak();
			}
			server->ProcessMessage(jobMsg.first, jobMsg.second);
		}
#endif
#else
#if defined(dfPROCESSING_MODE_THREAD_RECV_INFO_QUEUE_EVENT)
		DWORD ret = WaitForMultipleObjects(server->m_WorkerThreadCnt, server->m_WorkerThreadRecvEvents, FALSE, INFINITE);
		if (WAIT_OBJECT_0 <= ret && ret < WAIT_OBJECT_0 + server->m_WorkerThreadCnt) {
			HANDLE recvEvent = server->m_WorkerThreadRecvEvents[ret];
			std::queue<stRecvInfo>& recvQ = server->m_ThreadEventRecvqMap[recvEvent];
			CRITICAL_SECTION* lockPtr = server->m_ThreadEventLockMap[recvEvent];		// 임시 동기화 객체

			size_t recvQueueSize = recvQ.size();	// -> 이 크기에 포함되지 않은 수신 정보는 다음 이벤트에서 확인됨이 보장.
			// OnRecv 쪽에서 (1) 수신 정보 인큐, (2) SetEvent 순 이기에...
			for (size_t i = 0; i < recvQueueSize; i++) {
				EnterCriticalSection(lockPtr);							// 임시 동기화 객체
				stRecvInfo& recvInfo = recvQ.front();
				UINT64 sessionID = recvInfo.sessionID;
				size_t recvSize = recvInfo.recvMsgCnt;
				recvQ.pop();
				LeaveCriticalSection(lockPtr);							// 임시 동기화 객체	

				server->ProcessMessage(sessionID, recvSize);
#if defined(CONNECT_MOINTORING_SERVER)
				server->m_UpdateThreadTransaction++;
#endif
			}
		}
		else {
			// WaitForMultipleObjects Error..
			DebugBreak();
		}
#elif defined(dfPROCESSING_MODE_THREAD_RECV_INFO_QUEUE_POLLING)
		for (auto iter : server->m_TlsRecvQueueVec) {
			std::queue<stRecvInfo>* recvInfoQ = iter.first;
			CRITICAL_SECTION* lockPtr = iter.second;
			size_t recvQueueSize = recvInfoQ->size();
			for (size_t i = 0; i < recvQueueSize; i++) {
				EnterCriticalSection(lockPtr);							// 임시 동기화 객체
				stRecvInfo& recvInfo = recvInfoQ->front();
				UINT64 sessionID = recvInfo.sessionID;
				size_t recvSize = recvInfo.recvMsgCnt;
				recvInfoQ->pop();
				LeaveCriticalSection(lockPtr);							// 임시 동기화 객체	

				server->ProcessMessage(sessionID, recvSize);
#if defined(CONNECT_MOINTORING_SERVER)
				server->m_UpdateThreadTransaction++;
#endif
			}
		}
#endif
#endif
	}
}

#if defined(dfPROCESSING_MODE_THREAD_SINGLE_JOB_QUEUE_POLLING)
void ChattingServer::ProcessMessage(UINT64 sessionID, JBuffer* msg) {
	WORD type;
	msg->Peek(&type);
	switch (type)
	{
	case en_SESSION_RELEASE:
	{
		Proc_SessionRelease(sessionID);
	}
	break;
	case en_SESSION_RELEASE_BEFORE_LOGIN:
	{
		Proc_SessionRelease(sessionID, true);
	}
	break;
	case en_PACKET_CS_CHAT_REQ_LOGIN:
	{
		if (msg->GetUseSize() < sizeof(MSG_PACKET_CS_CHAT_REQ_LOGIN)) {
			DebugBreak();
		}
		MSG_PACKET_CS_CHAT_REQ_LOGIN loginReqMsg;
		(*msg) >> loginReqMsg;	// 복사 없이 내부 버퍼를 그대로 캐스팅하는 방법은??
		Proc_REQ_LOGIN(sessionID, loginReqMsg);
	}
	break;
	case en_PACKET_CS_CHAT_REQ_SECTOR_MOVE:
	{
		if (msg->GetUseSize() < sizeof(MSG_PACKET_CS_CHAT_REQ_SECTOR_MOVE)) {
			DebugBreak();
		}
		MSG_PACKET_CS_CHAT_REQ_SECTOR_MOVE moveReqMsg;
		(*msg) >> moveReqMsg;	// 복사 없이 내부 버퍼를 그대로 캐스팅하는 방법은??
		Proc_REQ_SECTOR_MOVE(sessionID, moveReqMsg);
	}
	break;
	case en_PACKET_CS_CHAT_REQ_MESSAGE:
	{
		if (msg->GetUseSize() < sizeof(MSG_PACKET_CS_CHAT_REQ_MESSAGE)) {
			DebugBreak();
		}
		MSG_PACKET_CS_CHAT_REQ_MESSAGE chatReqMsg;
		(*msg) >> chatReqMsg;	// 복사 없이 내부 버퍼를 그대로 캐스팅하는 방법은??
		BYTE* message = new BYTE[chatReqMsg.MessageLen];
		memcpy(message, msg->GetDequeueBufferPtr(), chatReqMsg.MessageLen);
		Proc_REQ_MESSAGE(sessionID, chatReqMsg, message);
		delete[] message;
	}
	break;
	case en_PACKET_CS_CHAT_REQ_HEARTBEAT:
		break;
	default:
		DebugBreak();
		break;
	}

#if defined(ALLOC_BY_TLS_MEM_POOL)
	//m_SerialBuffPoolMgr.GetTlsMemPool().FreeMem(msg, to_string(sessionID) + ", FreeMem (ChattingServer::ProcessMessage)");
	FreeSerialBuff(msg);
#else
	delete msg;
#endif

#if defined(CONNECT_MOINTORING_SERVER)
	m_UpdateThreadTransaction++;
#endif
}
#else
void ChattingServer::ProcessMessage(UINT64 sessionID, size_t msgCnt)
{
#if defined(PLAYER_CREATE_RELEASE_LOG)
	stPlayerLog& playerLog = GetPlayerLog();
	playerLog.procMsgFlag = true;
	playerLog.sessionID = sessionID;
	playerLog.sessinIdIndex = (uint16)sessionID;
	playerLog.uint0 = msgCnt;
#endif

#if defined(dfLOCKFREE_QUEUE_SYNCHRONIZATION)
#if defined(dfPROCESSING_MODE_THREAD_RECV_INFO_QUEUE_POLLING)
	// 세션 별 메시지 큐로부터 메시지 획득
	AcquireSRWLockShared(&m_SessionMessageqMapSrwLock);
	auto msgQueueIter = m_SessionMessageQueueMap.find(sessionID);
	if (msgQueueIter == m_SessionMessageQueueMap.end()) {
		// 세션 삭제 메시지가 먼저 큐에 들어옴 -> 세션 관련 자료구조 삭제
		DebugBreak();
		return;
	}
	LockFreeQueue<JBuffer*>& sessionMsgQ = msgQueueIter->second;
	ReleaseSRWLockShared(&m_SessionMessageqMapSrwLock);
#endif
#else
	AcquireSRWLockShared(&m_SessionMessageqMapSrwLock);

	auto msgQueueIter = m_SessionMessageQueueMap.find(sessionID);
	auto msgQueueLockIter = m_SessionMessageQueueLockMap.find(sessionID);
	if (msgQueueIter == m_SessionMessageQueueMap.end() || msgQueueLockIter == m_SessionMessageQueueLockMap.end()) {
		// 세션 삭제 메시지가 먼저 큐에 들어옴 -> 세션 관련 자료구조 삭제
		DebugBreak();
		return;
	}
	std::queue<JBuffer*>& sessionMsgQ = msgQueueIter->second;
	CRITICAL_SECTION* lockPtr = msgQueueLockIter->second;

	ReleaseSRWLockShared(&m_SessionMessageqMapSrwLock);

	EnterCriticalSection(lockPtr);	// 임시 동기화 객체
	if (sessionMsgQ.empty()) {
		DebugBreak();
	}
	LeaveCriticalSection(lockPtr);	// 임시 동기화 객체
#endif

	for (size_t i = 0; i < msgCnt; i++) {
#if defined(dfLOCKFREE_QUEUE_SYNCHRONIZATION)
#if defined(dfPROCESSING_MODE_THREAD_RECV_INFO_QUEUE_POLLING)
		JBuffer* msg;
		if (!sessionMsgQ.Dequeue(msg)) {
			DebugBreak();
		}
#endif
#else
		EnterCriticalSection(lockPtr);						// 임시 동기화 객체
		JBuffer* msg = sessionMsgQ.front();
		sessionMsgQ.pop();
		LeaveCriticalSection(lockPtr);						// 임시 동기화 객체
#endif
		WORD type;
		msg->Peek(&type);
		switch (type)
		{
		case en_SESSION_RELEASE:
		{
			Proc_SessionRelease(sessionID);
		}
		break;
		case en_SESSION_RELEASE_BEFORE_LOGIN:
		{
			Proc_SessionRelease(sessionID, true);
		}
		break;
		case en_PACKET_CS_CHAT_REQ_LOGIN:
		{
			if (msg->GetUseSize() < sizeof(MSG_PACKET_CS_CHAT_REQ_LOGIN)) {
				DebugBreak();
			}
			MSG_PACKET_CS_CHAT_REQ_LOGIN loginReqMsg;
			(*msg) >> loginReqMsg;	// 복사 없이 내부 버퍼를 그대로 캐스팅하는 방법은??
			Proc_REQ_LOGIN(sessionID, loginReqMsg);
		}
		break;
		case en_PACKET_CS_CHAT_REQ_SECTOR_MOVE:
		{
			if (msg->GetUseSize() < sizeof(MSG_PACKET_CS_CHAT_REQ_SECTOR_MOVE)) {
				DebugBreak();
			}
			MSG_PACKET_CS_CHAT_REQ_SECTOR_MOVE moveReqMsg;
			(*msg) >> moveReqMsg;	// 복사 없이 내부 버퍼를 그대로 캐스팅하는 방법은??
			Proc_REQ_SECTOR_MOVE(sessionID, moveReqMsg);
		}
		break;
		case en_PACKET_CS_CHAT_REQ_MESSAGE:
		{
			if (msg->GetUseSize() < sizeof(MSG_PACKET_CS_CHAT_REQ_MESSAGE)) {
				DebugBreak();
			}
			MSG_PACKET_CS_CHAT_REQ_MESSAGE chatReqMsg;
			(*msg) >> chatReqMsg;	// 복사 없이 내부 버퍼를 그대로 캐스팅하는 방법은??
			BYTE* message = new BYTE[chatReqMsg.MessageLen];
			memcpy(message, msg->GetDequeueBufferPtr(), chatReqMsg.MessageLen);
			Proc_REQ_MESSAGE(sessionID, chatReqMsg, message);
			delete[] message;
		}
		break;
		case en_PACKET_CS_CHAT_REQ_HEARTBEAT:
			break;
		default:
			DebugBreak();
			break;
		}

#if defined(ALLOC_BY_TLS_MEM_POOL)
		//m_SerialBuffPoolMgr.GetTlsMemPool().FreeMem(msg, to_string(sessionID) + ", FreeMem (ChattingServer::ProcessMessage)");
		FreeSerialBuff(msg);
#else
		delete msg;
#endif

#if defined(CONNECT_MOINTORING_SERVER)
		m_UpdateThreadTransaction++;
#endif
	}
}
#endif

UINT __stdcall ChattingServer::PerformanceCountFunc(void* arg)
{
	ChattingServer* chatserver = (ChattingServer*)arg;
	chatserver->m_SerialBuffPoolMgr.AllocTlsMemPool();

	JBuffer* loginMsg = chatserver->AllocSerialSendBuff(sizeof(WORD) + sizeof(int));
	*loginMsg << (WORD)en_PACKET_SS_MONITOR_LOGIN;
	*loginMsg << (int)dfSERVER_CHAT_SERVER;
	chatserver->SendPacketToCLanServer(loginMsg);

	while (!chatserver->m_PerfCountStop) {
		time_t now = time(NULL);

		chatserver->m_UpdateThreadTps = chatserver->m_UpdateThreadTransaction;
		chatserver->m_UpdateThreadTransaction = 0;

		chatserver->m_PerfCounter->ResetPerfCounter();
		chatserver->m_MontDataMap[dfMONITOR_DATA_TYPE_CHAT_SERVER_RUN].dataValue = 1;
		chatserver->m_MontDataMap[dfMONITOR_DATA_TYPE_CHAT_SERVER_RUN].timeStamp = now;
		chatserver->m_MontDataMap[dfMONITOR_DATA_TYPE_CHAT_SERVER_CPU].dataValue = chatserver->m_PerfCounter->ProcessTotal();
		chatserver->m_MontDataMap[dfMONITOR_DATA_TYPE_CHAT_SERVER_CPU].timeStamp = now;
		chatserver->m_MontDataMap[dfMONITOR_DATA_TYPE_CHAT_SERVER_MEM].dataValue = chatserver->m_PerfCounter->GetPerfCounter(CHAT_SERVER_MEMORY_USAGE_QUERY);
		chatserver->m_MontDataMap[dfMONITOR_DATA_TYPE_CHAT_SERVER_MEM].dataValue /= 1'000'000;	// 서버 메모리 사용량 MB
		chatserver->m_MontDataMap[dfMONITOR_DATA_TYPE_CHAT_SERVER_MEM].timeStamp = now;
		chatserver->m_MontDataMap[dfMONITOR_DATA_TYPE_CHAT_SESSION].dataValue = chatserver->GetSessionCount();		// GetSessionCount
		chatserver->m_MontDataMap[dfMONITOR_DATA_TYPE_CHAT_SESSION].timeStamp = now;
		chatserver->m_MontDataMap[dfMONITOR_DATA_TYPE_CHAT_PLAYER].dataValue = chatserver->m_SessionIdAccountMap.size();
		chatserver->m_MontDataMap[dfMONITOR_DATA_TYPE_CHAT_PLAYER].timeStamp = now;
		chatserver->m_MontDataMap[dfMONITOR_DATA_TYPE_CHAT_UPDATE_TPS].dataValue = chatserver->m_UpdateThreadTps;
		chatserver->m_MontDataMap[dfMONITOR_DATA_TYPE_CHAT_UPDATE_TPS].timeStamp = now;
		chatserver->m_MontDataMap[dfMONITOR_DATA_TYPE_CHAT_PACKET_POOL].dataValue = chatserver->GetAllocMemPoolUsageSize();
		chatserver->m_MontDataMap[dfMONITOR_DATA_TYPE_CHAT_PACKET_POOL].timeStamp = now;
		chatserver->m_MontDataMap[dfMONITOR_DATA_TYPE_CHAT_UPDATEMSG_POOL].dataValue = chatserver->GetUpdateMsgPoolSize();
		chatserver->m_MontDataMap[dfMONITOR_DATA_TYPE_CHAT_UPDATEMSG_POOL].timeStamp = now;

		chatserver->SendPerfCountMsg();

		Sleep(1000);
	}
	return 0;
}

void ChattingServer::SendPerfCountMsg()
{
	JBuffer* perfMsg = AllocSerialBuff();
	for (auto iter : m_MontDataMap) {
		BYTE dataType = iter.first;
		stMontData& montData = iter.second;

		stMSG_HDR* hdr = perfMsg->DirectReserve<stMSG_HDR>();
		hdr->code = dfPACKET_CODE;
		hdr->len = sizeof(WORD) + sizeof(BYTE) + sizeof(int) + sizeof(int);
		hdr->randKey = (BYTE)(-1);
		stMSG_MONITOR_DATA_UPDATE* body = perfMsg->DirectReserve<stMSG_MONITOR_DATA_UPDATE>();
		body->Type = en_PACKET_SS_MONITOR_DATA_UPDATE;
		body->DataType = dataType;
		body->DataValue = montData.dataValue;
		body->TimeStamp = montData.timeStamp;
	}

	SendPacketToCLanServer(perfMsg);
}

/*
//# 원본 데이터 바이트 단위  D1 D2 D3 D4
//----------------------------------------------------------------------------------------------------------
//| D1 | D2 | D3 | D4 |
//----------------------------------------------------------------------------------------------------------
//D1 ^ (RK + 1) = P1 | D2 ^ (P1 + RK + 2) = P2 | D3 ^ (P2 + RK + 3) = P3 | D4 ^ (P3 + RK + 4) = P4 |
//P1 ^ (K + 1) = E1 | P2 ^ (E1 + K + 2) = E2 | P3 ^ (E2 + K + 3) = E3 | P4 ^ (E3 + K + 4) = E4 |
//
//# 암호 데이터 바이트 단위  E1 E2 E3 E4
//----------------------------------------------------------------------------------------------------------
//E1                      E2                          E3                           E4
//----------------------------------------------------------------------------------------------------------
bool ChattingServer::Decode(BYTE randKey, USHORT payloadLen, BYTE checkSum, BYTE* payloads)
{
	BYTE Pb = checkSum ^ (dfPACKET_KEY + 1);
	BYTE payloadSum = Pb ^ (randKey + 1);
	BYTE Eb = checkSum;

	for (USHORT i = 1; i <= payloadLen; i++) {
		BYTE Pn = payloads[i - 1] ^ (Eb + dfPACKET_KEY + (BYTE)(i + 1));
		BYTE Dn = Pn ^ (Pb + randKey + (BYTE)(i + 1));

		Pb = Pn;
		Eb = payloads[i - 1];
		payloads[i - 1] = Dn;
	}

	// checksum 검증
	BYTE payloadSumCmp = 0;
	for (USHORT i = 0; i < payloadLen; i++) {
		payloadSumCmp += payloads[i];
		payloadSumCmp %= 256;
	}
	if (payloadSum != payloadSumCmp) {
		DebugBreak();
		return false;
	}

	return true;
}
void ChattingServer::Encode(BYTE randKey, USHORT payloadLen, BYTE& checkSum, BYTE* payloads)
{
	BYTE payloadSum = 0;
	for (USHORT i = 0; i < payloadLen; i++) {
		payloadSum += payloads[i];
		payloadSum %= 256;
	}
	BYTE Pb = payloadSum ^ (randKey + 1);
	BYTE Eb = Pb ^ (dfPACKET_KEY + 1);
	checkSum = Eb;

	for (USHORT i = 1; i <= payloadLen; i++) {
		BYTE Pn = payloads[i - 1] ^ (Pb + randKey + (BYTE)(i + 1));
		BYTE En = Pn ^ (Eb + dfPACKET_KEY + (BYTE)(i + 1));

		payloads[i - 1] = En;

		Pb = Pn;
		Eb = En;
	}

#if defined(ENCODE_TEST)
	BYTE* encodedPayloads = new BYTE[payloadLen];
	memcpy(encodedPayloads, payloads, payloadLen);
	Decode(randKey, payloadLen, checkSum, encodedPayloads);
#endif
}
*/

void ChattingServer::Proc_REQ_LOGIN(UINT64 sessionID, MSG_PACKET_CS_CHAT_REQ_LOGIN& body)
{
#if defined(PLAYER_CREATE_RELEASE_LOG)
	USHORT playerLogIdx = InterlockedIncrement16((short*)&m_PlayerLogIdx);
	memset(&m_PlayerLog[playerLogIdx], 0, sizeof(stPlayerLog));
#endif

	bool releaseBeforeLogin = false;
	{
		std::lock_guard<std::mutex> lockGuard(m_LoginWaitSessionsMtx);
		if (m_LoginWaitSessions.find(sessionID) != m_LoginWaitSessions.end()) {
			m_LoginWaitSessions.erase(sessionID);
		}
		else {
			releaseBeforeLogin = true;
			// 로그인 요청 메시지 이전에 세션이 종료될 수 있다.
		}
	}

	if (!releaseBeforeLogin) {

		stAccoutInfo accountInfo;
		memcpy(&accountInfo, &body.AccountNo, sizeof(stAccoutInfo));
		accountInfo.X = -1;
		AcquireSRWLockExclusive(&m_SessionAccountMapSrwLock);
		m_SessionIdAccountMap.insert({ sessionID, accountInfo });
		ReleaseSRWLockExclusive(&m_SessionAccountMapSrwLock);

#if defined(PLAYER_CREATE_RELEASE_LOG)
		m_PlayerLog[playerLogIdx].packetID = en_PACKET_CS_CHAT_REQ_LOGIN;
		m_PlayerLog[playerLogIdx].sessionID = sessionID;
		uint64 sessionIdx = sessionID;
		m_PlayerLog[playerLogIdx].sessinIdIndex = (uint16)sessionIdx;
		m_PlayerLog[playerLogIdx].accountNo = accountInfo.AccountNo;
#endif

		Send_RES_LOGIN(sessionID, true, accountInfo.AccountNo);

#if defined(SESSION_LOG)
		InterlockedIncrement64(&m_TotalLoginCnt);
#endif
	}
}

void ChattingServer::Send_RES_LOGIN(UINT64 sessionID, BYTE STATUS, INT64 AccountNo)
{	
#if defined(PLAYER_CREATE_RELEASE_LOG)
	USHORT playerLogIdx = InterlockedIncrement16((short*)&m_PlayerLogIdx);
	memset(&m_PlayerLog[playerLogIdx], 0, sizeof(stPlayerLog));
#endif

#if defined(ALLOC_BY_TLS_MEM_POOL)
	// 공통 라이브러리 헤더까지 할당
	//JBuffer* sendMessage = AllocSerialSendBuff(sizeof(MSG_PACKET_CS_CHAT_RES_LOGIN), sessionID, "Send_RES_LOGIN");	
	JBuffer* sendMessage = AllocSerialSendBuff(sizeof(MSG_PACKET_CS_CHAT_RES_LOGIN));
#else
	std::shared_ptr<JBuffer> sendMessage = make_shared<JBuffer>();
	stMSG_HDR* hdr = sendMessage->DirectReserve<stMSG_HDR>();
	hdr->code = dfPACKET_CODE;
	hdr->len = sizeof(MSG_PACKET_CS_CHAT_RES_LOGIN);
	hdr->randKey = (BYTE)rand();
#endif

	(*sendMessage) << (WORD)en_PACKET_CS_CHAT_RES_LOGIN << STATUS << AccountNo;

#if defined(ALLOC_BY_TLS_MEM_POOL)
	if (!SendPacket(sessionID, sendMessage)) {
		FreeSerialBuff(sendMessage);
#else
	if (!SendPacket(sessionID, sendMessage)) {
#endif

#if defined(PLAYER_CREATE_RELEASE_LOG)
		m_PlayerLog[playerLogIdx].sendSuccess = false;
#endif
	}
	else {
#if defined(PLAYER_CREATE_RELEASE_LOG)
		m_PlayerLog[playerLogIdx].sendSuccess = true;
#endif
	}

#if defined(PLAYER_CREATE_RELEASE_LOG)
	m_PlayerLog[playerLogIdx].packetID = en_PACKET_CS_CHAT_RES_LOGIN;
	m_PlayerLog[playerLogIdx].sessionID = sessionID;
	uint64 sessionIdx = sessionID;
	m_PlayerLog[playerLogIdx].sessinIdIndex = (uint16)sessionIdx;
	m_PlayerLog[playerLogIdx].accountNo = AccountNo;
#endif
	
}

void ChattingServer::Proc_REQ_SECTOR_MOVE(UINT64 sessionID, MSG_PACKET_CS_CHAT_REQ_SECTOR_MOVE& body)
{
#if defined(PLAYER_CREATE_RELEASE_LOG)
	USHORT playerLogIdx = InterlockedIncrement16((short*)&m_PlayerLogIdx);
	memset(&m_PlayerLog[playerLogIdx], 0, sizeof(stPlayerLog));
#endif

	AcquireSRWLockShared(&m_SessionAccountMapSrwLock);
	stAccoutInfo& accountInfo = m_SessionIdAccountMap[sessionID];
	ReleaseSRWLockShared(&m_SessionAccountMapSrwLock);
	//std::cout << "기존 X: " << accountInfo.X << ", Y: " << accountInfo.Y << std::endl;
	if (accountInfo.X >= 0 && accountInfo.X <= dfSECTOR_X_MAX && accountInfo.Y >= 0 && accountInfo.X <= dfSECTOR_Y_MAX) {
		AcquireSRWLockExclusive(&m_SectorSrwLock[accountInfo.Y][accountInfo.X]);
		std::set<UINT64>& sector = m_SectorMap[accountInfo.Y][accountInfo.X];
		sector.erase(sessionID);
		ReleaseSRWLockExclusive(&m_SectorSrwLock[accountInfo.Y][accountInfo.X]);
	}

	if (body.SectorX < 0 || body.SectorX > dfSECTOR_X_MAX || body.SectorY < 0 || body.SectorY > dfSECTOR_Y_MAX) {
		// 범위 초과
		DebugBreak();
	}
	else {
		accountInfo.X = body.SectorX;
		accountInfo.Y = body.SectorY;
		m_SectorMap[accountInfo.Y][accountInfo.X].insert(sessionID);
	}

	//std::cout << "신규 X: " << accountInfo.X << ", 신규 Y: " << accountInfo.Y << std::endl;

#if defined(PLAYER_CREATE_RELEASE_LOG)
	m_PlayerLog[playerLogIdx].joinFlag = false;
	m_PlayerLog[playerLogIdx].leaveFlag = false;
	m_PlayerLog[playerLogIdx].packetID = en_PACKET_CS_CHAT_REQ_SECTOR_MOVE;
	m_PlayerLog[playerLogIdx].sessionID = sessionID;
	//m_PlayerLog[playerLogIdx].sessinIdIndex = (uint16)sessionID;
	uint64 sessionIdx = sessionID;
	//sessionIdx <<= 48;
	m_PlayerLog[playerLogIdx].sessinIdIndex = (uint16)sessionIdx;
	m_PlayerLog[playerLogIdx].accountNo = accountInfo.AccountNo;
#endif

	Send_RES_SECTOR_MOVE(sessionID, accountInfo.AccountNo, accountInfo.X, accountInfo.Y);
}

void ChattingServer::Send_RES_SECTOR_MOVE(UINT64 sessionID, INT64 AccountNo, WORD SectorX, WORD SectorY)
{
#if defined(PLAYER_CREATE_RELEASE_LOG)
	USHORT playerLogIdx = InterlockedIncrement16((short*)&m_PlayerLogIdx);
	memset(&m_PlayerLog[playerLogIdx], 0, sizeof(stPlayerLog));
#endif

#if defined(ALLOC_BY_TLS_MEM_POOL)
	//JBuffer* sendMessage = AllocSerialSendBuff(sizeof(MSG_PACKET_CS_CHAT_RES_SECTOR_MOVE), sessionID, "Send_RES_SECTOR_MOVE");
	JBuffer* sendMessage = AllocSerialSendBuff(sizeof(MSG_PACKET_CS_CHAT_RES_SECTOR_MOVE));
#else
	std::shared_ptr<JBuffer> sendMessage = make_shared<JBuffer>();
	stMSG_HDR* hdr = sendMessage->DirectReserve<stMSG_HDR>();
	hdr->code = dfPACKET_CODE;
	hdr->len = sizeof(MSG_PACKET_CS_CHAT_RES_SECTOR_MOVE);
	hdr->randKey = (BYTE)rand();
#endif

	(*sendMessage) << (WORD)en_PACKET_CS_CHAT_RES_SECTOR_MOVE << AccountNo << SectorX << SectorY;

#if defined(ALLOC_BY_TLS_MEM_POOL)
	if (!SendPacket(sessionID, sendMessage)) {
		FreeSerialBuff(sendMessage);
#else
	if (!SendPacket(sessionID, sendMessage)) {
#endif

#if defined(PLAYER_CREATE_RELEASE_LOG)
		m_PlayerLog[playerLogIdx].sendSuccess = false;
#endif
	}
	else {
#if defined(PLAYER_CREATE_RELEASE_LOG)
		m_PlayerLog[playerLogIdx].sendSuccess = true;
#endif
	}

#if defined(PLAYER_CREATE_RELEASE_LOG)
	m_PlayerLog[playerLogIdx].packetID = en_PACKET_CS_CHAT_RES_SECTOR_MOVE;
	m_PlayerLog[playerLogIdx].sessionID = sessionID;
	uint64 sessionIdx = sessionID;
	m_PlayerLog[playerLogIdx].sessinIdIndex = (uint16)sessionIdx;
	m_PlayerLog[playerLogIdx].accountNo = AccountNo;
#endif
}

void ChattingServer::Proc_REQ_MESSAGE(UINT64 sessionID, MSG_PACKET_CS_CHAT_REQ_MESSAGE& body, BYTE* message)
{
#if defined(PLAYER_CREATE_RELEASE_LOG)
	USHORT playerLogIdx = InterlockedIncrement16((short*)&m_PlayerLogIdx);
	memset(&m_PlayerLog[playerLogIdx], 0, sizeof(stPlayerLog));
#endif


#if defined(ALLOC_BY_TLS_MEM_POOL)
	//JBuffer* sendMessage = AllocSerialSendBuff(sizeof(MSG_PACKET_CS_CHAT_RES_MESSAGE) + body.MessageLen, sessionID, "Proc_REQ_MESSAGE");
	JBuffer* sendMessage = AllocSerialSendBuff(sizeof(MSG_PACKET_CS_CHAT_RES_MESSAGE) + body.MessageLen);
#else
	std::shared_ptr<JBuffer> sendMessage = make_shared<JBuffer>();
	stMSG_HDR* hdr = sendMessage->DirectReserve<stMSG_HDR>();
	hdr->code = dfPACKET_CODE;
	hdr->len = sizeof(MSG_PACKET_CS_CHAT_RES_MESSAGE) + body.MessageLen;
	hdr->randKey = (BYTE)rand();
#endif

	AcquireSRWLockShared(&m_SessionAccountMapSrwLock);
	stAccoutInfo& accountInfo = m_SessionIdAccountMap[sessionID];
	ReleaseSRWLockShared(&m_SessionAccountMapSrwLock);

#if defined(PLAYER_CREATE_RELEASE_LOG)
	m_PlayerLog[playerLogIdx].packetID = en_PACKET_CS_CHAT_REQ_MESSAGE;
	m_PlayerLog[playerLogIdx].sessionID = sessionID;
	//m_PlayerLog[playerLogIdx].sessinIdIndex = (uint16)sessionID;
	uint64 sessionIdx = sessionID;
	//sessionIdx <<= 48;
	m_PlayerLog[playerLogIdx].sessinIdIndex = (uint16)sessionIdx;
	m_PlayerLog[playerLogIdx].accountNo = accountInfo.AccountNo;
#endif

	(*sendMessage) << (WORD)en_PACKET_CS_CHAT_RES_MESSAGE;
	(*sendMessage) << accountInfo.AccountNo;
	(*sendMessage) << accountInfo.ID;
	(*sendMessage) << accountInfo.Nickname;
	(*sendMessage) << body.MessageLen;
	sendMessage->Enqueue(message, body.MessageLen);

	
	bool ownMsgFlag = false;
	for (int y = accountInfo.Y - 1; y <= accountInfo.Y + 1; y++) {
		for (int x = accountInfo.X - 1; x <= accountInfo.X + 1; x++) {
			if (y < 0 || y > dfSECTOR_Y_MAX || x < 0 || x > dfSECTOR_X_MAX) {
				continue;
			}

			AcquireSRWLockShared(&m_SectorSrwLock[y][x]);
			std::set<UINT64>& sector = m_SectorMap[y][x];
			for (auto iter = sector.begin(); iter != sector.end(); iter++) {
				// 예외 처리?
				if (*iter == sessionID) {
					ownMsgFlag = true;
				}

#if defined(PLAYER_CREATE_RELEASE_LOG)
				USHORT logIdx = InterlockedIncrement16((short*)&m_PlayerLogIdx);
				memset(&m_PlayerLog[logIdx], 0, sizeof(stPlayerLog));
#endif

#if defined(ALLOC_BY_TLS_MEM_POOL)
				AddRefSerialBuff(sendMessage, to_string(sessionID) + ", Forwaring Chat Msg to " + to_string(*iter));

				if (!SendPacket(*iter, sendMessage)) {
					FreeSerialBuff(sendMessage);
#else
				std::shared_ptr<JBuffer> sptr = sendMessage;
				if(!SendPacket(*iter, sptr)) {
#endif

#if defined(PLAYER_CREATE_RELEASE_LOG)
					m_PlayerLog[logIdx].sendSuccess = false;
#endif
				}
				else {
#if defined(PLAYER_CREATE_RELEASE_LOG)
					m_PlayerLog[logIdx].sendSuccess = true;
#endif
				}
#if defined(PLAYER_CREATE_RELEASE_LOG)
				m_PlayerLog[logIdx].packetID = en_PACKET_CS_CHAT_RES_MESSAGE;
				m_PlayerLog[logIdx].sessionID = sessionID;
				uint64 sessionIdx = sessionID;
				m_PlayerLog[logIdx].sessinIdIndex = (uint16)sessionIdx;
				m_PlayerLog[logIdx].accountNo = accountInfo.AccountNo;
				m_PlayerLog[logIdx].sessionID_dest = *iter;
#endif
			}
			ReleaseSRWLockShared(&m_SectorSrwLock[y][x]);
		}
	}
	
#if defined(ALLOC_BY_TLS_MEM_POOL)
	//m_SerialBuffPoolMgr.GetTlsMemPool().FreeMem(sendMessage, to_string(sessionID) + ", FreeMem (ChattingServer::Proc_REQ_MESSAGE)");
	FreeSerialBuff(sendMessage);
#endif
}

void ChattingServer::Proc_REQ_HEARTBEAT()
{
}

void ChattingServer::Proc_SessionRelease(UINT64 sessionID, bool beforeLogin)
{
	AcquireSRWLockShared(&m_SessionAccountMapSrwLock);
	stAccoutInfo& accountInfo = m_SessionIdAccountMap[sessionID];
	ReleaseSRWLockShared(&m_SessionAccountMapSrwLock);

#if defined(PLAYER_CREATE_RELEASE_LOG)
	stPlayerLog& playerLog = GetPlayerLog();
	playerLog.joinFlag = false;
	playerLog.leaveFlag = false;
	playerLog.packetID = en_SESSION_RELEASE;
	playerLog.sessionID = sessionID;
	playerLog.sessinIdIndex = (uint16)sessionID;
	playerLog.accountNo = accountInfo.AccountNo;
#endif

#if !defined(dfPROCESSING_MODE_THREAD_SINGLE_JOB_QUEUE_POLLING)
	//////////////////////////////////////////////////////////////
	// 세션 자료구조 정리
	//////////////////////////////////////////////////////////////
	AcquireSRWLockExclusive(&m_SessionMessageqMapSrwLock);
#if !defined(dfLOCKFREE_QUEUE_SYNCHRONIZATION)
	auto lockIter = m_SessionMessageQueueLockMap.find(sessionID);
	if (lockIter != m_SessionMessageQueueLockMap.end()) {
		CRITICAL_SECTION* lockPtr = lockIter->second;
		DeleteCriticalSection(lockPtr);
		m_SessionMessageQueueLockMap.erase(lockIter);
		delete lockPtr;
	}
#endif
	auto iter = m_SessionMessageQueueMap.find(sessionID);
	if (iter != m_SessionMessageQueueMap.end()) {
#if defined(dfLOCKFREE_QUEUE_SYNCHRONIZATION)
		LockFreeQueue<JBuffer*>& sessionMsgQ = m_SessionMessageQueueMap[sessionID];
		// 남은 메시지 정리
		while (sessionMsgQ.GetSize() > 0) {
			JBuffer* msg;
			if (!sessionMsgQ.Dequeue(msg)) {
				DebugBreak();
			}
#if defined(ALLOC_BY_TLS_MEM_POOL)
			FreeSerialBuff(msg);
#else
			delete msg;
#endif
		}
#else
		std::queue<JBuffer*>& sessionMsgQ = m_SessionMessageQueueMap[sessionID];
		// 남은 메시지 정리
		while (!sessionMsgQ.empty()) {
			JBuffer* msg = sessionMsgQ.front();
			sessionMsgQ.pop();
#if defined(ALLOC_BY_TLS_MEM_POOL)
			FreeSerialBuff(msg);
#else
			delete msg;
#endif
		}
#endif	
		m_SessionMessageQueueMap.erase(iter);
	}
	ReleaseSRWLockExclusive(&m_SessionMessageqMapSrwLock);

#endif

	if (!beforeLogin) {
		//////////////////////////////////////////////////////////////
		// account 및 섹터 자료구조 정리
		//////////////////////////////////////////////////////////////
		if (accountInfo.X >= 0 && accountInfo.X <= dfSECTOR_X_MAX && accountInfo.Y >= 0 && accountInfo.X <= dfSECTOR_Y_MAX) {
			AcquireSRWLockExclusive(&m_SectorSrwLock[accountInfo.Y][accountInfo.X]);
			std::set<UINT64>& sector = m_SectorMap[accountInfo.Y][accountInfo.X];
			sector.erase(sessionID);
			ReleaseSRWLockExclusive(&m_SectorSrwLock[accountInfo.Y][accountInfo.X]);
		}

		AcquireSRWLockExclusive(&m_SessionAccountMapSrwLock);
		m_SessionIdAccountMap.erase(sessionID);
		ReleaseSRWLockExclusive(&m_SessionAccountMapSrwLock);
	}
}

#if defined(PLAYER_CREATE_RELEASE_LOG)
void ChattingServer::PlayerFileLog()
{
	time_t now = time(0);
	struct tm timeinfo;
	char buffer[80];
	localtime_s(&timeinfo, &now);
	strftime(buffer, sizeof(buffer), "PlayerLog-%Y-%m-%d_%H-%M-%S", &timeinfo);
	std::string currentDateTime = std::string(buffer);

	// 파일 경로 생성
	std::string filePath = "./" + currentDateTime + ".txt";

	// 파일 스트림 열기
	std::ofstream outputFile(filePath);

	if (!outputFile) {
		std::cerr << "파일을 열 수 없습니다." << std::endl;
		return;
	}

	outputFile << currentDateTime << std::endl;

	for (USHORT i = 0; i <= m_DeletedSendPacketIdx; i++) {
		if (m_DeletedSendPacketLog[i].accountNo == 0) {
			break;
		}

		outputFile << "-------------------------------------------------" << std::endl;
		if (m_DeletedSendPacketLog[i].type == en_PACKET_CS_CHAT_RES_LOGIN) {
			outputFile << "[RES_LOGIN Deleted from SendBuffer]" << std::endl;
		}
		else if (m_DeletedSendPacketLog[i].type == en_PACKET_CS_CHAT_RES_SECTOR_MOVE) {
			outputFile << "[RES_SECTOR_MOVE Deleted from SendBuffer]" << std::endl;
		}
		else if (m_DeletedSendPacketLog[i].type == en_PACKET_CS_CHAT_RES_MESSAGE) {
			outputFile << "[RES_MESSAGE Deleted from SendBuffer]" << std::endl;
		}
		else {
			DebugBreak();
		}
		outputFile << "accountID: " << m_DeletedSendPacketLog[i].accountNo << std::endl;
	}

	for (USHORT i = 0; i <= m_PlayerLogIdx; i++) {
		if (m_PlayerLog[i].sessionID == 0) {
			break;
		}

		outputFile << "-------------------------------------------------" << std::endl;
		if (m_PlayerLog[i].joinFlag) {
			outputFile << "[OnClientJoin]" << std::endl;
			outputFile << "sessionID   : " << m_PlayerLog[i].sessionID << std::endl;
			outputFile << "sessionIndex: " << m_PlayerLog[i].sessinIdIndex << std::endl;
		}
		else if (m_PlayerLog[i].leaveFlag) {
			outputFile << "[OnClientLeave]" << std::endl;
			outputFile << "sessionID   : " << m_PlayerLog[i].sessionID << std::endl;
			outputFile << "sessionIndex: " << m_PlayerLog[i].sessinIdIndex << std::endl;
		}
		else if (m_PlayerLog[i].recvFlag) {
			outputFile << "[OnRecv]" << std::endl;
			outputFile << "sessionID   : " << m_PlayerLog[i].sessionID << std::endl;
			outputFile << "sessionIndex: " << m_PlayerLog[i].sessinIdIndex << std::endl;
			outputFile << "recvInfo.cnt: " << m_PlayerLog[i].uint0 << std::endl;
		}
		else if (m_PlayerLog[i].procMsgFlag) {
			outputFile << "[ProcessMessage]" << std::endl;
			outputFile << "sessionID   : " << m_PlayerLog[i].sessionID << std::endl;
			outputFile << "sessionIndex: " << m_PlayerLog[i].sessinIdIndex << std::endl;
			outputFile << "msgCnt      : " << m_PlayerLog[i].uint0 << std::endl;
		}
		else {
			if (m_PlayerLog[i].packetID == en_PACKET_CS_CHAT_REQ_LOGIN) {
				outputFile << "[REQ_LOGIN]" << std::endl;
			}
			else if (m_PlayerLog[i].packetID == en_PACKET_CS_CHAT_REQ_SECTOR_MOVE) {
				outputFile << "[REQ_SECTOR_MOVE]" << std::endl;
			}
			else if (m_PlayerLog[i].packetID == en_PACKET_CS_CHAT_REQ_MESSAGE) {
				outputFile << "[REQ_MESSAGE]" << std::endl;
			}
			else if (m_PlayerLog[i].packetID == en_SESSION_RELEASE) {
				outputFile << "[SESSION_RELEASE]" << std::endl;
			}
			else if (m_PlayerLog[i].packetID == en_SESSION_RELEASE_BEFORE_LOGIN) {
				outputFile << "[SESSION_RELEASE_BEFORE_LOGIN]" << std::endl;
			}
			else if (m_PlayerLog[i].packetID == en_PACKET_CS_CHAT_RES_LOGIN) {
				if (m_PlayerLog[i].sendSuccess) {
					outputFile << "[RES_LOGIN SUCCESS]" << std::endl;
				}
				else {
					outputFile << "[RES_LOGIN FAIL]" << std::endl;
				}
			}
			else if (m_PlayerLog[i].packetID == en_PACKET_CS_CHAT_RES_SECTOR_MOVE) {
				if (m_PlayerLog[i].sendSuccess) {
					outputFile << "[RES_SECTOR_MOVE SUCCESS]" << std::endl;
				}
				else {
					outputFile << "[RES_SECTOR_MOVE FAIL]" << std::endl;
				}
			}
			else if (m_PlayerLog[i].packetID == en_PACKET_CS_CHAT_RES_MESSAGE) {
				if (m_PlayerLog[i].sendSuccess) {
					outputFile << "[RES_MESSAGE SUCCESS]" << std::endl;
				}
				else {
					outputFile << "[RES_MESSAGE FAIL]" << std::endl;
				}
				outputFile << "destination(sessionID): " << m_PlayerLog[i].sessionID_dest << std::endl;
			}
			else {
				DebugBreak();
			}

			outputFile << "sessionID   : " << m_PlayerLog[i].sessionID << std::endl;
			outputFile << "sessionIndex: " << m_PlayerLog[i].sessinIdIndex << std::endl;
			outputFile << "accountNo   : " << m_PlayerLog[i].accountNo << std::endl;
		}
	}

}
#endif