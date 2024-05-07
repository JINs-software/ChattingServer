#pragma once

#include "CLanServer.h"
#include "CommonProtocol.h"
#include "ChattingServerConfig.h"
#include "DataStruct.h"

#include <map>
#include <set>
#include <unordered_map>

#define PLAYER_CREATE_RELEASE_LOG

class ChattingServer : public CLanServer
{
#if defined(PLAYER_CREATE_RELEASE_LOG)
	struct stPlayerLog {
		bool joinFlag = false;
		bool leaveFlag = false;
		uint64 sessionID = 0;
		uint64 sessinIdIndex;
		int64 accountNo;
		en_PACKET_TYPE packetID;
	};
	USHORT m_PlayerLogIdx;
	std::vector<stPlayerLog> m_PlayerLog;
#endif
public:
	ChattingServer(const char* serverIP, uint16 serverPort,
		DWORD numOfIocpConcurrentThrd, uint16 numOfWorkerThreads, uint16 maxOfConnections,
		size_t tlsMemPoolDefaultUnitCnt = CHAT_TLS_MEM_POOL_DEFAULT_UNIT_CNT, size_t tlsMemPoolDefaultCapacity = CHAT_TLS_MEM_POOL_DEFAULT_UNIT_CAPACITY,
		uint32 sessionSendBuffSize = CHAT_SERV_SESSION_SEND_BUFF_SIZE, uint32 sessionRecvBuffSize = CHAT_SERV_SESSION_RECV_BUFF_SIZE,
		bool beNagle = true
	) 
		: CLanServer(serverIP, serverPort, numOfIocpConcurrentThrd, numOfWorkerThreads, maxOfConnections, true, false,
			tlsMemPoolDefaultUnitCnt, tlsMemPoolDefaultCapacity,
			sessionSendBuffSize, sessionRecvBuffSize
			), 
		m_WorkerThreadCnt(0),
		m_LimitAcceptance(maxOfConnections)
	{
#if defined(PLAYER_CREATE_RELEASE_LOG)
		m_PlayerLogIdx = -1;
		m_PlayerLog.resize(USHRT_MAX + 1);
#endif
		m_RecvEventTlsIndex = TlsAlloc();
		InitializeSRWLock(&m_SessionMessageqMapSrwLock);

		// SRWLOCK m_SectorSrwLock[dfSECTOR_Y_MAX + 1][dfSECTOR_X_MAX + 1];
		for (WORD i = 0; i < dfSECTOR_Y_MAX + 1; i++) {
			for (WORD j = 0; j < dfSECTOR_X_MAX + 1; j++) {
				InitializeSRWLock(&m_SectorSrwLock[i][j]);
			}
		}

		InitializeSRWLock(&m_SessionAccountMapSrwLock);
	}

	void PlayerFileLog();

private:
	virtual bool OnWorkerThreadCreate(HANDLE thHnd) override;
	virtual void OnWorkerThreadCreateDone() override;
	virtual void OnWorkerThreadStart() override;
	virtual bool OnConnectionRequest(/*IP, Port*/) override;
	virtual void OnClientJoin(UINT64 sessionID) override;
	virtual void OnClientLeave(UINT64 sessionID) override;
	virtual void OnRecv(UINT64 sessionID, JBuffer& recvBuff) override;
	virtual void OnError() override;

private:
	size_t	m_LimitAcceptance;
	DWORD	m_RecvEventTlsIndex;

	// IOCP 작업자 스레드 갯수
	UINT8	m_WorkerThreadCnt;
	// IOCP 작업자 스레드의 수신 이벤트 배열
	HANDLE	m_WorkerThreadRecvEvents[MAX_WORKER_THREAD_CNT];
	// 작업자 스레드 <-> 작업자 스레드 사용 수신 이벤트 초기 연결용 맵
	//std::map<HANDLE, HANDLE> thEventIndexMap;
	// 작업자 스레드에서 key 획득을 위해 호출하는 GetCurrentThread 함수는 의사 핸들 반환
	// 따라서 key는 스레드 핸들이 아닌 스레드 ID로 변경
	std::map<DWORD, HANDLE> thEventIndexMap;

	// 작업자 스레드 수신 이벤트 <-> RecvInfo 큐 맵핑
	std::unordered_map<HANDLE, std::queue<stRecvInfo>>	m_ThreadEventRecvqMap;
	// 임시 동기화 객체 (락-프리 적용 전)
	std::unordered_map < HANDLE, CRITICAL_SECTION*>		m_ThreadEventLockMap;

	// 세션 별 자료구조
	std::set<UINT64>	m_LoginWaitSessions;
	std::mutex			m_LoginWaitSessionsMtx;

	std::unordered_map<UINT64, std::queue<JBuffer*>>		m_SessionMessageQueueMap;
	SRWLOCK	m_SessionMessageqMapSrwLock;
	// 임시 동기화 객체 (락-프리 적용 전)
	std::unordered_map<UINT64, CRITICAL_SECTION*>			m_SessionMessageQueueLockMap; // 메시지 큐 별 동기화 객체

	std::unordered_map<UINT64, stAccoutInfo>				m_SessionIdAccountMap;
	SRWLOCK m_SessionAccountMapSrwLock;

	// Process Thread
	HANDLE m_ProcessThreadHnd;
	std::set<UINT64> m_SectorMap[dfSECTOR_Y_MAX+1][dfSECTOR_X_MAX+1];
	SRWLOCK m_SectorSrwLock[dfSECTOR_Y_MAX + 1][dfSECTOR_X_MAX + 1];

private:
	// Process Thread Working Function
	static UINT __stdcall ProcessThreadFunc(void* arg);
	void ProcessMessage(UINT64 sessionID, size_t msgCnt);

	bool Decode(BYTE randKey, USHORT payloadLen, BYTE checkSum, BYTE* payloads);
	void Encode(BYTE randKey, USHORT payloadLen, BYTE& checkSum, BYTE* payloads);

	void Proc_REQ_LOGIN(UINT64 sessionID, MSG_PACKET_CS_CHAT_REQ_LOGIN& body);
	void Send_RES_LOGIN(UINT64 sessionID, BYTE STATUS, INT64 AccountNo);
	void Proc_REQ_SECTOR_MOVE(UINT64 sessionID, MSG_PACKET_CS_CHAT_REQ_SECTOR_MOVE& body);
	void Send_RES_SECTOR_MOVE(UINT64 sessionID, INT64 AccountNo, WORD SectorX, WORD SectorY);
	void Proc_REQ_MESSAGE(UINT64 sessionID, MSG_PACKET_CS_CHAT_REQ_MESSAGE& body, BYTE* message);
	void Proc_REQ_HEARTBEAT();
	
	void Proc_SessionRelease(UINT64 sessionID);
};

