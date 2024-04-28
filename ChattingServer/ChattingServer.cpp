#include "ChattingServer.h"

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
	m_ProcessThreadHnd = (HANDLE)_beginthreadex(NULL, 0, ProcessThreadFunc, this, 0, NULL);
}

void ChattingServer::OnWorkerThreadStart() {
	if (TlsGetValue(m_RecvEventTlsIndex) == NULL) {
		DWORD thID = GetThreadId(GetCurrentThread());
		HANDLE thEventHnd = thEventIndexMap[thID];
		TlsSetValue(m_RecvEventTlsIndex, thEventHnd);

		m_ThreadEventRecvqMap.insert({ thEventHnd, std::queue<stRecvInfo>()});
		// �ӽ� ����ȭ ��ü
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
	return true;
}

void ChattingServer::OnClientJoin(uint64 sessionID)
{
	//std::cout << "[OnClientJoin] sessionID: " << sessionID << std::endl;
	{
		std::lock_guard<std::mutex> lockGuard(m_LoginWaitSessionsMtx);
		m_LoginWaitSessions.insert(sessionID);
	}

	// LOGIN_REQ ó�� �Լ��κ��� �̵�
	AcquireSRWLockExclusive(&m_SessionMessageqMapSrwLock);
	m_SessionMessageQueueMap.insert({ sessionID, std::queue<JBuffer*>()});
	// �ӽ� ����ȭ ��ü
	CRITICAL_SECTION* lockPtr = new CRITICAL_SECTION;
	InitializeCriticalSection(lockPtr);
	m_SessionMessageQueueLockMap.insert({ sessionID, lockPtr });
	ReleaseSRWLockExclusive(&m_SessionMessageqMapSrwLock);
}

void ChattingServer::OnClientLeave(uint64 sessionID)
{
}

void ChattingServer::OnRecv(uint64 sessionID, JBuffer& recvBuff)
{	
	HANDLE thHnd = GetCurrentThread();

	// [�޽��� ����]	
	// (�ӽ�) ������ �ܿ��� Code ���� ��/���ڵ����� ����
	while (recvBuff.GetUseSize() >= sizeof(stMSG_HDR)) {

		stRecvInfo recvInfo;
		recvInfo.sessionID = sessionID;
		recvInfo.recvMsgCnt = 0;

		stMSG_HDR msgHdr;
		recvBuff.Peek(&msgHdr);

		if (msgHdr.code != dfPACKET_CODE) {
			// �ڵ� ����ġ
			// ���� ���� ����!

			break;
		}
		if (recvBuff.GetUseSize() < sizeof(stMSG_HDR) + msgHdr.len) {
			// �޽��� �̿ϼ�
			break;
		}
		
		//// ����ȭ ���� ����
		//recvBuff >> msgHdr;
		//JBuffer* message = new JBuffer(msgHdr.len);
		//recvBuff.Dequeue(message->GetBeginBufferPtr(), msgHdr.len);
		//message->DirectMoveEnqueueOffset(msgHdr.len);
		//
		//if (!Decode(msgHdr.randKey, msgHdr.len, msgHdr.checkSum, message->GetBeginBufferPtr())) {
		//	// ���ڵ� ����
		//	// ���� ���� ����
		//	DebugBreak();
		//	break;
		//}

		recvBuff >> msgHdr;
		if (!Decode(msgHdr.randKey, msgHdr.len, msgHdr.checkSum, recvBuff.GetDequeueBufferPtr())) {
			DebugBreak();
			// ���� ���� ����?
		}

		UINT dequeueSize = 0;
		while (recvBuff.GetUseSize() > 0) {			/// ????		// [TO DO] msgHdr.len������ ��ť�� �ϴ� ������ ����
			WORD type;
			recvBuff.Peek(&type);

			JBuffer* message = (JBuffer*)m_SerialBuffPoolMgr.GetTlsMemPool().AllocMem();
			message->ClearBuffer();
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
				// .. ���� �� �޽��� ť ����
				AcquireSRWLockShared(&m_SessionMessageqMapSrwLock);
				std::queue<JBuffer*>& sessionMsgQ = m_SessionMessageQueueMap[sessionID];
				CRITICAL_SECTION* lockPtr = m_SessionMessageQueueLockMap[sessionID];		// �ӽ� ����ȭ ��ü
				ReleaseSRWLockShared(&m_SessionMessageqMapSrwLock);							

				EnterCriticalSection(lockPtr);								// �ӽ� ����ȭ ��ü
				sessionMsgQ.push(message);
				LeaveCriticalSection(lockPtr);								// �ӽ� ����ȭ ��ü
				recvInfo.recvMsgCnt++;
			}
			else {
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
			HANDLE recvEvent = (HANDLE)TlsGetValue(m_RecvEventTlsIndex);
			std::queue<stRecvInfo>& recvInfoQ = m_ThreadEventRecvqMap[recvEvent];
			CRITICAL_SECTION* lockPtr =  m_ThreadEventLockMap[recvEvent];		// �ӽ� ����ȭ ��ü
			EnterCriticalSection(lockPtr);							// �ӽ� ����ȭ ��ü
			recvInfoQ.push(recvInfo);
			LeaveCriticalSection(lockPtr);							// �ӽ� ����ȭ ��ü
			SetEvent(recvEvent);
		}
	}

	// �������� Decode�� ���ؼ� ���� �����۰� �� ���� ���� ��Ȳ�� ���ƾ� ��. ���� Encode, Decode�� ���̺귯������ �����ϵ���..
	if (recvBuff.GetUseSize() == 0) {
		recvBuff.ClearBuffer();
	}

	
	//HANDLE recvEvent = (HANDLE)TlsGetValue(m_RecvEventTlsIndex);
	//
	//// 1. ������ �� �޽��� �����ۿ� Enqueue
	//// (�����۸� ����ϴ� ������ Reader�� Writer�� 1:1 �����̱� ������)
	//// 2. ������ Enqueue �Ŀ��� Enq �̺�Ʈ�� �ñ׳� ���·� ����
	//// 3. ������ �ڵ忡�� ������ �� Enq �̺�Ʈ ��ü�� ��� �ִ�. �� �̺�Ʈ�� ���� Wait�� �Ѵ�. 
	//// 
	//
	//// ���� �� �޽��� ť�� ����
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
	//	// ���� ���� ����..?
	//	//JBuffer msg()
	//	JBuffer jbuff;
	//
	//	sessionMsgQ.push(jbuff);
	//	recvPacketSize++;
	//}
	//
	//// m_ThreadEventRecvqMap ����
	//std::queue<stRecvInfo>& recvInfoQ = m_ThreadEventRecvqMap[recvEvent];
	//recvInfoQ.push({ sessionID, recvPacketSize });
	//
	//// ProcessThread�� �˸�!
	//SetEvent(recvEvent);
}

void ChattingServer::OnError()
{
}

UINT __stdcall ChattingServer::ProcessThreadFunc(void* arg)
{
	ChattingServer* server = (ChattingServer*)arg;
#if defined(ALLOC_BY_TLS_MEM_POOL)
	server->m_SerialBuffPoolIdx = server->m_SerialBuffPoolMgr.AllocTlsMemPool();	// �����ڿ��� ������ Default ���� ����
#endif

	while (true) {
		DWORD ret = WaitForMultipleObjects(server->m_WorkerThreadCnt, server->m_WorkerThreadRecvEvents, FALSE, INFINITE);
		if (WAIT_OBJECT_0 <= ret && ret < WAIT_OBJECT_0 + server->m_WorkerThreadCnt) {
			HANDLE recvEvent = server->m_WorkerThreadRecvEvents[ret];
			std::queue<stRecvInfo>& recvQ = server->m_ThreadEventRecvqMap[recvEvent];
			CRITICAL_SECTION* lockPtr = server->m_ThreadEventLockMap[recvEvent];		// �ӽ� ����ȭ ��ü
			//if (recvQ.size() > 1) {
			//	DebugBreak();
			//}

			size_t recvQueueSize = recvQ.size();	// -> �� ũ�⿡ ���Ե��� ���� ���� ������ ���� �̺�Ʈ���� Ȯ�ε��� ����.
			// OnRecv �ʿ��� (1) ���� ���� ��ť, (2) SetEvent �� �̱⿡...
			for (size_t i = 0; i < recvQueueSize; i++) {
				EnterCriticalSection(lockPtr);							// �ӽ� ����ȭ ��ü
				stRecvInfo& recvInfo = recvQ.front();
				UINT64 sessionID = recvInfo.sessionID;
				size_t recvSize = recvInfo.recvMsgCnt;
				recvQ.pop();
				LeaveCriticalSection(lockPtr);							// �ӽ� ����ȭ ��ü	

				server->ProcessMessage(sessionID, recvSize);
			}
		}
		else {
			// WaitForMultipleObjects Error..
			DebugBreak();
		}

	}
}

void ChattingServer::ProcessMessage(UINT64 sessionID, size_t msgCnt)
{
	// .. ���� �� �޽��� ť ����
	AcquireSRWLockShared(&m_SessionMessageqMapSrwLock);
	std::queue<JBuffer*>& sessionMsgQ = m_SessionMessageQueueMap[sessionID];
	CRITICAL_SECTION* lockPtr = m_SessionMessageQueueLockMap[sessionID];	// �ӽ� ����ȭ ��ü
	ReleaseSRWLockShared(&m_SessionMessageqMapSrwLock);
	if (sessionMsgQ.empty()) {
		DebugBreak();
	}
	for (size_t i = 0; i < msgCnt; i++) {
		EnterCriticalSection(lockPtr);						// �ӽ� ����ȭ ��ü
		JBuffer* msg = sessionMsgQ.front();
		sessionMsgQ.pop();
		LeaveCriticalSection(lockPtr);						// �ӽ� ����ȭ ��ü

		WORD type;
		msg->Peek(&type);
		switch (type)
		{
		case en_PACKET_CS_CHAT_REQ_LOGIN:
		{
			if (msg->GetUseSize() < sizeof(MSG_PACKET_CS_CHAT_REQ_LOGIN)) {
				DebugBreak();
			}
			MSG_PACKET_CS_CHAT_REQ_LOGIN loginReqMsg;
			(*msg) >> loginReqMsg;	// ���� ���� ���� ���۸� �״�� ĳ�����ϴ� �����??
			Proc_REQ_LOGIN(sessionID, loginReqMsg);
		}
		break;
		case en_PACKET_CS_CHAT_REQ_SECTOR_MOVE:
		{
			if (msg->GetUseSize() < sizeof(MSG_PACKET_CS_CHAT_REQ_SECTOR_MOVE)) {
				DebugBreak();
			}
			MSG_PACKET_CS_CHAT_REQ_SECTOR_MOVE moveReqMsg;
			(*msg) >> moveReqMsg;	// ���� ���� ���� ���۸� �״�� ĳ�����ϴ� �����??
			Proc_REQ_SECTOR_MOVE(sessionID, moveReqMsg);
		}
		break;
		case en_PACKET_CS_CHAT_REQ_MESSAGE:
		{
			if (msg->GetUseSize() < sizeof(MSG_PACKET_CS_CHAT_REQ_MESSAGE)) {
				DebugBreak();
			}
			MSG_PACKET_CS_CHAT_REQ_MESSAGE chatReqMsg;
			(*msg) >> chatReqMsg;	// ���� ���� ���� ���۸� �״�� ĳ�����ϴ� �����??
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

		m_SerialBuffPoolMgr.GetTlsMemPool().FreeMem(msg);
	}
}

//# ���� ������ ����Ʈ ����  D1 D2 D3 D4
//----------------------------------------------------------------------------------------------------------
//| D1 | D2 | D3 | D4 |
//----------------------------------------------------------------------------------------------------------
//D1 ^ (RK + 1) = P1 | D2 ^ (P1 + RK + 2) = P2 | D3 ^ (P2 + RK + 3) = P3 | D4 ^ (P3 + RK + 4) = P4 |
//P1 ^ (K + 1) = E1 | P2 ^ (E1 + K + 2) = E2 | P3 ^ (E2 + K + 3) = E3 | P4 ^ (E3 + K + 4) = E4 |
//
//# ��ȣ ������ ����Ʈ ����  E1 E2 E3 E4
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

	// checksum ����
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
	//std::cout << "[Proc_REQ_LOGIN] sessionID: " << sessionID << ", accountNo: " << body.AccountNo << std::endl;
	// ���� ���� ���� -> LoginWait �� ��� �� �ش� �޽��� ó���� �Ŀ� ó������ ������� ���� �� ����
	// �α��� ��� �¿� �ش� ������ �����ϸ� �����ϴ� ������ ����
	//std::cout << "[LOGIN] accountNo: " << body.AccountNo << std::endl;
	{
		std::lock_guard<std::mutex> lockGuard(m_LoginWaitSessionsMtx);
		if (m_LoginWaitSessions.find(sessionID) != m_LoginWaitSessions.end()) {
			m_LoginWaitSessions.erase(sessionID);
		}
	}

	//AcquireSRWLockExclusive(&m_SessionMessageqMapSrwLock);
	//m_SessionMessageQueueMap.insert({ sessionID, std::queue<JBuffer*>()});
	//// �ӽ� ����ȭ ��ü
	//CRITICAL_SECTION* lockPtr = new CRITICAL_SECTION;
	//InitializeCriticalSection(lockPtr);
	//m_SessionMessageQueueLockMap.insert({ sessionID, lockPtr });
	//ReleaseSRWLockExclusive(&m_SessionMessageqMapSrwLock);
	// => OnClientJoin���� �۾�??
	// LOGIN_REQ ���� �����ϴ� ������ �޽����� ���� ���� �޽��� ť�� �������� ����..

	stAccoutInfo accountInfo;
	memcpy(&accountInfo, &body.AccountNo, sizeof(stAccoutInfo));
	accountInfo.X = -1;
	m_SessionIdAccountMap.insert({ sessionID, accountInfo});

	Send_RES_LOGIN(sessionID, true, accountInfo.AccountNo);
}

void ChattingServer::Send_RES_LOGIN(UINT64 sessionID, BYTE STATUS, INT64 AccountNo)
{	
	//std::cout << "[Send_RES_LOGIN] sessionID: " << sessionID << ", accountNo: " << AccountNo << std::endl;
	// Unicast Reply
	JBuffer* sendMessage = m_SerialBuffPoolMgr.GetTlsMemPool().AllocMem();
	sendMessage->ClearBuffer();
	//m_SerialBuffPoolMgr.GetTlsMemPool().IncrementRefCnt(sendMessage);

	stMSG_HDR* hdr = sendMessage->DirectReserve<stMSG_HDR>();
	hdr->code = dfPACKET_CODE;
	hdr->len = sizeof(MSG_PACKET_CS_CHAT_RES_LOGIN);
	hdr->randKey = (BYTE)rand();

	(*sendMessage) << (WORD)en_PACKET_CS_CHAT_RES_LOGIN << STATUS << AccountNo;

	Encode(hdr->randKey, hdr->len, hdr->checkSum, sendMessage->GetBufferPtr(sizeof(stMSG_HDR)));

	SendPacket(sessionID, sendMessage);

	//m_SerialBuffPoolMgr.GetTlsMemPool().FreeMem(sendMessage);
}

void ChattingServer::Proc_REQ_SECTOR_MOVE(UINT64 sessionID, MSG_PACKET_CS_CHAT_REQ_SECTOR_MOVE& body)
{
	//std::cout << "[Proc_REQ_SECTOR_MOVE] sessionID: " << sessionID << ", accountNo: " << body.AccountNo << ", X: " << body.SectorX << ", Y: " << body.SectorY << std::endl;

	stAccoutInfo& accountInfo = m_SessionIdAccountMap[sessionID];
	//std::cout << "���� X: " << accountInfo.X << ", Y: " << accountInfo.Y << std::endl;
	if (accountInfo.X >= 0 && accountInfo.X <= dfSECTOR_X_MAX && accountInfo.Y >= 0 && accountInfo.X <= dfSECTOR_Y_MAX) {
		std::set<UINT64>& sector = m_SectorMap[accountInfo.Y][accountInfo.X];
		sector.erase(sessionID);
	}

	if (body.SectorX < 0 || body.SectorX > dfSECTOR_X_MAX || body.SectorY < 0 || body.SectorY > dfSECTOR_Y_MAX) {
		// ���� �ʰ�
		DebugBreak();
	}
	else {
		accountInfo.X = body.SectorX;
		accountInfo.Y = body.SectorY;
		m_SectorMap[accountInfo.Y][accountInfo.X].insert(sessionID);
	}

	//std::cout << "�ű� X: " << accountInfo.X << ", �ű� Y: " << accountInfo.Y << std::endl;

	Send_RES_SECTOR_MOVE(sessionID, accountInfo.AccountNo, accountInfo.X, accountInfo.Y);
}

void ChattingServer::Send_RES_SECTOR_MOVE(UINT64 sessionID, INT64 AccountNo, WORD SectorX, WORD SectorY)
{
	//std::cout << "[Send_RES_SECTOR_MOVE] sessionID: " << sessionID << ", accountNo: " << AccountNo << std::endl;
	// Unicast Reply
	JBuffer* sendMessage = m_SerialBuffPoolMgr.GetTlsMemPool().AllocMem();
	sendMessage->ClearBuffer();
	//m_SerialBuffPoolMgr.GetTlsMemPool().IncrementRefCnt(sendPacket);

	stMSG_HDR* hdr = sendMessage->DirectReserve<stMSG_HDR>();
	hdr->code = dfPACKET_CODE;
	hdr->len = sizeof(MSG_PACKET_CS_CHAT_RES_SECTOR_MOVE);
	hdr->randKey = (BYTE)rand();

	(*sendMessage) << (WORD)en_PACKET_CS_CHAT_RES_SECTOR_MOVE << AccountNo << SectorX << SectorY;

	Encode(hdr->randKey, hdr->len, hdr->checkSum, sendMessage->GetBufferPtr(sizeof(stMSG_HDR)));

	SendPacket(sessionID, sendMessage);

	//m_SerialBuffPoolMgr.GetTlsMemPool().FreeMem(sendMessage);
}

void ChattingServer::Proc_REQ_MESSAGE(UINT64 sessionID, MSG_PACKET_CS_CHAT_REQ_MESSAGE& body, BYTE* message)
{
	//std::cout << "[Proc_REQ_MESSAGE] sessionID: " << sessionID << ", accountNo: " << body.AccountNo << std::endl;

	TlsMemPool<JBuffer>& tlsMemPool = m_SerialBuffPoolMgr.GetTlsMemPool();
	JBuffer* sendMessage = tlsMemPool.AllocMem();
	sendMessage->ClearBuffer();

	//tlsMemPool.IncrementRefCnt(sendMessage);		// ���� ���� �޽���

	stMSG_HDR* hdr = sendMessage->DirectReserve<stMSG_HDR>();
	hdr->code = dfPACKET_CODE;
	hdr->len = sizeof(MSG_PACKET_CS_CHAT_RES_MESSAGE) + body.MessageLen;
	hdr->randKey = (BYTE)rand();

	stAccoutInfo& accountInfo = m_SessionIdAccountMap[sessionID];

	(*sendMessage) << (WORD)en_PACKET_CS_CHAT_RES_MESSAGE;
	(*sendMessage) << accountInfo.AccountNo;
	(*sendMessage) << accountInfo.ID;
	(*sendMessage) << accountInfo.Nickname;
	(*sendMessage) << body.MessageLen;
	sendMessage->Enqueue(message, body.MessageLen);

	Encode(hdr->randKey, hdr->len, hdr->checkSum, sendMessage->GetBufferPtr(sizeof(stMSG_HDR)));
	
	//for (WORD y = accountInfo.Y - 1; y <= accountInfo.Y + 1; y++) {
	//	for (WORD x = accountInfo.X - 1; x <= accountInfo.X + 1; x++) {
	for (int y = accountInfo.Y - 1; y <= accountInfo.Y + 1; y++) {
		for (int x = accountInfo.X - 1; x <= accountInfo.X + 1; x++) {
			if (y < 0 || y > dfSECTOR_Y_MAX || x < 0 || x > dfSECTOR_X_MAX) {
				continue;
			}

			std::set<UINT64>& sector = m_SectorMap[y][x];
			for (auto iter = sector.begin(); iter != sector.end(); iter++) {
				tlsMemPool.IncrementRefCnt(sendMessage);
				//std::cout << "[Proc_REQ_MESSAGE | SendPacekt] sessionID: " << *iter << std::endl;
				SendPacket(*iter, sendMessage);
			}
		}
	}

	tlsMemPool.FreeMem(sendMessage);

	//std::cout << "[Proc_REQ_MESSAGE | DONE ] sessionID: " << sessionID << ", accountNo: " << body.AccountNo << std::endl;
}

void ChattingServer::Proc_REQ_HEARTBEAT()
{
}
