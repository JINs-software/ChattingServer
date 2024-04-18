#pragma once

#include "CLanServer.h"
#include "ChattingServerConfig.h"
#include "DataStruct.h"

#include <map>
#include <unordered_map>

class ChattingServer : public CLanServer
{
	ChattingServer(const char* serverIP, UINT16 serverPort,
		DWORD numOfIocpConcurrentThrd, UINT16 numOfWorkerThreads,
		UINT16 maxOfConnections, bool beNagle = true,
		UINT32 sessionSendBuffSize = CHAT_SERV_SESSION_SEND_BUFF_SIZE, UINT32 sessionRecvBuffSize = CHAT_SERV_SESSION_RECV_BUFF_SIZE
	) 
		: CLanServer(serverIP, serverPort, numOfIocpConcurrentThrd, numOfWorkerThreads, maxOfConnections), 
		m_LimitAcceptance(maxOfConnections), m_WorkerThreadCnt(0)
	{}

	void Init();

	virtual void OnWorkerTrehadCreate(HANDLE thHnd) override;
	virtual bool OnConnectionRequest(/*IP, Port*/) override;
	virtual void OnClientJoin(UINT64 sessionID) override;
	virtual void OnClientLeave(UINT64 sessionID) override;
	virtual void OnRecv(UINT64 sessionID, JBuffer& recvBuff) override;
	virtual void OnError() override;

	static UINT __stdcall ProcessThreadFunc(void* arg);
	void ForwardChattingMessage(UINT64 sessionID, size_t recvSize);

private:
	size_t	m_LimitAcceptance;
	std::map<HANDLE, HANDLE> thEventIndexMap;	// 스레드 핸들 <-> 스레드 이벤트 핸들 맵핑
	HANDLE	m_WorkerThreadRecvEvents[MAX_WORKER_THREAD_CNT];
	UINT8	m_WorkerThreadCnt;
	DWORD	m_RecvEventTlsIndex;

	std::unordered_map<HANDLE, std::queue<stRecvInfo>>	m_ThreadEventRecvqMap;

	std::unordered_map<UINT64, std::queue<JBuffer>>		m_SessionMessageqMap;
	SRWLOCK	m_SessionMessageqMapSrwLock;
};

