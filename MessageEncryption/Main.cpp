#include <Windows.h>

#define dfPACKET_KEY	0xa9	// 고정 키
#define dfRAND_KEY		0x31	// 랜덤 키

bool Decode(BYTE randKey, USHORT payloadLen, BYTE* payloads)
{
	BYTE Pb = 0;
	BYTE Eb = 0;

	for (USHORT i = 1; i <= payloadLen; i++) {
		BYTE Pn = payloads[i - 1] ^ (Eb + dfPACKET_KEY + i);
		BYTE Dn = Pn ^ (Pb + randKey + i);

		Pb = Pn;
		Eb = payloads[i - 1];

		payloads[i - 1] = Dn;
	}

	return true;
}
void Encode(BYTE randKey, USHORT payloadLen, BYTE* payloads)
{
	//# 원본 데이터 바이트 단위  D1 D2 D3 D4
	//----------------------------------------------------------------------------------------------------------
	//| D1 | D2 | D3 | D4 |
	//----------------------------------------------------------------------------------------------------------
	//D1 ^ (RK + 1) = P1 | D2 ^ (P1 + RK + 2) = P2 | D3 ^ (P2 + RK + 3) = P3 | D4 ^ (P3 + RK + 4) = P4 |
	//P1 ^ (K + 1) = E1 | P2 ^ (E1 + K + 2) = E2 | P3 ^ (E2 + K + 3) = E3 | P4 ^ (E3 + K + 4) = E4 |
	BYTE Pb = 0;
	BYTE Eb = 0;

	for (USHORT i = 1; i <= payloadLen; i++) {
		BYTE Pn = payloads[i - 1] ^ (Pb + randKey + (BYTE)i);
		BYTE En = Pn ^ (Eb + dfPACKET_KEY + (BYTE)i);

		payloads[i - 1] = En;

		Pb = Pn;
		Eb = En;
	}
}

int main() {
	char charArray[55] = {
		'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a',
		'b', 'b', 'b', 'b', 'b', 'b', 'b', 'b', 'b', 'b',
		'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c',
		'1', '2', '3', '4', '5', '6', '7', '8', '9', '0',
		'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', NULL
	};

	short shortArray[55] = {
		0x61, 0x61, 0x61, 0x61, 0x61, 0x61, 0x61, 0x61, 0x61, 0x61,
		0x62, 0x62, 0x62, 0x62, 0x62, 0x62, 0x62, 0x62, 0x62, 0x62,
		0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63,
		0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x30,
		0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x00
	};

	Encode(dfRAND_KEY, 55, (BYTE*)charArray);
	Decode(dfRAND_KEY, 55, (BYTE*)charArray);

	Encode(dfRAND_KEY, 55 * 2, (BYTE*)shortArray);
	Decode(dfRAND_KEY, 55 * 2, (BYTE*)shortArray);
}