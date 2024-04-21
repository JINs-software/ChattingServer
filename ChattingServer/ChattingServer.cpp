#include "ChattingServer.h"

UINT __stdcall ChattingServer::ProcessThreadFunc(void* arg)
{
	ChattingServer* server = (ChattingServer*)arg;

	while (true) {
		DWORD ret = WaitForMultipleObjects(server->m_WorkerThreadCnt, server->m_WorkerThreadRecvEvents, FALSE, INFINITE);
		if (WAIT_OBJECT_0 <= ret && ret < WAIT_OBJECT_0 + server->m_WorkerThreadCnt) {
			HANDLE recvEvent = server->m_WorkerThreadRecvEvents[ret];
			std::queue<stRecvInfo>& recvQ = server->m_ThreadEventRecvqMap[recvEvent];
			stRecvInfo& recvInfo = recvQ.front();
			UINT64 sessionID = recvInfo.sessionID;
			size_t recvSize = recvInfo.recvMsgCnt;
			recvQ.pop();

			server->ForwardChattingMessage(sessionID, recvSize);
		}
		else {
			// WaitForMultipleObjects Error..
			DebugBreak();
		}

	}
}

void ChattingServer::ProcessMessage(UINT64 sessionID, size_t msgCnt)
{
	// .. 세션 별 메시지 큐 삽입
	AcquireSRWLockShared(&m_SessionMessageqMapSrwLock);
	std::queue<JBuffer*>& sessionMsgQ = m_SessionMessageQueueMap[sessionID];
	for (size_t i = 0; i < msgCnt; i++) {
		JBuffer* msg = sessionMsgQ.front();
		sessionMsgQ.pop();

		WORD type;
		msg->Peek(&type);
		switch (type)
		{
		case en_PACKET_CS_CHAT_REQ_LOGIN:
			MSG_PACKET_CS_CHAT_REQ_LOGIN loginReqMsg;
			(*msg) >> loginReqMsg;	// 복사 없이 내부 버퍼를 그대로 캐스팅하는 방법은??
			Proc_REQ_LOGIN(sessionID, loginReqMsg);
			break;
		case en_PACKET_CS_CHAT_REQ_SECTOR_MOVE:
			MSG_PACKET_CS_CHAT_REQ_SECTOR_MOVE moveReqMsg;
			(*msg) >> moveReqMsg;	// 복사 없이 내부 버퍼를 그대로 캐스팅하는 방법은??
			Proc_REQ_SECTOR_MOVE(sessionID, moveReqMsg);
			break;
		case en_PACKET_CS_CHAT_REQ_MESSAGE:
			MSG_PACKET_CS_CHAT_REQ_MESSAGE chatReqMsg;
			(*msg) >> chatReqMsg;	// 복사 없이 내부 버퍼를 그대로 캐스팅하는 방법은??
			Proc_REQ_MESSAGE(sessionID, chatReqMsg);
			break;
		case en_PACKET_CS_CHAT_REQ_HEARTBEAT:
			break;
		default:
			DebugBreak();
			break;
		}
	}
	ReleaseSRWLockShared(&m_SessionMessageqMapSrwLock);
}

void ChattingServer::ForwardChattingMessage(UINT64 sessionID, size_t recvSize)
{
	AcquireSRWLockShared(&m_SessionMessageqMapSrwLock);
	std::queue<JBuffer*>& sessionMsgQ = m_SessionMessageQueueMap[sessionID];
	ReleaseSRWLockShared(&m_SessionMessageqMapSrwLock);

	// recvSize 만큼 pop!
}

bool ChattingServer::OnWorkerThreadCreate(HANDLE thHnd)
{
	if (m_WorkerThreadCnt >= MAX_WORKER_THREAD_CNT) {
		return false;
	}

	HANDLE recvEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	UINT8 eventIdx = m_WorkerThreadCnt++;
	m_WorkerThreadRecvEvents[eventIdx] = recvEvent;
	thEventIndexMap.insert({ thHnd, recvEvent });

}

void ChattingServer::OnWorkerThreadStart() {
	if (TlsGetValue(m_RecvEventTlsIndex) == NULL) {
		HANDLE thEvent = thEventIndexMap[GetCurrentThread()];
		TlsSetValue(m_RecvEventTlsIndex, thEvent);

		m_ThreadEventRecvqMap.insert({GetCurrentThread(), std::queue<stRecvInfo>()});
	}
	else {
		DebugBreak();
	}
}

