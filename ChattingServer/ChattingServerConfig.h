#pragma once

///////////////////////////////////////////////////////////////
// CLanServer
///////////////////////////////////////////////////////////////
#define CHAT_SERV_IP_ADDR_STR				"10.0.0.1"
#define CHAT_SERV_PORT						6000
#define CHAT_SERV_LIMIT_ACCEPTANCE			20000

#define CHAT_TLS_MEM_POOL_DEFAULT_UNIT_CNT			5000
#define CHAT_TLS_MEM_POOL_DEFAULT_UNIT_CAPACITY		1000

#define CHAT_SERV_SESSION_SEND_BUFF_SIZE	30000
#define CHAT_SERV_SESSION_RECV_BUFF_SIZE	30000

#define MAX_WORKER_THREAD_CNT				32
#define IOCP_WORKER_THREAD_CNT				4


///////////////////////////////////////////////////////////////
// ChattingServer
///////////////////////////////////////////////////////////////
//#define SINGLE_UPDATE_THREAD
#if !defined(SINGLE_UPDATE_THREAD)
#define PROCESS_THREAD_CNT					4
#endif

// AccountInfo 메모리 풀 설정
// (size_t defaultMemPoolUnitCnt, size_t defaultMemPoolCapcity, bool refCntMemPool = false, bool placementNewMemPool = false);
#define DEFAULT_ACCOUNT_INFO_MEMPOOL_CNT		100
#define DEFAULT_ACCOUNT_INFO_MEMPOOL_CAPACITY	
