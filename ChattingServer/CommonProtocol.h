//#ifndef __GODDAMNBUG_ONLINE_PROTOCOL__
//#define __GODDAMNBUG_ONLINE_PROTOCOL__

#include <minwindef.h>

#define dfPACKET_CODE		0x77
#define dfPACKET_KEY		0x32

#define dfSECTOR_X_MAX		50
#define dfSECTOR_Y_MAX		50

enum en_PACKET_TYPE
{
	////////////////////////////////////////////////////////
	//
	//	Client & Server Protocol
	//
	////////////////////////////////////////////////////////

	//------------------------------------------------------
	// Chatting Server
	//------------------------------------------------------
	en_PACKET_CS_CHAT_SERVER			= 0,

	//------------------------------------------------------------
	// 채팅서버 로그인 요청
	//
	//	{
	//		WORD	Type
	//
	//		INT64	AccountNo
	//		WCHAR	ID[20]				// null 포함
	//		WCHAR	Nickname[20]		// null 포함
	//		char	SessionKey[64];		// 인증토큰
	//	}
	//
	//------------------------------------------------------------
	en_PACKET_CS_CHAT_REQ_LOGIN,

	//------------------------------------------------------------
	// 채팅서버 로그인 응답
	//
	//	{
	//		WORD	Type
	//
	//		BYTE	Status				// 0:실패	1:성공
	//		INT64	AccountNo
	//	}
	//
	//------------------------------------------------------------
	en_PACKET_CS_CHAT_RES_LOGIN,

	//------------------------------------------------------------
	// 채팅서버 섹터 이동 요청
	//
	//	{
	//		WORD	Type
	//
	//		INT64	AccountNo
	//		WORD	SectorX
	//		WORD	SectorY
	//	}
	//
	//------------------------------------------------------------
	en_PACKET_CS_CHAT_REQ_SECTOR_MOVE,

	//------------------------------------------------------------
	// 채팅서버 섹터 이동 결과
	//
	//	{
	//		WORD	Type
	//
	//		INT64	AccountNo
	//		WORD	SectorX
	//		WORD	SectorY
	//	}
	//
	//------------------------------------------------------------
	en_PACKET_CS_CHAT_RES_SECTOR_MOVE,

	//------------------------------------------------------------
	// 채팅서버 채팅보내기 요청
	//
	//	{
	//		WORD	Type
	//
	//		INT64	AccountNo
	//		WORD	MessageLen
	//		WCHAR	Message[MessageLen / 2]		// null 미포함
	//	}
	//
	//------------------------------------------------------------
	en_PACKET_CS_CHAT_REQ_MESSAGE,

	//------------------------------------------------------------
	// 채팅서버 채팅보내기 응답  (다른 클라가 보낸 채팅도 이걸로 받음)
	//
	//	{
	//		WORD	Type
	//
	//		INT64	AccountNo
	//		WCHAR	ID[20]						// null 포함
	//		WCHAR	Nickname[20]				// null 포함
	//		
	//		WORD	MessageLen
	//		WCHAR	Message[MessageLen / 2]		// null 미포함
	//	}
	//
	//------------------------------------------------------------
	en_PACKET_CS_CHAT_RES_MESSAGE,

	//------------------------------------------------------------
	// 하트비트
	//
	//	{
	//		WORD		Type
	//	}
	//
	//
	// 클라이언트는 이를 30초마다 보내줌.
	// 서버는 40초 이상동안 메시지 수신이 없는 클라이언트를 강제로 끊어줘야 함.
	//------------------------------------------------------------	
	en_PACKET_CS_CHAT_REQ_HEARTBEAT,

	en_SESSION_RELEASE
};

//#endif

#pragma pack(push, 1)

struct stMSG_HDR {
	BYTE	code;
	USHORT	len;
	BYTE	randKey;
	BYTE	checkSum;
};

struct MSG_PACKET_CS_CHAT_REQ_LOGIN {
	WORD Type;
	INT64 AccountNo;
	WCHAR ID[20];			// null 포함
	WCHAR Nickname[20];		// null 포함
	char sessinKey[64];		// 인증 토큰
};
struct MSG_PACKET_CS_CHAT_RES_LOGIN {
	WORD	Type;
	BYTE	Status;				// 0:실패	1:성공
	INT64	AccountNo;
};
struct MSG_PACKET_CS_CHAT_REQ_SECTOR_MOVE {
	WORD	Type;
	INT64	AccountNo;
	WORD	SectorX;
	WORD	SectorY;
};
struct MSG_PACKET_CS_CHAT_RES_SECTOR_MOVE {
	WORD	Type;
	INT64	AccountNo;
	WORD	SectorX;
	WORD	SectorY;
};
struct MSG_PACKET_CS_CHAT_REQ_MESSAGE {
	WORD	Type;
	INT64	AccountNo;
	WORD	MessageLen;

	//WCHAR	Message[MessageLen / 2];		// null 미포함
	//WCHAR* Message;
};	
struct MSG_PACKET_CS_CHAT_RES_MESSAGE {
	WORD	Type;
	INT64	AccountNo;
	WCHAR	ID[20];						// null 포함
	WCHAR	Nickname[20];				// null 포함
	
	WORD	MessageLen;

	//WCHAR	Message[MessageLen / 2];		// null 미포함
	//WCHAR* Message;
};
struct MSG_PACKET_CS_CHAT_REQ_HEARTBEAT {
	WORD		Type;
};

#pragma pack(pop)