bool ChattingServer::OnConnectionRequest()
{
	return true;
}

void ChattingServer::OnClientJoin(uint64 sessionID)
{
	{
		std::lock_guard<std::mutex> lockGuard(m_LoginWaitSessionsMtx);
		m_LoginWaitSessions.insert(sessionID);
	}
}

void ChattingServer::OnClientLeave(uint64 sessionID)
{
}

void ChattingServer::OnRecv(uint64 sessionID, JBuffer& recvBuff)
{	
	HANDLE thHnd = GetCurrentThread();
	stRecvInfo recvInfo;
	recvInfo.sessionID = sessionID;
	recvInfo.recvMsgCnt = 0;

	// [메시지 수신]	
	// (임시) 컨텐츠 단에서 Code 부터 인/디코딩까지 수행
	while (recvBuff.GetUseSize() >= sizeof(stMSG_HDR)) {
		stMSG_HDR msgHdr;
		recvBuff.Peek(&msgHdr);

		if (msgHdr.code != dfPACKET_CODE) {
			// 코드 불일치
			// 연결 강제 종료!

			break;
		}
		if (recvBuff.GetUseSize() < sizeof(stMSG_HDR) + msgHdr.len) {
			// 메시지 미완성
			break;
		}

		recvBuff >> msgHdr;

		// 직렬화 버퍼 생성
		JBuffer* message = new JBuffer(msgHdr.len);
		recvBuff.Dequeue(message->GetBeginBufferPtr(), msgHdr.len);
		message->DirectMoveEnqueueOffset(msgHdr.len);

		if (!Decode(msgHdr.randKey, msgHdr.len, msgHdr.checkSum, message->GetBeginBufferPtr())) {
			// 디코딩 실패
			// 연결 강제 종료

			break;
		}
	
		// .. 세션 별 메시지 큐 삽입
		AcquireSRWLockShared(&m_SessionMessageqMapSrwLock);
		std::queue<JBuffer*>& sessinMsgQ = m_SessionMessageQueueMap[sessionID];
		sessinMsgQ.push(message);
		ReleaseSRWLockShared(&m_SessionMessageqMapSrwLock);
		recvInfo.recvMsgCnt++;
	}

	if (recvInfo.recvMsgCnt > 0) {
		std::queue<stRecvInfo>& recvInfoQ = m_ThreadEventRecvqMap[thHnd];
		recvInfoQ.push(recvInfo);

		HANDLE recvEvent = (HANDLE)TlsGetValue(m_RecvEventTlsIndex);
		SetEvent(recvEvent);
	}

	
	//HANDLE recvEvent = (HANDLE)TlsGetValue(m_RecvEventTlsIndex);
	//
	//// 1. 스레드 별 메시지 링버퍼에 Enqueue
	//// (링버퍼를 사용하는 이유는 Reader와 Writer가 1:1 구조이기 때문에)
	//// 2. 링버퍼 Enqueue 후에는 Enq 이벤트를 시그널 상태로 변경
	//// 3. 컨텐츠 코드에선 스레드 별 Enq 이벤트 객체를 들고 있다. 이 이벤트에 대해 Wait을 한다. 
	//// 
	//
	//// 세션 별 메시지 큐에 삽입
	//AcquireSRWLockShared(&m_SessionMessageqMapSrwLock);
	//std::queue<JBuffer>& sessionMsgQ = m_SessionMessageqMap[sessionID];
	//ReleaseSRWLockShared(&m_SessionMessageqMapSrwLock);
	//size_t recvPacketSize = 0;
	//while (true) {
	//	if (recvBuff.GetUseSize() < sizeof(MSG_HEADER)) {
	//		break;
	//	}
	//	MSG_HEADER msgHdr;
	//	recvBuff.Peek<MSG_HEADER>(&msgHdr);
	//
	//	if (recvBuff.GetUseSize() < sizeof(MSG_HEADER) + msgHdr.msgLength) {
	//		break;
	//	}
	//
	//	// 내부 버퍼 복사..?
	//	//JBuffer msg()
	//	JBuffer jbuff;
	//
	//	sessionMsgQ.push(jbuff);
	//	recvPacketSize++;
	//}
	//
	//// m_ThreadEventRecvqMap 삽입
	//std::queue<stRecvInfo>& recvInfoQ = m_ThreadEventRecvqMap[recvEvent];
	//recvInfoQ.push({ sessionID, recvPacketSize });
	//
	//// ProcessThread에 알림!
	//SetEvent(recvEvent);
}

