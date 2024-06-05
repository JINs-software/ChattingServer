#pragma once

#include "ChattingServerConfig.h"

#if defined(CONNECT_MOINTORING_SERVER)
#include "CLanClient.h"
#include "PerformanceCounter.h"
#else
#include "CLanServer.h"
#endif
#include "ChattingProtocol.h"

#include "DataStruct.h"

#include <map>
#include <set>
#include <unordered_map>

#include "LockFreeQueue.h"

#if defined(CONNECT_MOINTORING_SERVER)
class ChattingServer : public CLanClient
#else
class ChattingServer : public CLanServer
#endif
{
#if defined(PLAYER_CREATE_RELEASE_LOG)
	struct stPlayerLog {
		bool joinFlag = false;
		bool leaveFlag = false;
		bool recvFlag = false;
		bool procMsgFlag = false;
		uint64 sessionID = 0;
		uint64 sessinIdIndex;
		int64 accountNo;
		en_PACKET_TYPE packetID;
		bool sendSuccess = false;
		uint64 sessionID_dest;

		uint64 uint0;

		void Init() {
			memset(this, 0, sizeof(stPlayerLog));
		}
	};
	USHORT m_PlayerLogIdx;
	std::vector<stPlayerLog> m_PlayerLog;
	stPlayerLog& GetPlayerLog() {
		USHORT playerLogIdx = InterlockedIncrement16((short*)&m_PlayerLogIdx);
		m_PlayerLog[playerLogIdx].Init();

		return m_PlayerLog[playerLogIdx];
	}

	struct stDeletedSendPacket {
		WORD type;
		INT64 accountNo;
	};
	USHORT m_DeletedSendPacketIdx;
	std::vector<stDeletedSendPacket> m_DeletedSendPacketLog;

public:
	void PlayerFileLog();
#endif
public:
#if defined(ALLOC_BY_TLS_MEM_POOL)
	ChattingServer(const char* serverIP, uint16 serverPort,
		DWORD numOfIocpConcurrentThrd, uint16 numOfWorkerThreads, uint16 maxOfConnections,
		size_t tlsMemPoolDefaultUnitCnt = CHAT_TLS_MEM_POOL_DEFAULT_UNIT_CNT, size_t tlsMemPoolDefaultCapacity = CHAT_TLS_MEM_POOL_DEFAULT_UNIT_CAPACITY,
		uint32 sessionSendBuffSize = CHAT_SERV_SESSION_SEND_BUFF_SIZE, uint32 sessionRecvBuffSize = CHAT_SERV_SESSION_RECV_BUFF_SIZE,
		bool beNagle = true
	) 
#if defined(CONNECT_MOINTORING_SERVER)
		: CLanClient(serverIP, serverPort, numOfIocpConcurrentThrd, numOfWorkerThreads, maxOfConnections, true, false),
		//: CLanClient(serverIP, serverPort, 
		//	numOfIocpConcurrentThrd, numOfWorkerThreads, maxOfConnections, 
		//	true, false, tlsMemPoolDefaultUnitCnt, tlsMemPoolDefaultCapacity,
		//	sessionSendBuffSize, sessionRecvBuffSize
		//	), 
#else
		: CLanServer(serverIP, serverPort, numOfIocpConcurrentThrd, numOfWorkerThreads, maxOfConnections, true, false,
			tlsMemPoolDefaultUnitCnt, tlsMemPoolDefaultCapacity,
			sessionSendBuffSize, sessionRecvBuffSize
		),
#endif
		m_WorkerThreadCnt(0),
		m_LimitAcceptance(maxOfConnections)
#else
	ChattingServer(const char* serverIP, uint16 serverPort,
		DWORD numOfIocpConcurrentThrd, uint16 numOfWorkerThreads, uint16 maxOfConnections,
		uint32 sessionSendBuffSize = CHAT_SERV_SESSION_SEND_BUFF_SIZE, uint32 sessionRecvBuffSize = CHAT_SERV_SESSION_RECV_BUFF_SIZE,
		bool beNagle = true
	)
		: CLanServer(serverIP, serverPort, numOfIocpConcurrentThrd, numOfWorkerThreads, maxOfConnections, true, false,
			sessionSendBuffSize, sessionRecvBuffSize
		),
		m_WorkerThreadCnt(0),
		m_LimitAcceptance(maxOfConnections)
#endif
	{
#if defined(PLAYER_CREATE_RELEASE_LOG)
		m_PlayerLogIdx = -1;
		m_PlayerLog.resize(USHRT_MAX + 1);

		m_DeletedSendPacketIdx = -1;
		m_DeletedSendPacketLog.resize(USHRT_MAX + 1);
#endif

#if defined(dfLOCKFREE_QUEUE_SYNCHRONIZATION)
#if defined(dfPROCESSING_MODE_THREAD_RECV_INFO_QUEUE_POLLING)
		m_TlsRecvQueueIdx = TlsAlloc();
		m_TlsRecvQueueVec.resize(IOCP_WORKER_THREAD_CNT);
#endif
#else
#if defined(dfPROCESSING_MODE_THREAD_RECV_INFO_QUEUE_EVENT)
		m_RecvEventTlsIndex = TlsAlloc();
#elif defined(dfPROCESSING_MODE_THREAD_RECV_INFO_QUEUE_POLLING)
		m_TlsRecvQueueIdx = TlsAlloc();
		m_TlsRecvQueueLockIdx = TlsAlloc();
		m_TlsRecvQueueVec.resize(IOCP_WORKER_THREAD_CNT);
#endif
#endif

#if !defined(dfPROCESSING_MODE_THREAD_SINGLE_JOB_QUEUE_POLLING)
		InitializeSRWLock(&m_SessionMessageqMapSrwLock);
#endif

		// SRWLOCK m_SectorSrwLock[dfSECTOR_Y_MAX + 1][dfSECTOR_X_MAX + 1];
		for (WORD i = 0; i < dfSECTOR_Y_MAX + 1; i++) {
			for (WORD j = 0; j < dfSECTOR_X_MAX + 1; j++) {
				InitializeSRWLock(&m_SectorSrwLock[i][j]);
			}
		}

		InitializeSRWLock(&m_SessionAccountMapSrwLock);
	}

public:
	bool Start() {
		if (!CLanClient::Start()) {
			return false;
		}
#if defined(CONNECT_MOINTORING_SERVER)
		if (!ConnectLanServer("127.0.0.1", 10001, 1)) {
			return false;
		}
#endif

		return true;
	}
	void Stop() {
#if defined(CONNECT_MOINTORING_SERVER)
		DisconnectLanServer();
#endif
		CLanClient::Stop();
	}

private:
	virtual bool OnWorkerThreadCreate(HANDLE thHnd) override;
	virtual void OnWorkerThreadCreateDone() override;
	virtual void OnWorkerThreadStart() override;
	virtual bool OnConnectionRequest(/*IP, Port*/) override;
	virtual void OnClientJoin(UINT64 sessionID) override;
	virtual void OnClientLeave(UINT64 sessionID) override;
#if !defined(ON_RECV_BUFFERING)
	virtual void OnRecv(UINT64 sessionID, JBuffer& recvBuff) override;
#else 
	virtual void OnRecv(UINT64 sessionID, std::queue<JBuffer>& bufferedQueue, size_t recvDataLen);
#endif
	virtual void OnError() override;

#if defined(CONNECT_MOINTORING_SERVER)
	virtual void OnEnterJoinServer() override;
	virtual void OnLeaveServer() override;
	virtual void OnRecvFromCLanServer(JBuffer& recvBuff) override;
#endif

public:
	virtual void ServerConsoleLog() override {
#if defined(dfPROCESSING_MODE_THREAD_RECV_INFO_QUEUE_EVENT)
		std::cout << "[thread recvInfo queue]" << std::endl;
		for (auto recvq : m_ThreadEventRecvqMap) {
			std::cout << "	size: " << recvq.second.size() << std::endl;
		}
#endif
		std::cout << "[Login Wait Session] : " << m_LoginWaitSessions.size() << std::endl;
#if defined(dfLOCKFREE_QUEUE_SYNCHRONIZATION) && defined(dfPROCESSING_MODE_THREAD_RECV_INFO_QUEUE_POLLING)
		std::cout << "[Session Msg Queue Cnt] : " << m_SessionMessageQueueMap.size() << std::endl;
#endif
		std::cout << "[Account Map size] : " << m_SessionIdAccountMap.size() << std::endl;
		int sectorCnt = 0;
		for (int y = 0; y <= dfSECTOR_Y_MAX; y++) {
			for (int x = 0; x <= dfSECTOR_X_MAX; x++) {
				sectorCnt += m_SectorMap[y][x].size();
			}
		}
		std::cout << "[Sector Member Cnt] : " << sectorCnt << std::endl;
	}

private:
	size_t	m_LimitAcceptance;
	// IOCP 작업자 스레드 갯수
	UINT8	m_WorkerThreadCnt;

#if defined(dfLOCKFREE_QUEUE_SYNCHRONIZATION)
#if defined(dfPROCESSING_MODE_THREAD_RECV_INFO_QUEUE_POLLING)
	////////////////////////////////////////////////////////////////////////////////////
	// 2. ThreadRecvInfoQueue(with Lock) + SessionMessageQueue(with Lock) + UpdateThread Polling
	////////////////////////////////////////////////////////////////////////////////////
	// IOCP 작업자 스레드의 RecvEvent Queue Info TlsIdx
	DWORD																m_TlsRecvQueueIdx;					
	std::vector<LockFreeQueue<stRecvInfo>*>								m_TlsRecvQueueVec;
	inline int GetUpdateMsgPoolSize() {
		int ret = 0;
		for (int i = 0; i < m_TlsRecvQueueVec.size(); i++) {
			ret += m_TlsRecvQueueVec[i]->GetSize();
		}
		return ret;
	}
#elif defined(dfPROCESSING_MODE_THREAD_SINGLE_JOB_QUEUE_POLLING)
	inline int GetUpdateMsgPoolSize() {
		return m_MessageLockFreeQueue.GetSize();
	}
#endif
#else
#if defined(dfPROCESSING_MODE_THREAD_RECV_INFO_QUEUE_EVENT)
	////////////////////////////////////////////////////////////////////////////////////
	// 1. ThreadRecvInfoQueue(with Lock) + SessionMessageQueue(with Lock) + Event Alert to UpdateThread
	////////////////////////////////////////////////////////////////////////////////////
	DWORD	m_RecvEventTlsIndex;
	// IOCP 작업자 스레드의 수신 이벤트 배열
	HANDLE	m_WorkerThreadRecvEvents[IOCP_WORKER_THREAD_CNT];
	// 작업자 스레드 <-> 작업자 스레드 사용 수신 이벤트 초기 연결용 맵
	//std::map<HANDLE, HANDLE> thEventIndexMap;
	// 작업자 스레드에서 key 획득을 위해 호출하는 GetCurrentThread 함수는 의사 핸들 반환
	// 따라서 key는 스레드 핸들이 아닌 스레드 ID로 변경
	std::map<DWORD, HANDLE> thEventIndexMap;
	// 작업자 스레드 수신 이벤트 <-> RecvInfo 큐 맵핑
	std::unordered_map<HANDLE, std::queue<stRecvInfo>>	m_ThreadEventRecvqMap;
	// 임시 동기화 객체 (추후 락-프리 큐로 변경)
	// 충돌 대상: 멀티-프로세싱(업데이트) 스레드 간
	std::unordered_map<HANDLE, CRITICAL_SECTION*>		m_ThreadEventLockMap;
#elif defined(dfPROCESSING_MODE_THREAD_RECV_INFO_QUEUE_POLLING)
	////////////////////////////////////////////////////////////////////////////////////
	// 2. ThreadRecvInfoQueue(with Lock) + SessionMessageQueue(with Lock) + UpdateThread Polling
	////////////////////////////////////////////////////////////////////////////////////
	DWORD												m_TlsRecvQueueIdx;					// IOCP 작업자 스레드의 RecvEvent Queue Info TlsIdx
	DWORD												m_TlsRecvQueueLockIdx;				// IOCP 작업자 스레드의 RecvEvent Queue Lock TlsIdx
	std::vector<std::pair<std::queue<stRecvInfo>*, CRITICAL_SECTION*>>	m_TlsRecvQueueVec;
	inline int GetUpdateMsgPoolSize() {
		int ret = 0;
		for (int i = 0; i < m_TlsRecvQueueVec.size(); i++) {
			ret += m_TlsRecvQueueVec[i].first->size();
		}
		return ret;
	}
#endif
#endif

	// 세션 별 자료구조
	std::set<UINT64>	m_LoginWaitSessions;
	// std::mutex 동기화 객체
	// 충돌 대상: IOCP 작업자 스레드들과 프로세싱(업데이트) 스레드 간
	std::mutex			m_LoginWaitSessionsMtx;

#if defined(dfLOCKFREE_QUEUE_SYNCHRONIZATION)
#if defined(dfPROCESSING_MODE_THREAD_RECV_INFO_QUEUE_POLLING)
	std::unordered_map<UINT64, LockFreeQueue<JBuffer*>>		m_SessionMessageQueueMap;
	SRWLOCK													m_SessionMessageqMapSrwLock;
#else
	LockFreeQueue<std::pair<UINT64, JBuffer*>>				m_MessageLockFreeQueue;
#endif
#else
	std::unordered_map<UINT64, std::queue<JBuffer*>>		m_SessionMessageQueueMap;
	// 임시 동기화 객체 (추후 락-프리 큐로 변경)
	// 충돌 대상: IOCP 작업자 스레드(Enqueue)와 멀티-프로세싱(업데이트)(Dequeue) 스레드 간
	SRWLOCK													m_SessionMessageqMapSrwLock;
	std::unordered_map<UINT64, CRITICAL_SECTION*>			m_SessionMessageQueueLockMap; // 메시지 큐 별 동기화 객체
#endif

	std::unordered_map<UINT64, stAccoutInfo>				m_SessionIdAccountMap;
	// SRWLOCK 동기화 객체
	// 충돌 대상: 멀티-프로세싱(업데이트) 스레드 간
	// AcquireSRWLockExclusive:
	//	- REQ_LOGIN 패킷 처리 -> 계정(account) 생성 후 삽입
	//	- SESSION_RELEASE 처리 -> 계정 erase
	// AcquireSRWLockShared:
	//	- REQ_SECTOR_MOVE 처리
	//	- REQ_MESSAGE 패킷 처리
	//	- SESSION_RELEASE 처리(계정 정보 참조)
	SRWLOCK m_SessionAccountMapSrwLock;

	// Process Thread
	HANDLE m_ProcessThreadHnd;
	std::set<UINT64> m_SectorMap[dfSECTOR_Y_MAX+1][dfSECTOR_X_MAX+1];
	// SRWLOCK 동기화 객체
	// 충돌 대상: 멀티-프로세싱(업데이트) 스레드 간
	// AcquireSRWLockExclusive:
	//	- REQ_SECTOR_MOVE 처리
	//	- SESSION_RELEASE 처리
	// AcquireSRWLockShared:
	//	- REQ_MESSAGE 패킷 처리
	SRWLOCK m_SectorSrwLock[dfSECTOR_Y_MAX + 1][dfSECTOR_X_MAX + 1];

#if defined(CONNECT_MOINTORING_SERVER)
	// 업데이트 스레드 tps	
	int							m_UpdateThreadTps = 0;
	int							m_UpdateThreadTransaction = 0;

	PerformanceCounter*			m_PerfCounter;
	struct stMontData {
		int dataValue = 0;
		int timeStamp = 0;
	};
	std::map<BYTE, stMontData>	m_MontDataMap;

	HANDLE						m_PerfCountThreadFunc;
	bool						m_PerfCountStop = false;
#endif

private:
	// Process Thread Working Function
	static UINT __stdcall ProcessThreadFunc(void* arg);
#if defined(dfPROCESSING_MODE_THREAD_SINGLE_JOB_QUEUE_POLLING)
	void ProcessMessage(UINT64 sessionID, JBuffer* msg);
#else
	void ProcessMessage(UINT64 sessionID, size_t msgCnt);
#endif

#if defined(CONNECT_MOINTORING_SERVER)
	static UINT __stdcall PerformanceCountFunc(void* arg);
	void SendPerfCountMsg();
#endif

	void Proc_REQ_LOGIN(UINT64 sessionID, MSG_PACKET_CS_CHAT_REQ_LOGIN& body);
	void Send_RES_LOGIN(UINT64 sessionID, BYTE STATUS, INT64 AccountNo);
	void Proc_REQ_SECTOR_MOVE(UINT64 sessionID, MSG_PACKET_CS_CHAT_REQ_SECTOR_MOVE& body);
	void Send_RES_SECTOR_MOVE(UINT64 sessionID, INT64 AccountNo, WORD SectorX, WORD SectorY);
	void Proc_REQ_MESSAGE(UINT64 sessionID, MSG_PACKET_CS_CHAT_REQ_MESSAGE& body, BYTE* message);
	void Proc_REQ_HEARTBEAT();
	
	void Proc_SessionRelease(UINT64 sessionID, bool beforeLogin = false);
};

