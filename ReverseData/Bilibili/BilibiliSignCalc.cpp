// BilibiliSignCalc.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"
#include "md5.h"
#include <string.h>

int main()
{
	int i;
	unsigned char encrypt[] = "appkey=1d8b6e7d45233436&build=508000&channel=meizu&mobi_app=android&plat=0&platform=android&ts=1499765854&type=1";//b4191a2d1d6d6df3a2068e850d9230f6  
	
	unsigned char salt[] = "560c52ccd288fed045859ed18bffd973";

	unsigned char decrypt[16];
	MD5_CTX md5;
	MD5Init(&md5);
	MD5Update(&md5, encrypt, strlen((char *)encrypt));
	MD5Update(&md5, salt, strlen((char *)salt));
	MD5Final(&md5, decrypt);
	printf("加密前:%s\n加密后:", encrypt);
	for (i = 0; i<16; i++)
	{
		printf("%02x", decrypt[i]);
	}

	getchar();

	return 0;
}

