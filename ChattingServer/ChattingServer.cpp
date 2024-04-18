#include "ChattingServer.h"
#include "CommonProtocol.h"

void ChattingServer::Init() 
{
	m_RecvEventTlsIndex = TlsAlloc();
}
void ChattingServer::OnWorkerTrehadCreate(HANDLE thHnd)
{
	HANDLE recvEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	UINT8 eventIdx = m_WorkerThreadCnt++;
	m_WorkerThreadRecvEvents[eventIdx] = recvEvent;
	thEventIndexMap.insert({ thHnd, recvEvent });

}

bool ChattingServer::OnConnectionRequest()
{
	return false;
}

void ChattingServer::OnClientJoin(uint64 sessionID)
{
	// OnClientJoin ��ȯ �� �ش� ���ǿ� ���� OnRecv�� ȣ����� ������ ����.
	// ���� �� �޽��� ť ���� �ڷᱸ���� ���ο� ���� �� �޽��� ť�� �����Ѵ�.

	// (1) ���� �޽��� ť ����
	// (2) ���� ť ���� �ڷᱸ���� ����
	//	AcquireSRWLockExclusive(..)
	//  �ڷᱸ�� ����
	//  ReleaseSRWLockExclusive(..)
}

void ChattingServer::OnClientLeave(uint64 sessionID)
{
}

void ChattingServer::OnRecv(uint64 sessionID, JBuffer& recvBuff)
{
	if (TlsGetValue(m_RecvEventTlsIndex) == NULL) {
		HANDLE thEvent = thEventIndexMap[GetCurrentThread()];
		TlsSetValue(m_RecvEventTlsIndex, thEvent);
	}
	HANDLE recvEvent = (HANDLE)TlsGetValue(m_RecvEventTlsIndex);

	// 1. ������ �� �޽��� �����ۿ� Enqueue
	// (�����۸� ����ϴ� ������ Reader�� Writer�� 1:1 �����̱� ������)
	// 2. ������ Enqueue �Ŀ��� Enq �̺�Ʈ�� �ñ׳� ���·� ����
	// 3. ������ �ڵ忡�� ������ �� Enq �̺�Ʈ ��ü�� ��� �ִ�. �� �̺�Ʈ�� ���� Wait�� �Ѵ�. 
	// 

	// ���� �� �޽��� ť�� ����
	AcquireSRWLockShared(&m_SessionMessageqMapSrwLock);
	std::queue<JBuffer>& sessionMsgQ = m_SessionMessageqMap[sessionID];
	ReleaseSRWLockShared(&m_SessionMessageqMapSrwLock);
	size_t recvPacketSize = 0;
	while (true) {
		if (recvBuff.GetUseSize() < sizeof(MSG_HEADER)) {
			break;
		}
		MSG_HEADER msgHdr;
		recvBuff.Peek<MSG_HEADER>(&msgHdr);

		if (recvBuff.GetUseSize() < sizeof(MSG_HEADER) + msgHdr.msgLength) {
			break;
		}

		// ���� ���� ����..?
		//JBuffer msg()
		JBuffer jbuff;

		sessionMsgQ.push(jbuff);
		recvPacketSize++;
	}

	// m_ThreadEventRecvqMap ����
	std::queue<stRecvInfo>& recvInfoQ = m_ThreadEventRecvqMap[recvEvent];
	recvInfoQ.push({ sessionID, recvPacketSize });
	
	// ProcessThread�� �˸�!
	SetEvent(recvEvent);
}

void ChattingServer::OnError()
{
}

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
			size_t recvSize = recvInfo.recvPacketSize;
			recvQ.pop();

			server->ForwardChattingMessage(sessionID, recvSize);		
		}
		else {
			// WaitForMultipleObjects Error..
			DebugBreak();
		}

	}
}

void ChattingServer::ForwardChattingMessage(UINT64 sessionID, size_t recvSize)
{
	AcquireSRWLockShared(&m_SessionMessageqMapSrwLock);
	std::queue<JBuffer>& sessionMsgQ = m_SessionMessageqMap[sessionID];
	ReleaseSRWLockShared(&m_SessionMessageqMapSrwLock);

	// recvSize ��ŭ pop!
}
