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
// ·Î±×
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