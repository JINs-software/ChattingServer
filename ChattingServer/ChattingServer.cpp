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
	// OnClientJoin 반환 전 해당 세션에 대한 OnRecv가 호출되지 않음을 보장.
	// 세션 별 메시지 큐 관리 자료구조에 새로운 세션 별 메시지 큐를 삽입한다.

	// (1) 세션 메시지 큐 생성
	// (2) 세션 큐 관리 자료구조에 삽입
	//	AcquireSRWLockExclusive(..)
	//  자료구조 삽입
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

	// 1. 스레드 별 메시지 링버퍼에 Enqueue
	// (링버퍼를 사용하는 이유는 Reader와 Writer가 1:1 구조이기 때문에)
	// 2. 링버퍼 Enqueue 후에는 Enq 이벤트를 시그널 상태로 변경
	// 3. 컨텐츠 코드에선 스레드 별 Enq 이벤트 객체를 들고 있다. 이 이벤트에 대해 Wait을 한다. 
	// 

	// 세션 별 메시지 큐에 삽입
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

		// 내부 버퍼 복사..?
		//JBuffer msg()
		JBuffer jbuff;

		sessionMsgQ.push(jbuff);
		recvPacketSize++;
	}

	// m_ThreadEventRecvqMap 삽입
	std::queue<stRecvInfo>& recvInfoQ = m_ThreadEventRecvqMap[recvEvent];
	recvInfoQ.push({ sessionID, recvPacketSize });
	
	// ProcessThread에 알림!
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

	// recvSize 만큼 pop!
}
