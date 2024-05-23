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
	ChattingServer(uint8 processThreadCnt, const char* serverIP, uint16 serverPort,
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
		m_LimitAcceptance(maxOfConnections),
		m_ProcessThreadCnt(processThreadCnt)
	{
		m_ProcessThreadHnds.resize(m_ProcessThreadCnt);

#if defined(PLAYER_CREATE_RELEASE_LOG)
		m_PlayerLogIdx = -1;
		m_PlayerLog.resize(USHRT_MAX + 1);

		m_DeletedSendPacketIdx = -1;
		m_DeletedSendPacketLog.resize(USHRT_MAX + 1);
#endif
		m_RecvEventTlsIndex = TlsAlloc();
		InitializeSRWLock(&m_SessionMessageqMapSrwLock);

		// SRWLOCK m_SectorSrwLock[dfSECTOR_Y_MAX + 1][dfSECTOR_X_MAX + 1];
		for (WORD i = 0; i <= dfSECTOR_Y_MAX; i++) {
			for (WORD j = 0; j <= dfSECTOR_X_MAX; j++) {
				InitializeSRWLock(&m_SectorSrwLock[i][j]);
			}
		}

		InitializeSRWLock(&m_SessionAccountMapSrwLock);
	}

private:
	virtual bool OnWorkerThreadCreate(HANDLE thHnd) override;
	virtual void OnWorkerThreadCreateDone() override;
	virtual void OnWorkerThreadStart() override;
	virtual bool OnConnectionRequest(/*IP, Port*/) override;
	virtual void OnClientJoin(UINT64 sessionID) override;
	//virtual void OnDeleteSendPacket(uint64 sessionID, JBuffer& sendRingBuffer) override;
	virtual void OnClientLeave(UINT64 sessionID) override;
	virtual void OnRecv(UINT64 sessionID, JBuffer& recvBuff) override;
	virtual void OnError() override;

public:
	virtual void ServerConsoleLog() override {
		std::cout << "[thread recvInfo queue]" << std::endl;
		for (auto recvq : m_ThreadEventRecvqMap) {
			std::cout << "	size: " << recvq.second.size() << std::endl;
		}
		std::cout << "[Login Wait Session] : " << m_LoginWaitSessions.size() << std::endl;
		std::cout << "[Session Msg Queue Cnt] : " << m_SessionMessageQueueMap.size() << std::endl;
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
	DWORD	m_RecvEventTlsIndex;

	// IOCP �۾��� ������ ����
	UINT8	m_WorkerThreadCnt;
	// IOCP �۾��� �������� ���� �̺�Ʈ �迭
	HANDLE	m_WorkerThreadRecvEvents[MAX_WORKER_THREAD_CNT];
	// �۾��� ������ <-> �۾��� ������ ��� ���� �̺�Ʈ �ʱ� ����� ��
	//std::map<HANDLE, HANDLE> thEventIndexMap;
	// �۾��� �����忡�� key ȹ���� ���� ȣ���ϴ� GetCurrentThread �Լ��� �ǻ� �ڵ� ��ȯ
	// ���� key�� ������ �ڵ��� �ƴ� ������ ID�� ����
	std::map<DWORD, HANDLE> thEventIndexMap;

	// �۾��� ������ ���� �̺�Ʈ <-> RecvInfo ť ����
	std::unordered_map<HANDLE, std::queue<stRecvInfo>>	m_ThreadEventRecvqMap;
	// �ӽ� ����ȭ ��ü (���� ��-���� ť�� ����)
	// �浹 ���: ��Ƽ-���μ���(������Ʈ) ������ ��
	std::unordered_map < HANDLE, CRITICAL_SECTION*>		m_ThreadEventLockMap;

	// ���� �� �ڷᱸ��
	std::set<UINT64>	m_LoginWaitSessions;
	// std::mutex ����ȭ ��ü
	// �浹 ���: IOCP �۾��� �������� ���μ���(������Ʈ) ������ ��
	std::mutex			m_LoginWaitSessionsMtx;

	std::unordered_map<UINT64, std::queue<JBuffer*>>		m_SessionMessageQueueMap;
	SRWLOCK	m_SessionMessageqMapSrwLock;
	// �ӽ� ����ȭ ��ü (���� ��-���� ť�� ����)
	// �浹 ���: IOCP �۾��� ������(Enqueue)�� ��Ƽ-���μ���(������Ʈ)(Dequeue) ������ ��
	std::unordered_map<UINT64, CRITICAL_SECTION*>			m_SessionMessageQueueLockMap; // �޽��� ť �� ����ȭ ��ü

	////////////////////////////////////////////
	// Account ���� ����
	////////////////////////////////////////////
	//std::unordered_map<UINT64, stAccoutInfo>				m_SessionIdAccountMap;
	// => stAccoutInfo�� ���� ���۷��� ī���� �ʿ�
	std::unordered_map<UINT64, std::shared_ptr<stAccoutInfo>>	m_SessionIdAccountMap;
	
	
	// SRWLOCK ����ȭ ��ü
	// �浹 ���: ��Ƽ-���μ���(������Ʈ) ������ ��
	// AcquireSRWLockExclusive:
	//	- REQ_LOGIN ��Ŷ ó�� -> ����(account) ���� �� ����
	//	- SESSION_RELEASE ó�� -> ���� erase
	// AcquireSRWLockShared:
	//	- REQ_SECTOR_MOVE ó��
	//	- REQ_MESSAGE ��Ŷ ó��
	//	- SESSION_RELEASE ó��(���� ���� ����)
	SRWLOCK m_SessionAccountMapSrwLock;

	////////////////////////////////////////////
	// Sector ����
	////////////////////////////////////////////
	std::set<UINT64> m_SectorMap[dfSECTOR_Y_MAX+1][dfSECTOR_X_MAX+1];
	// SRWLOCK ����ȭ ��ü
	// �浹 ���: ��Ƽ-���μ���(������Ʈ) ������ ��
	// AcquireSRWLockExclusive:
	//	- REQ_SECTOR_MOVE ó��
	//	- SESSION_RELEASE ó��
	// AcquireSRWLockShared:
	//	- REQ_MESSAGE ��Ŷ ó��
	SRWLOCK m_SectorSrwLock[dfSECTOR_Y_MAX + 1][dfSECTOR_X_MAX + 1];

	////////////////////////////////////////////
	// Process(Update) Thread
	////////////////////////////////////////////
	UINT m_ProcessThreadCnt;
	std::vector<HANDLE> m_ProcessThreadHnds;

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
	void Proc_SessionReleaseBeforeLogin(UINT64 sessionID);
};

