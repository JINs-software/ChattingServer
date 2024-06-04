#pragma once

#define CHAT_SERV_IP_ADDR_STR						"127.0.0.1"
#define CHAT_SERV_PORT								6000
#define CHAT_SERV_LIMIT_ACCEPTANCE					10000

#define CHAT_TLS_MEM_POOL_DEFAULT_UNIT_CNT			5000
#define CHAT_TLS_MEM_POOL_DEFAULT_UNIT_CAPACITY		1000

#define CHAT_SERV_SESSION_SEND_BUFF_SIZE			30000
#define CHAT_SERV_SESSION_RECV_BUFF_SIZE			30000

#define IOCP_WORKER_THREAD_CNT						4

#define dfSECTOR_X_MAX								50
#define dfSECTOR_Y_MAX								50

////////////////////////////////////////////////////////
// �α�
////////////////////////////////////////////////////////
//#define PLAYER_CREATE_RELEASE_LOG

////////////////////////////////////////////////////////
// Processing(Update) Thread Mode
// 1. IOCP Worker Thread -> Session Msg Queue -> Processing Thread
//						 -> RecvInfo Queue    ->
//						 ======== Event =======>

// 2. IOCP Worker Thread -> Session Msg Queue -> Processing Thread
//												 (Polling)
////////////////////////////////////////////////////////
//#define dfPROCESSING_MODE_SESSIONQ_RECV_INFO_EVENT
#define dfPROCESSING_MODE_SESSIONQ_POLLING
//#define	dfPROCESSING_MODE_

////////////////////////////////////////////////////////
// Monitoring Server Connect
////////////////////////////////////////////////////////
#define CONNECT_MOINTORING_SERVER
#if defined(CONNECT_MOINTORING_SERVER)
#define CHAT_SERVER_MEMORY_USAGE_QUERY "CHATING_SERVER_MEMORY_USAGE"

enum en_MONT_PACKET_TYPE
{

	//------------------------------------------------------
	// Monitor Server Protocol
	//------------------------------------------------------


	////////////////////////////////////////////////////////
	//
	//   MonitorServer & MoniterTool Protocol / ������ ���� ����.
	//
	////////////////////////////////////////////////////////

	//------------------------------------------------------
	// Monitor Server  Protocol
	//------------------------------------------------------
	en_PACKET_SS_MONITOR = 20000,
	//------------------------------------------------------
	// Server -> Monitor Protocol
	//------------------------------------------------------
	//------------------------------------------------------------
	// LoginServer, GameServer , ChatServer  �� ����͸� ������ �α��� ��
	//
	// 
	//	{
	//		WORD	Type
	//
	//		int		ServerNo		//  �� �������� ���� ��ȣ�� �ο��Ͽ� ���
	//	}
	//
	//------------------------------------------------------------
	en_PACKET_SS_MONITOR_LOGIN,

	//------------------------------------------------------------
	// ������ ����͸������� ������ ����
	// �� ������ �ڽ��� ����͸����� ��ġ�� 1�ʸ��� ����͸� ������ ����.
	//
	// ������ �ٿ� �� ��Ÿ ������ ����͸� �����Ͱ� ���޵��� ���ҋ��� ����Ͽ� TimeStamp �� �����Ѵ�.
	// �̴� ����͸� Ŭ���̾�Ʈ���� ���,�� ����Ѵ�.
	// 
	//	{
	//		WORD	Type
	//
	//		BYTE	DataType				// ����͸� ������ Type �ϴ� Define ��.
	//		int		DataValue				// �ش� ������ ��ġ.
	//		int		TimeStamp				// �ش� �����͸� ���� �ð� TIMESTAMP  (time() �Լ�)
	//										// ���� time �Լ��� time_t Ÿ�Ժ����̳� 64bit �� ���񽺷����
	//										// int �� ĳ�����Ͽ� ����. �׷��� 2038�� ������ ��밡��
	//	}
	//
	//------------------------------------------------------------
	en_PACKET_SS_MONITOR_DATA_UPDATE
};
enum en_PACKET_SS_MONITOR_DATA_UPDATE {
	dfMONITOR_DATA_TYPE_CHAT_SERVER_RUN = 30,		// ä�ü��� ChatServer ���� ���� ON / OFF
	dfMONITOR_DATA_TYPE_CHAT_SERVER_CPU = 31,		// ä�ü��� ChatServer CPU ����
	dfMONITOR_DATA_TYPE_CHAT_SERVER_MEM = 32,		// ä�ü��� ChatServer �޸� ��� MByte
	dfMONITOR_DATA_TYPE_CHAT_SESSION = 33,			// ä�ü��� ���� �� (���ؼ� ��)
	dfMONITOR_DATA_TYPE_CHAT_PLAYER = 34,			// ä�ü��� �������� ����� �� (���� ������)
	dfMONITOR_DATA_TYPE_CHAT_UPDATE_TPS = 35,		// ä�ü��� UPDATE ������ �ʴ� �ʸ� Ƚ��
	dfMONITOR_DATA_TYPE_CHAT_PACKET_POOL = 36,		// ä�ü��� ��ŶǮ ��뷮
	dfMONITOR_DATA_TYPE_CHAT_UPDATEMSG_POOL = 37,	// ä�ü��� UPDATE MSG Ǯ ��뷮
};
enum en_SERVER_TYPE {
	dfSERVER_LOGIN_SERVER = 0,
	dfSERVER_ECHO_GAME_SERVER,
	dfSERVER_CHAT_SERVER,
	dfSERVER_SYSTEM
};
#endif