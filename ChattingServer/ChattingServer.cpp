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

			server->ProcessMessage(sessionID, recvSize);
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
	AcquireSRWLockShared(&m_SessionMessageqMapSrwLock);									// <= 데드락 발생 시초				
	std::queue<JBuffer*>& sessionMsgQ = m_SessionMessageQueueMap[sessionID];
	ReleaseSRWLockShared(&m_SessionMessageqMapSrwLock);
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
	DWORD thID = GetThreadId(thHnd);
	thEventIndexMap.insert({ thID, recvEvent });

}

void ChattingServer::OnWorkerThreadCreateDone()
{
	m_ProcessThreadHnd = (HANDLE)_beginthreadex(NULL, 0, ProcessThreadFunc, this, 0, NULL);
}

void ChattingServer::OnWorkerThreadStart() {
	if (TlsGetValue(m_RecvEventTlsIndex) == NULL) {
		DWORD thID = GetThreadId(GetCurrentThread());
		HANDLE thEventHnd = thEventIndexMap[thID];
		TlsSetValue(m_RecvEventTlsIndex, thEventHnd);

		m_ThreadEventRecvqMap.insert({ thEventHnd, std::queue<stRecvInfo>()});
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
		
		//// 직렬화 버퍼 생성
		//recvBuff >> msgHdr;
		//JBuffer* message = new JBuffer(msgHdr.len);
		//recvBuff.Dequeue(message->GetBeginBufferPtr(), msgHdr.len);
		//message->DirectMoveEnqueueOffset(msgHdr.len);
		//
		//if (!Decode(msgHdr.randKey, msgHdr.len, msgHdr.checkSum, message->GetBeginBufferPtr())) {
		//	// 디코딩 실패
		//	// 연결 강제 종료
		//	DebugBreak();
		//	break;
		//}

		recvBuff >> msgHdr;
		if (!Decode(msgHdr.randKey, msgHdr.len, msgHdr.checkSum, recvBuff.GetDequeueBufferPtr())) {
			DebugBreak();
			// 연결 강제 종료?
		}

		while (recvBuff.GetUseSize() > 0) {
			WORD type;
			recvBuff.Peek(&type);

			JBuffer* message = NULL;
			switch (type)
			{
			case en_PACKET_CS_CHAT_REQ_LOGIN:
			{
				message = new JBuffer(sizeof(MSG_PACKET_CS_CHAT_REQ_LOGIN));
				recvBuff.Dequeue(message->GetDequeueBufferPtr(), sizeof(MSG_PACKET_CS_CHAT_REQ_LOGIN));
				message->DirectMoveEnqueueOffset(sizeof(MSG_PACKET_CS_CHAT_REQ_LOGIN));
			}
				break;
			case en_PACKET_CS_CHAT_REQ_SECTOR_MOVE:
			{
				message = new JBuffer(sizeof(MSG_PACKET_CS_CHAT_REQ_SECTOR_MOVE));
				recvBuff.Dequeue(message->GetDequeueBufferPtr(), sizeof(MSG_PACKET_CS_CHAT_REQ_SECTOR_MOVE));
				message->DirectMoveEnqueueOffset(sizeof(MSG_PACKET_CS_CHAT_REQ_SECTOR_MOVE));
			}
				break;
			case en_PACKET_CS_CHAT_REQ_MESSAGE:
			{
				WORD messageLen;
				recvBuff.Peek(sizeof(WORD) + sizeof(INT64), (BYTE*)&messageLen, sizeof(WORD));

				message = new JBuffer(sizeof(MSG_PACKET_CS_CHAT_REQ_MESSAGE) + messageLen);
				recvBuff.Dequeue(message->GetDequeueBufferPtr(), sizeof(MSG_PACKET_CS_CHAT_REQ_MESSAGE) + messageLen);
				message->DirectMoveEnqueueOffset(sizeof(MSG_PACKET_CS_CHAT_REQ_MESSAGE) + messageLen);
			}
				break;
			case en_PACKET_CS_CHAT_REQ_HEARTBEAT:
				break;
			default:
				DebugBreak();
				break;
			}
			

			if (message != NULL) {
				// .. 세션 별 메시지 큐 삽입
				AcquireSRWLockShared(&m_SessionMessageqMapSrwLock);
				std::queue<JBuffer*>& sessinMsgQ = m_SessionMessageQueueMap[sessionID];
				sessinMsgQ.push(message);
				ReleaseSRWLockShared(&m_SessionMessageqMapSrwLock);
				recvInfo.recvMsgCnt++;
			}
		}
	}

	if (recvInfo.recvMsgCnt > 0) {
		HANDLE recvEvent = (HANDLE)TlsGetValue(m_RecvEventTlsIndex);
		std::queue<stRecvInfo>& recvInfoQ = m_ThreadEventRecvqMap[recvEvent];
		recvInfoQ.push(recvInfo);
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

	Send_RES_LOGIN(sessionID, true, accountInfo.AccountNo);
}

void ChattingServer::Send_RES_LOGIN(UINT64 sessionID, BYTE STATUS, INT64 AccountNo)
{
	// Unicast Reply
	JBuffer* sendPacket = m_SerialBuffPoolMgr.GetTlsMemPool().AllocMem();
	m_SerialBuffPoolMgr.GetTlsMemPool().IncrementRefCnt(sendPacket);

	(*sendPacket) << (WORD)en_PACKET_CS_CHAT_RES_LOGIN << STATUS << AccountNo;
	SendPacket(sessionID, sendPacket);

	m_SerialBuffPoolMgr.GetTlsMemPool().FreeMem(sendPacket);
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
	JBuffer* sendPacket = m_SerialBuffPoolMgr.GetTlsMemPool().AllocMem();
	m_SerialBuffPoolMgr.GetTlsMemPool().IncrementRefCnt(sendPacket);

	(*sendPacket) << (WORD)en_PACKET_CS_CHAT_RES_SECTOR_MOVE << AccountNo << SectorX << SectorY;
	SendPacket(sessionID, sendPacket);

	m_SerialBuffPoolMgr.GetTlsMemPool().FreeMem(sendPacket);
}

void ChattingServer::Proc_REQ_MESSAGE(UINT64 sessionID, MSG_PACKET_CS_CHAT_REQ_MESSAGE& body)
{
	TlsMemPool<JBuffer>& tlsMemPool = m_SerialBuffPoolMgr.GetTlsMemPool();
	JBuffer* sendPacket = tlsMemPool.AllocMem();
	tlsMemPool.IncrementRefCnt(sendPacket);

	stAccoutInfo& accountInfo = m_SessionIdAccountMap[sessionID];

	(*sendPacket) << (WORD)en_PACKET_CS_CHAT_RES_MESSAGE;
	(*sendPacket) << accountInfo.AccountNo;
	(*sendPacket) << accountInfo.ID;
	(*sendPacket) << accountInfo.Nickname;
	(*sendPacket) << body.MessageLen;
	
	//sendPacket->Enqueue((BYTE*)body.Message, body.MessageLen);
	DebugBreak();

	for (WORD y = accountInfo.Y - 1; y <= accountInfo.Y + 1; y++) {
		for (WORD x = accountInfo.X - 1; x <= accountInfo.X + 1; x++) {
			if (y < 0 || y > dfSECTOR_Y_MAX || x < 0 || x > dfSECTOR_X_MAX) {
				continue;
			}

			std::set<UINT64>& sector = m_SectorMap[y][x];
			for (auto iter = sector.begin(); iter != sector.end(); iter++) {
				tlsMemPool.IncrementRefCnt(sendPacket);
				SendPacket(*iter, sendPacket);
			}
		}
	}

	tlsMemPool.FreeMem(sendPacket);
}

void ChattingServer::Proc_REQ_HEARTBEAT()
{
}
