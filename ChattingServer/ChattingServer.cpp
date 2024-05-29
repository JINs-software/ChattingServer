#include "ChattingServer.h"
#include <fstream>

bool ChattingServer::OnWorkerThreadCreate(HANDLE thHnd)
{
	if (m_WorkerThreadCnt >= MAX_WORKER_THREAD_CNT) {
		return false;
	}

	HANDLE recvEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	UINT8 eventIdx = m_WorkerThreadCnt++;
	m_WorkerThreadRecvEvents[eventIdx] = recvEvent;
	DWORD thID = GetThreadId(thHnd);
	thEventIndexMap.insert({ thID, recvEvent });

	return true;
}

void ChattingServer::OnWorkerThreadCreateDone()
{
	//m_ProcessThreadHnd = (HANDLE)_beginthreadex(NULL, 0, ProcessThreadFunc, this, 0, NULL);
	for (UINT8 i = 0; i < m_ProcessThreadCnt; i++) {
		m_ProcessThreadHnds[i] = (HANDLE)_beginthreadex(NULL, 0, ProcessThreadFunc, this, 0, NULL);
		if (m_ProcessThreadHnds[i] == NULL) {
			DebugBreak();
		}
	}
}

void ChattingServer::OnWorkerThreadStart() {
	if (TlsGetValue(m_RecvEventTlsIndex) == NULL) {
		DWORD thID = GetThreadId(GetCurrentThread());
		HANDLE thEventHnd = thEventIndexMap[thID];
		TlsSetValue(m_RecvEventTlsIndex, thEventHnd);

		m_ThreadEventRecvqMap.insert({ thEventHnd, std::queue<stRecvInfo>()});
		// 임시 동기화 객체
		CRITICAL_SECTION* lockPtr = new CRITICAL_SECTION;
		InitializeCriticalSection(lockPtr);
		m_ThreadEventLockMap.insert({ thEventHnd, lockPtr });
	}
	else {
		DebugBreak();
	}
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

	// 2. 세션 별 메시지 큐 생성 및 메시지 큐 관리 자료구조에 삽입
	AcquireSRWLockExclusive(&m_SessionMessageqMapSrwLock);

	m_SessionMessageQueueMap.insert({ sessionID, std::queue<JBuffer*>()});
	// 임시 동기화 객체(메시지 큐 별 동기화 객체 생성)
	CRITICAL_SECTION* lockPtr = new CRITICAL_SECTION;
	if (lockPtr == NULL) {
		DebugBreak();
	}
	InitializeCriticalSection(lockPtr);
	m_SessionMessageQueueLockMap.insert({ sessionID, lockPtr });

	ReleaseSRWLockExclusive(&m_SessionMessageqMapSrwLock);
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
	JBuffer* message = (JBuffer*)m_SerialBuffPoolMgr.GetTlsMemPool().AllocMem(1, to_string(sessionID) + ", OnClientLeave");
	message->ClearBuffer();
#else
	JBuffer* message = new JBuffer();
#endif
	if(loginWaitSession) {
		(*message) << (WORD)en_SESSION_RELEASE_BEFORE_LOGIN;
	}
	else {
		(*message) << (WORD)en_SESSION_RELEASE;
	}

	// 메시지 큐 & 메시지 큐 동기화 객체 GET
	AcquireSRWLockShared(&m_SessionMessageqMapSrwLock);
	//std::queue<JBuffer*>& sessionMsgQ = m_SessionMessageQueueMap[sessionID];
	//CRITICAL_SECTION* lockPtr = m_SessionMessageQueueLockMap[sessionID];		// 임시 동기화 객체
	auto msgQueueIter = m_SessionMessageQueueMap.find(sessionID);
	auto msgQueueLockIter = m_SessionMessageQueueLockMap.find(sessionID);
	if (msgQueueIter == m_SessionMessageQueueMap.end() || msgQueueLockIter == m_SessionMessageQueueLockMap.end()) {
		SessionReleaseLog();
#if defined(PLAYER_CREATE_RELEASE_LOG)
		PlayerFileLog();
#endif
		DebugBreak();
	}
	std::queue<JBuffer*>& sessionMsgQ = msgQueueIter->second;
	CRITICAL_SECTION* lockPtr = msgQueueLockIter->second;
	ReleaseSRWLockShared(&m_SessionMessageqMapSrwLock);

	EnterCriticalSection(lockPtr);								// 임시 동기화 객체
	sessionMsgQ.push(message);
	LeaveCriticalSection(lockPtr);								// 임시 동기화 객체

	stRecvInfo recvInfo;
	recvInfo.sessionID = sessionID;
	recvInfo.recvMsgCnt = 1;
	HANDLE recvEvent = (HANDLE)TlsGetValue(m_RecvEventTlsIndex);
	std::queue<stRecvInfo>& recvInfoQ = m_ThreadEventRecvqMap[recvEvent];

	lockPtr = m_ThreadEventLockMap[recvEvent];								// 임시 동기화 객체
	EnterCriticalSection(lockPtr);							// 임시 동기화 객체
	recvInfoQ.push(recvInfo);
	LeaveCriticalSection(lockPtr);							// 임시 동기화 객체
	SetEvent(recvEvent);
}

