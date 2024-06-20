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

#if defined(TOKEN_AUTH_TO_REDIS_MODE)
namespace RedisCpp {
	class CRedisConn;
}
#endif

#if defined(CONNECT_MOINTORING_SERVER)
class ChattingServer : public CLanClient
#else
class ChattingServer : public CLanServer
#endif
{
public:
	/////////////////////////////////////////////////////////////////////////////////////
	// ChattingServer ������
	/////////////////////////////////////////////////////////////////////////////////////
#if defined(ALLOC_BY_TLS_MEM_POOL)
	ChattingServer(const char* serverIP, uint16 serverPort,
		DWORD numOfIocpConcurrentThrd, uint16 numOfWorkerThreads, uint16 maxOfConnections,
		size_t tlsMemPoolDefaultUnitCnt = 0, size_t tlsMemPoolDefaultCapacity = 0,
		UINT serialBufferSize = CHAT_SERIAL_BUFFER_SIZE,
#if defined(LOCKFREE_SEND_QUEUE)
		uint32 sessionRecvBuffSize = CHAT_SERV_SESSION_RECV_BUFF_SIZE,
#else
		uint32 sessionSendBuffSize = CHAT_SERV_SESSION_SEND_BUFF_SIZE, uint32 sessionRecvBuffSize = CHAT_SERV_SESSION_RECV_BUFF_SIZE,
#endif
		bool beNagle = true
	) 
#if defined(CONNECT_MOINTORING_SERVER)
		: CLanClient(serverIP, serverPort, numOfIocpConcurrentThrd, numOfWorkerThreads, maxOfConnections, 
			tlsMemPoolDefaultUnitCnt, tlsMemPoolDefaultCapacity, true, false,
			serialBufferSize,
#if defined(LOCKFREE_SEND_QUEUE)
			sessionRecvBuffSize
#else
			sessionSendBuffSize, sessionRecvBuffSize
#endif
		),
#else
		: CLanServer(serverIP, serverPort, numOfIocpConcurrentThrd, numOfWorkerThreads, maxOfConnections, 
			tlsMemPoolDefaultUnitCnt, tlsMemPoolDefaultCapacity, true, false,
			serialBufferSize,
#if defined(LOCKFREE_SEND_QUEUE)
			sessionRecvBuffSize
#else
			sessionSendBuffSize, sessionRecvBuffSize
#endif
		),
#endif
		m_StopFlag(false),
		m_ServerStart(false),
		m_NumOfIOCPWorkerThreadConf(numOfWorkerThreads),
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
	}
	/////////////////////////////////////////////////////////////////////////////////////
	// ChattingServer �Ҹ���, Stop ȣ�� ��ȣ�� �� Stop �Լ� ȣ��
	/////////////////////////////////////////////////////////////////////////////////////
	~ChattingServer() {
		if (!m_StopFlag) {
			Stop();
		}
	}

public:
	/////////////////////////////////////////////////////////////////////////////////////
	// ChattingServer::Start
	// - ���� ��ü Ǯ ����, CHAT_SERV_LIMIT_ACCEPTANCE: �ִ� ���뷮
	// - ����͸� ī���� ����
	/////////////////////////////////////////////////////////////////////////////////////
	bool Start();
	/////////////////////////////////////////////////////////////////////////////////////
	// ChattingServer::Stop
	// - ���� ��ü Ǯ ����, CHAT_SERV_LIMIT_ACCEPTANCE: �ִ� ���뷮
	// - ����͸� ī���� ����
	/////////////////////////////////////////////////////////////////////////////////////
	void Stop();

private:
	virtual bool OnWorkerThreadCreate(HANDLE thHnd) override;
	virtual void OnWorkerThreadCreateDone() override;	// -> ���μ���(������Ʈ) ������ ����
														// -> ����͸� ���� �� ī���� ������ ����

	virtual void OnWorkerThreadStart() override;
	virtual bool OnConnectionRequest(/*IP, Port*/) override;
	virtual void OnClientJoin(UINT64 sessionID) override;	// -> ���� ���� �޽��� ������Ʈ �޽��� ť�� ť��
	virtual void OnClientLeave(UINT64 sessionID) override;	// -> ���� ���� �޽��� ������Ʈ �޽��� ť�� ť�� 
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
		std::cout << "[Login] Login Wait Session Cnt  : " << m_LoginWaitSessions.size() << std::endl;
		std::cout << "[Login] Login Session Cnt       : " << m_SessionIdAccountMap.size() << std::endl;
		std::cout << "[Account] Allocated Account Cnt : " << m_AccountPool->GetAllocatedObjectCnt() << std::endl;
	}


private:
	bool	m_NumOfIOCPWorkerThreadConf;
	bool	m_StopFlag;
	bool	m_ServerStart;
	size_t	m_LimitAcceptance;
	UINT8	m_WorkerThreadCnt;

#if defined(dfLOCKFREE_QUEUE_SYNCHRONIZATION)
#if defined(dfPROCESSING_MODE_THREAD_RECV_INFO_QUEUE_POLLING)
	////////////////////////////////////////////////////////////////////////////////////
	// 2. ThreadRecvInfoQueue(with Lock) + SessionMessageQueue(with Lock) + UpdateThread Polling
	////////////////////////////////////////////////////////////////////////////////////
	// IOCP �۾��� �������� RecvEvent Queue Info TlsIdx
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
	// IOCP �۾��� �������� ���� �̺�Ʈ �迭
	HANDLE	m_WorkerThreadRecvEvents[IOCP_WORKER_THREAD_CNT];
	// �۾��� ������ <-> �۾��� ������ ��� ���� �̺�Ʈ �ʱ� ����� ��
	//std::map<HANDLE, HANDLE> thEventIndexMap;
	// �۾��� �����忡�� key ȹ���� ���� ȣ���ϴ� GetCurrentThread �Լ��� �ǻ� �ڵ� ��ȯ
	// ���� key�� ������ �ڵ��� �ƴ� ������ ID�� ����
	std::map<DWORD, HANDLE> thEventIndexMap;
	// �۾��� ������ ���� �̺�Ʈ <-> RecvInfo ť ����
	std::unordered_map<HANDLE, std::queue<stRecvInfo>>	m_ThreadEventRecvqMap;
	// �ӽ� ����ȭ ��ü (���� ��-���� ť�� ����)
	// �浹 ���: ��Ƽ-���μ���(������Ʈ) ������ ��
	std::unordered_map<HANDLE, CRITICAL_SECTION*>		m_ThreadEventLockMap;