void ChattingServer::OnError()
{
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
	BYTE Eb = checkSum;
	BYTE Pb = checkSum ^ (dfPACKET_KEY + 1);
	BYTE payloadSum = Pb ^ (randKey + 1);

	for (USHORT i = 1; i <= payloadLen; i++) {
		BYTE En = payloads[i - 1];
		BYTE Pn = En ^ (Eb + dfPACKET_KEY + (BYTE)(i + 1));
		payloads[i - 1] = Pn ^ (Pb + randKey, (BYTE)(i + 1));
	}

	// checksum 검증
	BYTE payloadSumCmp = 0;
	for (USHORT i = 0; i < payloadLen; i++) {
		payloadSumCmp += payloads[i];
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
	checkSum = payloadSum ^ (randKey + 1);
	BYTE Pb = checkSum;
	BYTE Eb = Pb ^ (dfPACKET_KEY + 1);

	for (USHORT i = 1; i <= payloadLen; i++) {
		BYTE Pn = payloads[i - 1] ^ (Pb + randKey + (BYTE)(i + 1));
		BYTE En = Pn ^ (Eb + dfPACKET_KEY + (BYTE)(i + 1));

		payloads[i - 1] = En;

		Pb = Pn;
		Eb = En;
	}
}

void ChattingServer::Proc_REQ_LOGIN(UINT64 sessionID, MSG_PACKET_CS_CHAT_REQ_LOGIN& body)
{
	// 세션 연결 수립 -> LoginWait 셋 등록 전 해당 메시지 처리가 후에 처리됨이 보장되지 않을 수 있음
	// 로그인 대기 셋에 해당 세션이 존재하면 삭제하는 정도만 진행
	{
		std::lock_guard<std::mutex> lockGuard(m_LoginWaitSessionsMtx);
		if (m_LoginWaitSessions.find(sessionID) != m_LoginWaitSessions.end()) {
			m_LoginWaitSessions.erase(sessionID);
		}
	}

	AcquireSRWLockExclusive(&m_SessionMessageqMapSrwLock);
	m_SessionMessageQueueMap.insert({ sessionID, std::queue<JBuffer*>()});
	stAccoutInfo accountInfo;
	memcpy(&accountInfo, &body.AccountNo, sizeof(stAccoutInfo));
	accountInfo.X = -1;
	m_SessionIdAccountMap.insert({ sessionID, accountInfo});
	ReleaseSRWLockExclusive(&m_SessionMessageqMapSrwLock);
}

void ChattingServer::Send_RES_LOGIN(UINT64 sessionID, BYTE STATUS, INT64 AccountNo)
{
	// Unicast Reply
	
	SendPacket(sessionID, )
}

void ChattingServer::Proc_REQ_SECTOR_MOVE(UINT64 sessionID, MSG_PACKET_CS_CHAT_REQ_SECTOR_MOVE& body)
{
	stAccoutInfo& accountInfo = m_SessionIdAccountMap[sessionID];
	if (accountInfo.X != -1) {
		std::set<UINT64>& sector = m_SectorMap[accountInfo.Y][accountInfo.X];
		sector.erase(sessionID);
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
}

void ChattingServer::Send_RES_SECTOR_MOVE(UINT64 sessionID, INT64 AccountNo, WORD SectorX, WORD SectorY)
{
	// Unicast Reply
}

void ChattingServer::Proc_REQ_MESSAGE(UINT64 sessionID, MSG_PACKET_CS_CHAT_REQ_MESSAGE& body)
{
}

void ChattingServer::Send_RES_MESSAGE(UINT64 sessionID, INT64 AccountNo, WCHAR* ID, WCHAR Nickname, WORD MessageLen, WCHAR* Message)
{
	// Multicast Reply
}

void ChattingServer::Proc_REQ_HEARTBEAT()
{
}
