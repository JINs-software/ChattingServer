#pragma once

#define CHATSERVER_ASSERT
#define DELAY_TIME_CHECK

#define CHAT_SERV_IP_ADDR_STR						NULL
#define CHAT_SERV_PORT								12130
#define CHAT_SERV_LIMIT_ACCEPTANCE					18000

#define IOCP_WORKER_THREAD_CNT						2

#define MONT_SERVER_PROTOCOL_CODE					109
#define MONT_SERVER_PACKET_KEY						30

#define CHAT_TLS_MEM_POOL_DEFAULT_UNIT_CNT			0
#define CHAT_TLS_MEM_POOL_DEFAULT_UNIT_CAPACITY		1000

#define CHAT_SERIAL_BUFFER_SIZE						500

//#define CHAT_SERV_SESSION_SEND_BUFF_SIZE			10000
#define CHAT_SERV_SESSION_RECV_BUFF_SIZE			1000

#define RECV_BUFFERING_MODE							FALSE

#define dfSECTOR_X_MAX								50
#define dfSECTOR_Y_MAX								50

////////////////////////////////////////////////////////
// 로그
////////////////////////////////////////////////////////
//#define PLAYER_CREATE_RELEASE_LOG

#define dfLOCKFREE_QUEUE_SYNCHRONIZATION
#if defined(dfLOCKFREE_QUEUE_SYNCHRONIZATION)
// 1. IOCP Worker Thread -> Session Msg Queue(LockFree) -> Processing Thread (Polling RecvInfo Queue)
//						 -> RecvInfo Queue(LockFree)    ->	
////////////////////////////////////////////////////////
//#define dfPROCESSING_MODE_THREAD_RECV_INFO_QUEUE_POLLING
// 2. IOCP Worker Threads -> Single LockFree Job Queuue -> Processing Thread (Polling Job Queue)
//
////////////////////////////////////////////////////////
#define dfPROCESSING_MODE_THREAD_SINGLE_JOB_QUEUE_POLLING
#else
////////////////////////////////////////////////////////
// Processing(Update) Thread Mode
// 1. IOCP Worker Thread -> Session Msg Queue(with Lock) -> Processing Thread
//						 -> RecvInfo Queue(with Lock)    ->
//						 ======== Event =======>
//#define dfPROCESSING_MODE_THREAD_RECV_INFO_QUEUE_EVENT

// 2. IOCP Worker Thread -> Session Msg Queue(with Lock) -> Processing Thread (Polling RecvInfo Queue)
//						 -> RecvInfo Queue(with Lock)    ->	
//												 
////////////////////////////////////////////////////////
//#define dfPROCESSING_MODE_THREAD_RECV_INFO_QUEUE_POLLING
#endif

////////////////////////////////////////////////////////
// Monitoring Server Connect
////////////////////////////////////////////////////////
#define TOKEN_AUTH_TO_REDIS_MODE
#if defined(TOKEN_AUTH_TO_REDIS_MODE)
#define TOKEN_AUTH_REDIS_IP							"10.0.2.2"
#define TOKEN_AUTH_REDIS_PORT						6379							
#endif

////////////////////////////////////////////////////////
// Monitoring Server Connect
////////////////////////////////////////////////////////
#define CONNECT_MOINTORING_SERVER
#if defined(CONNECT_MOINTORING_SERVER)

#define MONT_SERVER_IP								"127.0.0.1"
#define MONT_SERVER_PORT							12121

enum en_MONT_PACKET_TYPE
{
	//------------------------------------------------------
	// Monitor Server Protocol
	//------------------------------------------------------


	////////////////////////////////////////////////////////
	//
	//   MonitorServer & MoniterTool Protocol / 응답을 받지 않음.
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
	// LoginServer, GameServer , ChatServer  가 모니터링 서버에 로그인 함
	//
	// 
	//	{
	//		WORD	Type
	//
	//		int		ServerNo		//  각 서버마다 고유 번호를 부여하여 사용
	//	}
	//
	//------------------------------------------------------------
	en_PACKET_SS_MONITOR_LOGIN,

	//------------------------------------------------------------
	// 서버가 모니터링서버로 데이터 전송
	// 각 서버는 자신이 모니터링중인 수치를 1초마다 모니터링 서버로 전송.
	//
	// 서버의 다운 및 기타 이유로 모니터링 데이터가 전달되지 못할떄를 대비하여 TimeStamp 를 전달한다.
	// 이는 모니터링 클라이언트에서 계산,비교 사용한다.
	// 
	//	{
	//		WORD	Type
	//
	//		BYTE	DataType				// 모니터링 데이터 Type 하단 Define 됨.
	//		int		DataValue				// 해당 데이터 수치.
	//		int		TimeStamp				// 해당 데이터를 얻은 시간 TIMESTAMP  (time() 함수)
	//										// 본래 time 함수는 time_t 타입변수이나 64bit 로 낭비스러우니
	//										// int 로 캐스팅하여 전송. 그래서 2038년 까지만 사용가능
	//	}
	//
	//------------------------------------------------------------
	en_PACKET_SS_MONITOR_DATA_UPDATE
};
enum en_PACKET_SS_MONITOR_DATA_UPDATE {
	dfMONITOR_DATA_TYPE_CHAT_SERVER_RUN = 30,		// 채팅서버 ChatServer 실행 여부 ON / OFF
	dfMONITOR_DATA_TYPE_CHAT_SERVER_CPU = 31,		// 채팅서버 ChatServer CPU 사용률
	dfMONITOR_DATA_TYPE_CHAT_SERVER_MEM = 32,		// 채팅서버 ChatServer 메모리 사용 MByte
	dfMONITOR_DATA_TYPE_CHAT_SESSION = 33,			// 채팅서버 세션 수 (컨넥션 수)
	dfMONITOR_DATA_TYPE_CHAT_PLAYER = 34,			// 채팅서버 인증성공 사용자 수 (실제 접속자)
	dfMONITOR_DATA_TYPE_CHAT_UPDATE_TPS = 35,		// 채팅서버 UPDATE 스레드 초당 초리 횟수
	dfMONITOR_DATA_TYPE_CHAT_PACKET_POOL = 36,		// 채팅서버 패킷풀 사용량
	dfMONITOR_DATA_TYPE_CHAT_UPDATEMSG_POOL = 37,	// 채팅서버 UPDATE MSG 풀 사용량
	dfMONITOR_DATA_TYPE_CHAT_UPDATE_WORKER_CPU = 38,	// 채팅서버 UPDATE MSG 풀 사용량
};
enum en_SERVER_TYPE {
	dfSERVER_LOGIN_SERVER = 0,
	dfSERVER_ECHO_GAME_SERVER,
	dfSERVER_CHAT_SERVER,
	dfSERVER_SYSTEM
};
#endif