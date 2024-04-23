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
	// ä�ü��� �α��� ��û
	//
	//	{
	//		WORD	Type
	//
	//		INT64	AccountNo
	//		WCHAR	ID[20]				// null ����
	//		WCHAR	Nickname[20]		// null ����
	//		char	SessionKey[64];		// ������ū
	//	}
	//
	//------------------------------------------------------------
	en_PACKET_CS_CHAT_REQ_LOGIN,

	//------------------------------------------------------------
	// ä�ü��� �α��� ����
	//
	//	{
	//		WORD	Type
	//
	//		BYTE	Status				// 0:����	1:����
	//		INT64	AccountNo
	//	}
	//
	//------------------------------------------------------------
	en_PACKET_CS_CHAT_RES_LOGIN,

	//------------------------------------------------------------
	// ä�ü��� ���� �̵� ��û
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
	// ä�ü��� ���� �̵� ���
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
	// ä�ü��� ä�ú����� ��û
	//
	//	{
	//		WORD	Type
	//
	//		INT64	AccountNo
	//		WORD	MessageLen
	//		WCHAR	Message[MessageLen / 2]		// null ������
	//	}
	//
	//------------------------------------------------------------
	en_PACKET_CS_CHAT_REQ_MESSAGE,

	//------------------------------------------------------------
	// ä�ü��� ä�ú����� ����  (�ٸ� Ŭ�� ���� ä�õ� �̰ɷ� ����)
	//
	//	{
	//		WORD	Type
	//
	//		INT64	AccountNo
	//		WCHAR	ID[20]						// null ����
	//		WCHAR	Nickname[20]				// null ����
	//		
	//		WORD	MessageLen
	//		WCHAR	Message[MessageLen / 2]		// null ������
	//	}
	//
	//------------------------------------------------------------
	en_PACKET_CS_CHAT_RES_MESSAGE,

	//------------------------------------------------------------
	// ��Ʈ��Ʈ
	//
	//	{
	//		WORD		Type
	//	}
	//
	//
	// Ŭ���̾�Ʈ�� �̸� 30�ʸ��� ������.
	// ������ 40�� �̻󵿾� �޽��� ������ ���� Ŭ���̾�Ʈ�� ������ ������� ��.
	//------------------------------------------------------------	
	en_PACKET_CS_CHAT_REQ_HEARTBEAT,



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
	WCHAR ID[20];			// null ����
	WCHAR Nickname[20];		// null ����
	char sessinKey[64];		// ���� ��ū
};
struct MSG_PACKET_CS_CHAT_RES_LOGIN {
	WORD	Type;
	BYTE	Status;				// 0:����	1:����
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

	//WCHAR	Message[MessageLen / 2];		// null ������
	//WCHAR* Message;
};
struct MSG_PACKET_CS_CHAT_RES_MESSAGE {
	WORD	Type;
	INT64	AccountNo;
	WCHAR	ID[20];						// null ����
	WCHAR	Nickname[20];				// null ����
	
	WORD	MessageLen;

	//WCHAR	Message[MessageLen / 2];		// null ������
	WCHAR* Message;
};
struct MSG_PACKET_CS_CHAT_REQ_HEARTBEAT {
	WORD		Type;
};

#pragma pack(pop)