void ChattingServer::OnRecv(uint64 sessionID, JBuffer& recvBuff)
{	
#if defined(PLAYER_CREATE_RELEASE_LOG)
	stPlayerLog& playerLog = GetPlayerLog();
	playerLog.recvFlag = true;
	playerLog.sessionID = sessionID;
	playerLog.sessinIdIndex = (uint16)sessionID;
#endif

	HANDLE thHnd = GetCurrentThread();

	// [메시지 수신]	
	// (임시) 컨텐츠 단에서 Code 부터 인/디코딩까지 수행
	while (recvBuff.GetUseSize() >= sizeof(stMSG_HDR)) {

		stRecvInfo recvInfo;
		recvInfo.sessionID = sessionID;
		recvInfo.recvMsgCnt = 0;

		stMSG_HDR msgHdr;
		recvBuff.Peek(&msgHdr);
		if (msgHdr.code != dfPACKET_CODE) {
			// 코드 불일치
			// 연결 강제 종료!
			DebugBreak();
			break;
		}
		if (recvBuff.GetUseSize() < sizeof(stMSG_HDR) + msgHdr.len) {
			// 메시지 미완성
			//DebugBreak();
			// => 메모리 풀 방식에서 스마트 포인터 방식 변경 시 미환성 메시지 수신 발생..
			break;
		}

		recvBuff >> msgHdr;
		if (!Decode(msgHdr.randKey, msgHdr.len, msgHdr.checkSum, recvBuff.GetDequeueBufferPtr())) {
			DebugBreak();
			// 연결 강제 종료?
		}

		UINT dequeueSize = 0;
		while (recvBuff.GetUseSize() > 0) {			/// ????		// [TO DO] msgHdr.len까지만 디큐잉 하는 것으로 변경
			WORD type;
			recvBuff.Peek(&type);

#if defined(ALLOC_BY_TLS_MEM_POOL)
			JBuffer* message = (JBuffer*)m_SerialBuffPoolMgr.GetTlsMemPool().AllocMem(1, to_string(sessionID) + ", OnRecv");
			message->ClearBuffer();
#else 
			JBuffer* message = new JBuffer();
#endif
			switch (type)
			{
			case en_PACKET_CS_CHAT_REQ_LOGIN:
			{
				recvBuff.Dequeue(message->GetDequeueBufferPtr(), sizeof(MSG_PACKET_CS_CHAT_REQ_LOGIN));
				message->DirectMoveEnqueueOffset(sizeof(MSG_PACKET_CS_CHAT_REQ_LOGIN));
				dequeueSize += sizeof(MSG_PACKET_CS_CHAT_REQ_LOGIN);
			}
				break;
			case en_PACKET_CS_CHAT_REQ_SECTOR_MOVE:
			{
				recvBuff.Dequeue(message->GetDequeueBufferPtr(), sizeof(MSG_PACKET_CS_CHAT_REQ_SECTOR_MOVE));
				message->DirectMoveEnqueueOffset(sizeof(MSG_PACKET_CS_CHAT_REQ_SECTOR_MOVE));
				dequeueSize += sizeof(MSG_PACKET_CS_CHAT_REQ_SECTOR_MOVE);
			}
				break;
			case en_PACKET_CS_CHAT_REQ_MESSAGE:
			{
				WORD messageLen;
				recvBuff.Peek(sizeof(WORD) + sizeof(INT64), (BYTE*)&messageLen, sizeof(WORD));
				recvBuff.Dequeue(message->GetDequeueBufferPtr(), sizeof(MSG_PACKET_CS_CHAT_REQ_MESSAGE) + messageLen);
				message->DirectMoveEnqueueOffset(sizeof(MSG_PACKET_CS_CHAT_REQ_MESSAGE) + messageLen);
				dequeueSize += sizeof(MSG_PACKET_CS_CHAT_REQ_MESSAGE) + messageLen;
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
			
			if (message != NULL) {
				// .. 세션 별 메시지 큐 삽입
				// 메시지 큐 & 메시지 큐 동기화 객체 GET
				AcquireSRWLockShared(&m_SessionMessageqMapSrwLock);
				//std::queue<JBuffer*>& sessionMsgQ = m_SessionMessageQueueMap[sessionID];
				//CRITICAL_SECTION* lockPtr = m_SessionMessageQueueLockMap[sessionID];		// 임시 동기화 객체
				auto msgQueueIter = m_SessionMessageQueueMap.find(sessionID);
				auto msgQueueLockIter = m_SessionMessageQueueLockMap.find(sessionID);
				if (msgQueueIter == m_SessionMessageQueueMap.end() || msgQueueLockIter == m_SessionMessageQueueLockMap.end()) {
					SessionReleaseLog();
#if defined(PLAYER_CREATE_RELEASE_LOG)
					PlayerFileLog();
#endif
					DebugBreak();
				}
				std::queue<JBuffer*>& sessionMsgQ = msgQueueIter->second;
				CRITICAL_SECTION* lockPtr = msgQueueLockIter->second;
				ReleaseSRWLockShared(&m_SessionMessageqMapSrwLock);

				EnterCriticalSection(lockPtr);								// 임시 동기화 객체
				sessionMsgQ.push(message);
				LeaveCriticalSection(lockPtr);								// 임시 동기화 객체

				recvInfo.recvMsgCnt++;
			}
			else {
				//m_SerialBuffPoolMgr.GetTlsMemPool().FreeMem(message, to_string(sessionID) + ", FreeMem (ChattingServer::OnRecv)");
				DebugBreak();
			}

			if (dequeueSize == msgHdr.len) {
				break;
			}
			else if (dequeueSize > msgHdr.len) {
				DebugBreak();
			}
		}

		if (recvInfo.recvMsgCnt > 0) {
#if defined(PLAYER_CREATE_RELEASE_LOG)
			playerLog.uint0 = recvInfo.recvMsgCnt;
#endif
			HANDLE recvEvent = (HANDLE)TlsGetValue(m_RecvEventTlsIndex);
			std::queue<stRecvInfo>& recvInfoQ = m_ThreadEventRecvqMap[recvEvent];
			CRITICAL_SECTION* lockPtr =  m_ThreadEventLockMap[recvEvent];			// 임시 동기화 객체
			EnterCriticalSection(lockPtr);							// 임시 동기화 객체
			recvInfoQ.push(recvInfo);
			LeaveCriticalSection(lockPtr);							// 임시 동기화 객체
			SetEvent(recvEvent);
		}
	}

	// 정상적인 Decode를 위해선 수신 링버퍼가 한 바퀴 도는 상황을 막아야 함. 추후 Encode, Decode는 라이브러리에서 진행하도록..
	if (recvBuff.GetUseSize() == 0) {
		recvBuff.ClearBuffer();
	}
}

void ChattingServer::OnError()
{
}

UINT __stdcall ChattingServer::ProcessThreadFunc(void* arg)
{
	ChattingServer* server = (ChattingServer*)arg;
#if defined(ALLOC_BY_TLS_MEM_POOL)
	// 상위 서버 클래스에서 관리하는 메모리 풀 
	server->m_SerialBuffPoolMgr.AllocTlsMemPool();	// 생성자에서 설정한 Default 값을 따름
													// AllocTlsMemPool()로 부터 반환된 인덱스는 IOCP 작업자 스레드 도입부에서 CLanServer 멤버에 저장되었다 가정
#endif

	// 종료 이벤트 추가
	HANDLE events[MAX_WORKER_THREAD_CNT + 1];
	events[0] = server->m_ProcessStopEvent;
	memcpy(&events[1], server->m_WorkerThreadRecvEvents, sizeof(m_WorkerThreadRecvEvents));

	while (true) {
		//DWORD ret = WaitForMultipleObjects(server->m_WorkerThreadCnt, server->m_WorkerThreadRecvEvents, FALSE, INFINITE);
		DWORD ret = WaitForMultipleObjects(server->m_WorkerThreadCnt + 1, events, FALSE, INFINITE);
		if (ret == WAIT_OBJECT_0) {
			break;
		}
		else if (WAIT_OBJECT_0 + 1 <= ret && ret < WAIT_OBJECT_0 + 1 + server->m_WorkerThreadCnt) {
			DWORD thEventIdx = ret - 1;
			// IOCP 작업자 스레드의 수신 이벤트(recvEvent), 수신 큐(recvQ), 수신 이벤트에 대한 동기화 객체(lockPtr) 관리 자료구조는
			// 프로세스 스레드의 동작 중에는 변경 없음을 가정.
			HANDLE recvEvent = server->m_WorkerThreadRecvEvents[thEventIdx];
			std::queue<stRecvInfo>& recvQ = server->m_ThreadEventRecvqMap[recvEvent];
			CRITICAL_SECTION* lockPtr = server->m_ThreadEventLockMap[recvEvent];		// 임시 동기화 객체

			//size_t recvQueueSize = recvQ.size();	// -> 이 크기에 포함되지 않은 수신 정보는 다음 이벤트에서 확인됨이 보장.
			//// OnRecv 쪽에서 (1) 수신 정보 인큐, (2) SetEvent 순 이기에...
			//for (size_t i = 0; i < recvQueueSize; i++) {
			//	EnterCriticalSection(lockPtr);							// 임시 동기화 객체
			//	stRecvInfo& recvInfo = recvQ.front();
			//	UINT64 sessionID = recvInfo.sessionID;
			//	size_t recvSize = recvInfo.recvMsgCnt;
			//	recvQ.pop();
			//	LeaveCriticalSection(lockPtr);							// 임시 동기화 객체	
			//
			//	server->ProcessMessage(sessionID, recvSize);
			//}

			while (true) {
				// 프로세싱 스레드가 단일 세션에 대해 recvInfo를 처리하는 동안은 작업자 스레드의 recvInfo ...
				stRecvInfo recvInfo;
				bool emptyFlag = true;
				EnterCriticalSection(lockPtr);
				if (!recvQ.empty()) {
					recvInfo = recvQ.front();
					recvQ.pop();
					emptyFlag = false;
				}
				LeaveCriticalSection(lockPtr);

				if (!emptyFlag) {
					server->ProcessMessage(recvInfo.sessionID, recvInfo.recvMsgCnt);
				}
				else {
					break;
				}
			}
		}
		else {
			// WaitForMultipleObjects Error..
			DebugBreak();
		}

	}

	return 0;
}

void ChattingServer::ProcessMessage(UINT64 sessionID, size_t msgCnt)
{
#if defined(PLAYER_CREATE_RELEASE_LOG)
	stPlayerLog& playerLog = GetPlayerLog();
	playerLog.procMsgFlag = true;
	playerLog.sessionID = sessionID;
	playerLog.sessinIdIndex = (uint16)sessionID;
	playerLog.uint0 = msgCnt;
#endif

	// 세션 별 메시지 큐로부터 메시지 획득
	AcquireSRWLockShared(&m_SessionMessageqMapSrwLock);

	auto msgQueueIter = m_SessionMessageQueueMap.find(sessionID);
	auto msgQueueLockIter = m_SessionMessageQueueLockMap.find(sessionID);
	if (msgQueueIter == m_SessionMessageQueueMap.end() || msgQueueLockIter == m_SessionMessageQueueLockMap.end()) {
		// 세션 삭제 메시지가 먼저 큐에 들어옴 -> 세션 관련 자료구조 삭제
		SessionReleaseLog();
#if defined(PLAYER_CREATE_RELEASE_LOG)
		PlayerFileLog();
#endif
		DebugBreak();
		return;
	}
	std::queue<JBuffer*>& sessionMsgQ = msgQueueIter->second;
	CRITICAL_SECTION* sessionMsgQLock = msgQueueLockIter->second;

	ReleaseSRWLockShared(&m_SessionMessageqMapSrwLock);


	for (size_t i = 0; i < msgCnt; i++) {
		EnterCriticalSection(sessionMsgQLock);						// 임시 동기화 객체
		JBuffer* msg = sessionMsgQ.front();
		sessionMsgQ.pop();

		//LeaveCriticalSection(sessionMsgQLock);						
		// => 단일 메시지 처리 동안은 메시지 큐 락 (멀티 스레드에서 단일 세션에서의 순서 제어)

		// 단일 세션에서의 순서 제어는 프로카데미 채팅 더미에서는 불필요함
		// 또한 메시지 처리 이후에 LeaveCriticalSection(sessionMsgQLock)를 호출하면 en_SESSION_RELEASE/en_SESSION_RELEASE_BEFORE_LOGIN의 메시지 처리의 경우 삭제된 동기화 객체에 대해 호출하여 메모리 오염을 유발함이 발견됨
		LeaveCriticalSection(sessionMsgQLock);

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
			Proc_SessionReleaseBeforeLogin(sessionID);
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
		m_SerialBuffPoolMgr.GetTlsMemPool().FreeMem(msg, to_string(sessionID) + ", FreeMem (ChattingServer::ProcessMessage)");
#else
		delete msg;
#endif

		//LeaveCriticalSection(sessionMsgQLock);
		// => 치명적인 결함 발생, 만약 메시지가 en_SESSION_RELEASE이나 en_SESSION_RELEASE_BEFORE_LOGIN를 처리하는 것이라면, 해당 처리 함수에서 세션 메시지 큐를 삭제함
		//	  따라서 메모리 오염이 발생하여, 다른 자료구조 관리 자료구조에 영향을 주는 것으로 확인)
	}
}

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
#if defined(SINGLE_UPDATE_THREAD)
		stAccoutInfo accountInfo;
		memcpy(&accountInfo, &body.AccountNo, sizeof(stAccoutInfo));
		accountInfo.X = -1;
		AcquireSRWLockExclusive(&m_SessionAccountMapSrwLock);
		m_SessionIdAccountMap.insert({ sessionID, accountInfo });
		ReleaseSRWLockExclusive(&m_SessionAccountMapSrwLock);
#else
		std::shared_ptr<stAccoutInfo> accountInfo = std::make_shared<stAccoutInfo>();
		accountInfo->AccountNo = body.AccountNo;
		accountInfo->X = -1;
		AcquireSRWLockExclusive(&m_SessionAccountMapSrwLock);
		m_SessionIdAccountMap.insert({ sessionID, accountInfo });
		ReleaseSRWLockExclusive(&m_SessionAccountMapSrwLock);
#endif

#if defined(PLAYER_CREATE_RELEASE_LOG)
		m_PlayerLog[playerLogIdx].packetID = en_PACKET_CS_CHAT_REQ_LOGIN;
		m_PlayerLog[playerLogIdx].sessionID = sessionID;
		uint64 sessionIdx = sessionID;
		m_PlayerLog[playerLogIdx].sessinIdIndex = (uint16)sessionIdx;
		m_PlayerLog[playerLogIdx].accountNo = accountInfo->AccountNo;
#endif

#if defined(SINGLE_UPDATE_THREAD)
		Send_RES_LOGIN(sessionID, true, accountInfo.AccountNo);
#else
		Send_RES_LOGIN(sessionID, true, accountInfo->AccountNo);
#endif

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
	JBuffer* sendMessage = m_SerialBuffPoolMgr.GetTlsMemPool().AllocMem(1, to_string(sessionID) + ", Send_RES_LOGIN");
	sendMessage->ClearBuffer();
#else
	std::shared_ptr<JBuffer> sendMessage = make_shared<JBuffer>();
#endif

	stMSG_HDR* hdr = sendMessage->DirectReserve<stMSG_HDR>();
	hdr->code = dfPACKET_CODE;
	hdr->len = sizeof(MSG_PACKET_CS_CHAT_RES_LOGIN);
	hdr->randKey = (BYTE)rand();

	(*sendMessage) << (WORD)en_PACKET_CS_CHAT_RES_LOGIN << STATUS << AccountNo;

	Encode(hdr->randKey, hdr->len, hdr->checkSum, sendMessage->GetBufferPtr(sizeof(stMSG_HDR)));

#if defined(ALLOC_BY_TLS_MEM_POOL)
	if (!SendPacket(sessionID, sendMessage)) {
		m_SerialBuffPoolMgr.GetTlsMemPool().FreeMem(sendMessage, "FreeMem (SendPacket Fail, Send_RES_LOGIN)");
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

#if defined(SINGLE_UPDATE_THREAD)
	AcquireSRWLockShared(&m_SessionAccountMapSrwLock);
	if (m_SessionIdAccountMap.find(sessionID) == m_SessionIdAccountMap.end()) {
		DebugBreak();
	}
	stAccoutInfo* accountInfo = &m_SessionIdAccountMap[sessionID];
	ReleaseSRWLockShared(&m_SessionAccountMapSrwLock);
#else
	// => 참조 카운터 증가 (스마트 포인터)
	AcquireSRWLockShared(&m_SessionAccountMapSrwLock);
	std::shared_ptr<stAccoutInfo> accountInfo = m_SessionIdAccountMap[sessionID];
	ReleaseSRWLockShared(&m_SessionAccountMapSrwLock);
#endif

	if (body.SectorX < 0 || body.SectorX > dfSECTOR_X_MAX || body.SectorY < 0 || body.SectorY > dfSECTOR_Y_MAX) {
		// 범위 초과
		DebugBreak();
	}
	else {
		// 실제 이동이 있는 경우만 섹터 변경
		if (accountInfo->Y != body.SectorY || accountInfo->X != body.SectorX) {
			if (accountInfo->X < 0 || accountInfo->X > dfSECTOR_X_MAX || accountInfo->Y < 0 || accountInfo->Y > dfSECTOR_Y_MAX) {
				// 섹터에 포함되지 않은 상태(로그인 직후)
				AcquireSRWLockExclusive(&m_SectorSrwLock[body.SectorY][body.SectorX]);
				std::set<UINT64>& sector = m_SectorMap[body.SectorY][body.SectorX];
				m_SectorMap[body.SectorY][body.SectorX].insert(sessionID);
				ReleaseSRWLockExclusive(&m_SectorSrwLock[body.SectorY][body.SectorX]);
			}
			else {
				// 잠금 순서 결정
				USHORT eraseSectorLockLevel = accountInfo->X + accountInfo->Y * (dfSECTOR_X_MAX + 1);
				USHORT insertSectorLockLevel = body.SectorX + body.SectorY * (dfSECTOR_X_MAX + 1);
				if (eraseSectorLockLevel < insertSectorLockLevel) {
					AcquireSRWLockExclusive(&m_SectorSrwLock[accountInfo->Y][accountInfo->X]);
					AcquireSRWLockExclusive(&m_SectorSrwLock[body.SectorY][body.SectorX]);
				}
				else {
					AcquireSRWLockExclusive(&m_SectorSrwLock[body.SectorY][body.SectorX]);
					AcquireSRWLockExclusive(&m_SectorSrwLock[accountInfo->Y][accountInfo->X]);
				}

				// 기존 영역에서 삭제
				if (m_SectorMap[accountInfo->Y][accountInfo->X].find(sessionID) == m_SectorMap[accountInfo->Y][accountInfo->X].end()) {
					DebugBreak();
				}
				m_SectorMap[accountInfo->Y][accountInfo->X].erase(sessionID);

				// 새로운 영역에 추가
				if (m_SectorMap[body.SectorY][body.SectorX].find(sessionID) != m_SectorMap[body.SectorY][body.SectorX].end()) {
					DebugBreak();
				}
				m_SectorMap[body.SectorY][body.SectorX].insert(sessionID);

				if (eraseSectorLockLevel < insertSectorLockLevel) {
					ReleaseSRWLockExclusive(&m_SectorSrwLock[body.SectorY][body.SectorX]);
					ReleaseSRWLockExclusive(&m_SectorSrwLock[accountInfo->Y][accountInfo->X]);
				}
				else {
					ReleaseSRWLockExclusive(&m_SectorSrwLock[accountInfo->Y][accountInfo->X]);
					ReleaseSRWLockExclusive(&m_SectorSrwLock[body.SectorY][body.SectorX]);
				}
			}

			accountInfo->X = body.SectorX;
			accountInfo->Y = body.SectorY;
		}
	}

	//std::cout << "신규 X: " << accountInfo->X << ", 신규 Y: " << accountInfo->Y << std::endl;

#if defined(PLAYER_CREATE_RELEASE_LOG)
	m_PlayerLog[playerLogIdx].joinFlag = false;
	m_PlayerLog[playerLogIdx].leaveFlag = false;
	m_PlayerLog[playerLogIdx].packetID = en_PACKET_CS_CHAT_REQ_SECTOR_MOVE;
	m_PlayerLog[playerLogIdx].sessionID = sessionID;
	//m_PlayerLog[playerLogIdx].sessinIdIndex = (uint16)sessionID;
	uint64 sessionIdx = sessionID;
	//sessionIdx <<= 48;
	m_PlayerLog[playerLogIdx].sessinIdIndex = (uint16)sessionIdx;
	m_PlayerLog[playerLogIdx].accountNo = accountInfo->AccountNo;
#endif

	Send_RES_SECTOR_MOVE(sessionID, accountInfo->AccountNo, accountInfo->X, accountInfo->Y);
}

void ChattingServer::Send_RES_SECTOR_MOVE(UINT64 sessionID, INT64 AccountNo, WORD SectorX, WORD SectorY)
{
#if defined(PLAYER_CREATE_RELEASE_LOG)
	USHORT playerLogIdx = InterlockedIncrement16((short*)&m_PlayerLogIdx);
	memset(&m_PlayerLog[playerLogIdx], 0, sizeof(stPlayerLog));
#endif

#if defined(ALLOC_BY_TLS_MEM_POOL)
	JBuffer* sendMessage = m_SerialBuffPoolMgr.GetTlsMemPool().AllocMem(1, to_string(sessionID) + ", Send_RES_SECTOR_MOVE");
	sendMessage->ClearBuffer();
#else
	std::shared_ptr<JBuffer> sendMessage = make_shared<JBuffer>();
#endif
	

	stMSG_HDR* hdr = sendMessage->DirectReserve<stMSG_HDR>();
	hdr->code = dfPACKET_CODE;
	hdr->len = sizeof(MSG_PACKET_CS_CHAT_RES_SECTOR_MOVE);
	hdr->randKey = (BYTE)rand();

	(*sendMessage) << (WORD)en_PACKET_CS_CHAT_RES_SECTOR_MOVE << AccountNo << SectorX << SectorY;

	Encode(hdr->randKey, hdr->len, hdr->checkSum, sendMessage->GetBufferPtr(sizeof(stMSG_HDR)));

#if defined(ALLOC_BY_TLS_MEM_POOL)
	if (!SendPacket(sessionID, sendMessage)) {
		m_SerialBuffPoolMgr.GetTlsMemPool().FreeMem(sendMessage, "FreeMem (SendPacket Fail, Send_RES_SECTOR_MOVE)");
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
	JBuffer* sendMessage = m_SerialBuffPoolMgr.GetTlsMemPool().AllocMem(1, to_string(sessionID) + ", Proc_REQ_MESSAGE");
	sendMessage->ClearBuffer();
#else
	std::shared_ptr<JBuffer> sendMessage = make_shared<JBuffer>();
#endif


	//tlsMemPool.IncrementRefCnt(sendMessage);		// 공유 전송 메시지

	stMSG_HDR* hdr = sendMessage->DirectReserve<stMSG_HDR>();
	hdr->code = dfPACKET_CODE;
	hdr->len = sizeof(MSG_PACKET_CS_CHAT_RES_MESSAGE) + body.MessageLen;
	hdr->randKey = (BYTE)rand();

	//AcquireSRWLockShared(&m_SessionAccountMapSrwLock);
	//stAccoutInfo& accountInfo = m_SessionIdAccountMap[sessionID];
	//ReleaseSRWLockShared(&m_SessionAccountMapSrwLock);
	// => 참조 카운터 증가 (스마트 포인터)
	AcquireSRWLockShared(&m_SessionAccountMapSrwLock);
#if defined(SINGLE_UPDATE_THREAD)
	if (m_SessionIdAccountMap.find(sessionID) == m_SessionIdAccountMap.end()) {
		DebugBreak();
	}
	stAccoutInfo* accountInfo = &m_SessionIdAccountMap[sessionID];
#else
	std::shared_ptr<stAccoutInfo> accountInfo = m_SessionIdAccountMap[sessionID];
#endif
	ReleaseSRWLockShared(&m_SessionAccountMapSrwLock);

#if defined(PLAYER_CREATE_RELEASE_LOG)
	m_PlayerLog[playerLogIdx].packetID = en_PACKET_CS_CHAT_REQ_MESSAGE;
	m_PlayerLog[playerLogIdx].sessionID = sessionID;
	//m_PlayerLog[playerLogIdx].sessinIdIndex = (uint16)sessionID;
	uint64 sessionIdx = sessionID;
	//sessionIdx <<= 48;
	m_PlayerLog[playerLogIdx].sessinIdIndex = (uint16)sessionIdx;
	m_PlayerLog[playerLogIdx].accountNo = accountInfo->AccountNo;
#endif

	(*sendMessage) << (WORD)en_PACKET_CS_CHAT_RES_MESSAGE;
	(*sendMessage) << accountInfo->AccountNo;
	(*sendMessage) << accountInfo->ID;
	(*sendMessage) << accountInfo->Nickname;
	(*sendMessage) << body.MessageLen;
	sendMessage->Enqueue(message, body.MessageLen);

	Encode(hdr->randKey, hdr->len, hdr->checkSum, sendMessage->GetBufferPtr(sizeof(stMSG_HDR)));
	
	// 메시지를 전달할 세션들을 중복 불가 셋에 삽입
	std::set<UINT64> destinationSessions;

	//bool ownMsgFlag = false;
	for (int y = accountInfo->Y - 1; y <= accountInfo->Y + 1; y++) {
		for (int x = accountInfo->X - 1; x <= accountInfo->X + 1; x++) {
			if (y < 0 || y > dfSECTOR_Y_MAX || x < 0 || x > dfSECTOR_X_MAX) {
				continue;
			}

			AcquireSRWLockShared(&m_SectorSrwLock[y][x]);
			std::set<UINT64>& sector = m_SectorMap[y][x];
			for (auto iter = sector.begin(); iter != sector.end(); iter++) {


//#if defined(PLAYER_CREATE_RELEASE_LOG)
//				USHORT logIdx = InterlockedIncrement16((short*)&m_PlayerLogIdx);
//				memset(&m_PlayerLog[logIdx], 0, sizeof(stPlayerLog));
//#endif
//
//				tlsMemPool.IncrementRefCnt(sendMessage, 1, to_string(sessionID) + ", Forwaring Chat Msg to " + to_string(*iter));
//				//std::cout << "[Proc_REQ_MESSAGE | SendPacekt] sessionID: " << *iter << std::endl;
//				if (!SendPacket(*iter, sendMessage)) {
//					m_SerialBuffPoolMgr.GetTlsMemPool().FreeMem(sendMessage, "FreeMem (SendPacket Fail, Forwaring Chat Mst)");
//
//#if defined(PLAYER_CREATE_RELEASE_LOG)
//					m_PlayerLog[logIdx].sendSuccess = false;
//#endif
//				}
//				else {
//#if defined(PLAYER_CREATE_RELEASE_LOG)
//					m_PlayerLog[logIdx].sendSuccess = true;
//#endif
//				}


				//destinationSessions.insert(*iter);
				// => 하단 범위 기반 for문에서 operator++ 런타임 에러 발생(트리 노드의 parent 참조 시 에러 원인 분석 필요)
				UINT64 destinationSessionID = *iter;
				destinationSessions.insert(destinationSessionID);

//#if defined(PLAYER_CREATE_RELEASE_LOG)
//				m_PlayerLog[logIdx].packetID = en_PACKET_CS_CHAT_RES_MESSAGE;
//				m_PlayerLog[logIdx].sessionID = sessionID;
//				uint64 sessionIdx = sessionID;
//				m_PlayerLog[logIdx].sessinIdIndex = (uint16)sessionIdx;
//				m_PlayerLog[logIdx].accountNo = accountInfo->AccountNo;
//				m_PlayerLog[logIdx].sessionID_dest = *iter;
//#endif
			}
			ReleaseSRWLockShared(&m_SectorSrwLock[y][x]);
		}
	}

#if defined(ALLOC_BY_TLS_MEM_POOL)
	for (auto destination : destinationSessions) {
		m_SerialBuffPoolMgr.GetTlsMemPool().IncrementRefCnt(sendMessage, 1, to_string(sessionID) + ", Forwaring Chat Msg to " + to_string(destination));
		if (!SendPacket(destination, sendMessage)) {
			m_SerialBuffPoolMgr.GetTlsMemPool().FreeMem(sendMessage, "FreeMem (SendPacket Fail, Forwaring Chat Mst)");
		}
	}

	m_SerialBuffPoolMgr.GetTlsMemPool().FreeMem(sendMessage, to_string(sessionID) + ", FreeMem (ChattingServer::Proc_REQ_MESSAGE)");
#else
	for (auto destination : destinationSessions) {
		std::shared_ptr<JBuffer> sptr = sendMessage;
		SendPacket(destination, sptr);
	}
#endif
}

void ChattingServer::Proc_REQ_HEARTBEAT()
{
}

void ChattingServer::Proc_SessionRelease(UINT64 sessionID)
{
	//AcquireSRWLockShared(&m_SessionAccountMapSrwLock);
	//stAccoutInfo& accountInfo = m_SessionIdAccountMap[sessionID];
	//ReleaseSRWLockShared(&m_SessionAccountMapSrwLock);
	// => 참조 카운터 증가 (스마트 포인터)
	AcquireSRWLockShared(&m_SessionAccountMapSrwLock);
#if defined(SINGLE_UPDATE_THREAD)
	if (m_SessionIdAccountMap.find(sessionID) == m_SessionIdAccountMap.end()) {
		DebugBreak();
	}
	stAccoutInfo* accountInfo = &m_SessionIdAccountMap[sessionID];
#else
	std::shared_ptr<stAccoutInfo> accountInfo = m_SessionIdAccountMap[sessionID];
#endif
	ReleaseSRWLockShared(&m_SessionAccountMapSrwLock);


#if defined(PLAYER_CREATE_RELEASE_LOG)
	stPlayerLog& playerLog = GetPlayerLog();
	playerLog.joinFlag = false;
	playerLog.leaveFlag = false;
	playerLog.packetID = en_SESSION_RELEASE;
	playerLog.sessionID = sessionID;
	playerLog.sessinIdIndex = (uint16)sessionID;
	playerLog.accountNo = accountInfo->AccountNo;
#endif

	// 세션 자료구조 정리
	AcquireSRWLockExclusive(&m_SessionMessageqMapSrwLock);
	auto lockIter = m_SessionMessageQueueLockMap.find(sessionID);
	if (lockIter != m_SessionMessageQueueLockMap.end()) {
		CRITICAL_SECTION* lockPtr = lockIter->second;
		DeleteCriticalSection(lockPtr);
		m_SessionMessageQueueLockMap.erase(lockIter);
		delete lockPtr;
	}
	auto iter = m_SessionMessageQueueMap.find(sessionID);
	if (iter != m_SessionMessageQueueMap.end()) {
		std::queue<JBuffer*>& sessionMsgQ = m_SessionMessageQueueMap[sessionID];
		
		// 남은 메시지 정리
		while (!sessionMsgQ.empty()) {
			JBuffer* msg = sessionMsgQ.front();
			sessionMsgQ.pop();
#if defined(ALLOC_BY_TLS_MEM_POOL)
			m_SerialBuffPoolMgr.GetTlsMemPool().FreeMem(msg, to_string(sessionID) + ", FreeMem (ChattingServer::Proc_SessionRelease)");
#else
			delete msg;
#endif
		}
		
		m_SessionMessageQueueMap.erase(iter);
	}
	ReleaseSRWLockExclusive(&m_SessionMessageqMapSrwLock);

	// account 및 섹터 자료구조 정리
	//stAccoutInfo& accountInfo = m_SessionIdAccountMap[sessionID];

	if (accountInfo->X >= 0 && accountInfo->X <= dfSECTOR_X_MAX && accountInfo->Y >= 0 && accountInfo->X <= dfSECTOR_Y_MAX) {
		AcquireSRWLockExclusive(&m_SectorSrwLock[accountInfo->Y][accountInfo->X]);
		if (m_SectorMap[accountInfo->Y][accountInfo->X].find(sessionID) == m_SectorMap[accountInfo->Y][accountInfo->X].end()) {
			DebugBreak();
		}
		m_SectorMap[accountInfo->Y][accountInfo->X].erase(sessionID);
		ReleaseSRWLockExclusive(&m_SectorSrwLock[accountInfo->Y][accountInfo->X]);
	}

	AcquireSRWLockExclusive(&m_SessionAccountMapSrwLock);
	m_SessionIdAccountMap.erase(sessionID);
	ReleaseSRWLockExclusive(&m_SessionAccountMapSrwLock);
}

void ChattingServer::Proc_SessionReleaseBeforeLogin(UINT64 sessionID) {
#if defined(PLAYER_CREATE_RELEASE_LOG)
	stPlayerLog& playerLog = GetPlayerLog();
	playerLog.joinFlag = false;
	playerLog.leaveFlag = false;
	playerLog.packetID = en_SESSION_RELEASE_BEFORE_LOGIN;
	playerLog.sessionID = sessionID;
	playerLog.sessinIdIndex = (uint16)sessionID;
#endif

	// 세션 자료구조 정리
	AcquireSRWLockExclusive(&m_SessionMessageqMapSrwLock);
	auto lockIter = m_SessionMessageQueueLockMap.find(sessionID);
	if (lockIter != m_SessionMessageQueueLockMap.end()) {
		CRITICAL_SECTION* lockPtr = lockIter->second;
		DeleteCriticalSection(lockPtr);
		m_SessionMessageQueueLockMap.erase(lockIter);
		delete lockPtr;
	}
	auto iter = m_SessionMessageQueueMap.find(sessionID);
	if (iter != m_SessionMessageQueueMap.end()) {
		std::queue<JBuffer*>& sessionMsgQ = m_SessionMessageQueueMap[sessionID];

		// 남은 메시지 정리
		while (!sessionMsgQ.empty()) {
			JBuffer* msg = sessionMsgQ.front();
			sessionMsgQ.pop();
#if defined(ALLOC_BY_TLS_MEM_POOL)
			m_SerialBuffPoolMgr.GetTlsMemPool().FreeMem(msg, to_string(sessionID) + ", FreeMem (ChattingServer::Proc_SessionRelease)");
#else
			delete msg;
#endif
		}

		m_SessionMessageQueueMap.erase(iter);
	}
	ReleaseSRWLockExclusive(&m_SessionMessageqMapSrwLock);
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