#elif defined(dfPROCESSING_MODE_THREAD_RECV_INFO_QUEUE_POLLING)
	////////////////////////////////////////////////////////////////////////////////////
	// 2. ThreadRecvInfoQueue(with Lock) + SessionMessageQueue(with Lock) + UpdateThread Polling
	////////////////////////////////////////////////////////////////////////////////////
	DWORD												m_TlsRecvQueueIdx;					// IOCP �۾��� �������� RecvEvent Queue Info TlsIdx
	DWORD												m_TlsRecvQueueLockIdx;				// IOCP �۾��� �������� RecvEvent Queue Lock TlsIdx
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

	//// ���� �� �ڷᱸ��
	std::set<UINT64>	m_LoginWaitSessions;
	//// std::mutex ����ȭ ��ü
	//// �浹 ���: IOCP �۾��� �������� ���μ���(������Ʈ) ������ ��
	//std::mutex			m_LoginWaitSessionsMtx;
	// => ���� ���� �޽��� ���� -> ������Ʈ �޽��� ť

#if defined(dfLOCKFREE_QUEUE_SYNCHRONIZATION)
#if defined(dfPROCESSING_MODE_THREAD_RECV_INFO_QUEUE_POLLING)
	std::unordered_map<UINT64, LockFreeQueue<JBuffer*>>		m_SessionMessageQueueMap;
	SRWLOCK													m_SessionMessageqMapSrwLock;
#elif defined(dfPROCESSING_MODE_THREAD_SINGLE_JOB_QUEUE_POLLING)
	LockFreeQueue<std::pair<UINT64, JBuffer*>>				m_MessageLockFreeQueue;
#endif
#else
	std::unordered_map<UINT64, std::queue<JBuffer*>>		m_SessionMessageQueueMap;
	// �ӽ� ����ȭ ��ü (���� ��-���� ť�� ����)
	// �浹 ���: IOCP �۾��� ������(Enqueue)�� ��Ƽ-���μ���(������Ʈ)(Dequeue) ������ ��
	SRWLOCK													m_SessionMessageqMapSrwLock;
	std::unordered_map<UINT64, CRITICAL_SECTION*>			m_SessionMessageQueueLockMap; // �޽��� ť �� ����ȭ ��ü
#endif

	AccountObjectPool*										m_AccountPool;
	std::unordered_map<UINT64, stAccoutInfo*>				m_SessionIdAccountMap;
	// SRWLOCK ����ȭ ��ü
	// �浹 ���: ��Ƽ-���μ���(������Ʈ) ������ ��
	// AcquireSRWLockExclusive:
	//	- REQ_LOGIN ��Ŷ ó�� -> ����(account) ���� �� ����
	//	- SESSION_RELEASE ó�� -> ���� erase
	// AcquireSRWLockShared:
	//	- REQ_SECTOR_MOVE ó��
	//	- REQ_MESSAGE ��Ŷ ó��
	//	- SESSION_RELEASE ó��(���� ���� ����)
	//SRWLOCK m_SessionAccountMapSrwLock;

	// Process Thread
	HANDLE m_ProcessThreadHnd;
	//std::set<UINT64> m_SectorMap[dfSECTOR_Y_MAX+1][dfSECTOR_X_MAX+1];
	std::map<UINT64, stAccoutInfo*> m_SectorMap[dfSECTOR_Y_MAX + 1][dfSECTOR_X_MAX + 1];
	// SRWLOCK ����ȭ ��ü
	// �浹 ���: ��Ƽ-���μ���(������Ʈ) ������ ��
	// AcquireSRWLockExclusive:
	//	- REQ_SECTOR_MOVE ó��
	//	- SESSION_RELEASE ó��
	// AcquireSRWLockShared:
	//	- REQ_MESSAGE ��Ŷ ó��
	
	//SRWLOCK m_SectorSrwLock[dfSECTOR_Y_MAX + 1][dfSECTOR_X_MAX + 1];
	// => �̱� ������Ʈ �����忡���� ���ʿ�

#if defined(TOKEN_AUTH_TO_REDIS_MODE)
	//RedisCpp::CRedisConn* m_RedisConn;
	LockFreeQueue<RedisCpp::CRedisConn*>	m_RedisConnPool;
#endif

#if defined(CONNECT_MOINTORING_SERVER)
public:
	// ������Ʈ ������ tps	
	int							m_UpdateThreadTps = 0;
	int							m_UpdateThreadTransaction = 0;

	PerformanceCounter*			m_PerfCounter;
	struct stMontData {
		int dataValue = 0;
		int timeStamp = 0;
	};
	std::map<BYTE, stMontData>	m_MontDataMap;

	HANDLE						m_PerfCountThreadFunc;
	bool						m_ConnMontServerFlag = false;
	bool						m_OnJoinMontServer = false;
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
	
	void Proc_SessionJoin(UINT64 sessionID);
	void Proc_SessionRelease(UINT64 sessionID);

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
};

