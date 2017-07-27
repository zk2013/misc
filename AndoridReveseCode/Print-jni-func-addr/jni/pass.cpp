/*
 * Copyright (C) 2009 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */
#include<stdio.h>
#include <string.h>
#include <jni.h>
#include <unistd.h>
#include <sched.h>
#include <signal.h>
#include <pthread.h>
#include <dirent.h>
#include <dlfcn.h>
#include <stdlib.h>
//#include "substrate.h"
#include "common.h"
#define GETLR(store_lr)  \
  __asm__ __volatile__(  \
    "mov %0, lr\n\t"  \
    :  "=r"(store_lr)  \
  )
extern "C" {
    JNIEXPORT void JNICALL init();
};
//#pragma obfuscate on
//void __attribute__ ((constructor)) my_init(void);

struct liblist{
	char name[256];
	unsigned long start;
	unsigned long end;
};
liblist * g_liblist = NULL;
typedef const void *MSImageRef;
typedef MSImageRef (*MSGetImageByName_f)(const char *file);
typedef void *(*MSFindSymbol_f)(MSImageRef image, const char *name);
typedef void (*MSHookFunction_f)(void *symbol, void *replace, void **result);

typedef void (*dvmPlatformInvoke_f)(void* pEnv, void* clazz, int argInfo, int argc,void* argv, const char* shorty, void* func, void* pReturn);
static MSGetImageByName_f MSGetImageByName;
static MSFindSymbol_f MSFindSymbol;
static MSHookFunction_f MSHookFunction;

static dvmPlatformInvoke_f g_dvmPlatformInvoke_old;
FILE* g_file = 0;
char* jstringTochar(JNIEnv* env, jstring jstr)
{
	char* rtn = NULL;
	jclass clsstring = env->FindClass("java/lang/String");
	jstring strencode = env->NewStringUTF("utf-8");
	jmethodID mid = env->GetMethodID(clsstring, "getBytes", "(Ljava/lang/String;)[B");
	jbyteArray barr= (jbyteArray)env->CallObjectMethod(jstr, mid, strencode);
	jsize alen = env->GetArrayLength(barr);
	jbyte* ba = env->GetByteArrayElements(barr, JNI_FALSE);
	if (alen > 0)
	{
		rtn = (char*)malloc(alen + 1);		
		memcpy(rtn, ba, alen);
		rtn[alen] = 0;
	}
	env->ReleaseByteArrayElements(barr, ba, 0);
	return rtn;
}

void GetLibList()
{
	FILE *maps;
	int i = 0;
  char name[128], line[512];
  snprintf(name, sizeof(name), "/proc/%d/maps", getpid());
  if ((maps = fopen(name, "r")) == NULL) {
		return;
	}
  while (fgets(line, sizeof(line)-1, maps) )
	{
		unsigned long start, end;
		char read, write, exec, cow, filename[256];
		int offset, dev_major, dev_minor, inode;

		/* initialise to zero */
		memset(filename, '\0', sizeof(filename));
		/* parse each line */
		
		if (sscanf(line, "%lx-%lx %c%c%c%c %x %x:%x %u %s", &start, &end, &read,
				&write, &exec, &cow, &offset, &dev_major, &dev_minor, &inode, filename) >= 6)
		{
			if (i >= 256) break;
			if (/*strstr(filename,"/data/") &&*/ strstr(filename,".so") && exec == 'x')
			{				
				strcpy(g_liblist[i].name,filename);	
				g_liblist[i].start = start;
				g_liblist[i].end = end;
				i++;
			}
		}
	}
	fclose(maps);	
}

bool GetModuleInfo(unsigned int addr,char* outlibname,unsigned int &outlibbase) 
{
	bool result = false;
	unsigned long start, end;
	for (int i=0; i < 256;i++)
	{
		//if (strstr(g_liblist[i].name,"/system/") && strstr(g_liblist[i].name,".so") && 
		//		(addr > g_liblist[i].start && addr < g_liblist[i].end))
		if (addr > g_liblist[i].start && addr < g_liblist[i].end)
		{
			strcpy(outlibname,g_liblist[i].name);	
			outlibbase = g_liblist[i].start;
			result = true;
			break;		
		}
	} 
  return result;
}

bool GetModuleBase(char* inlibname, unsigned int &outlibbase) {
	int ret = false;

	for (int i=0; i < 256;i++)
	{
		if (strstr(g_liblist[i].name, inlibname)) {
			outlibbase = g_liblist[i].start;
			ret = true;
			break;		
		}
	}

	return ret;
}

static void dvmPlatformInvoke_hook(void* pEnv, void* clazz, int argInfo, int argc, int* argv, const char* shorty, void* func, void* pReturn)
{
	unsigned int addr = (unsigned int)func;
	
	//if (((unsigned int)func > 0x4bfc5000) && ((unsigned int)func < 0x4bff0000))
	{
		//GetModuleInfo(pEnv,addr,argc,argv,shorty);
		/*char path_map[512]={0};
		if (GetModulePath(path_map, (unsigned int)addr)){
			if (!strstr(path_map, "/system"))
			{
				LOGI("%s  ->  func:0x%X", path_map, (unsigned int)addr);
			}
		}*/
	}
	char szLibName[256] = "\0";
	unsigned int libbase = 0;
	if (GetModuleInfo(addr,szLibName,libbase))
	{
		if (strstr(szLibName,"/data/"))
		{
			/*if((addr-libbase) == 0x4e49)
			{
				LOGI("---------------");
				sleep(5); 
			}*/
			LOGI("%s,%x,%x,%x",szLibName,libbase,addr,addr-libbase);
		}
	}	
	else
	{
		LOGI("GetModuleInfo fail,%x",addr);
	}
	g_dvmPlatformInvoke_old(pEnv, clazz, argInfo, argc, argv, shorty, func, pReturn);
}

static void HookFun(char *lpLibName,char *lpFunName,void *lpFunHookAddr,void **lpFunOldAddr)
{
	void * libc = (void *)MSGetImageByName(lpLibName);
	void * lpFunAddr = 0;
	
	if ( libc )
	{
		lpFunAddr = MSFindSymbol(libc, lpFunName);	
	}
	else
	{
		libc = dlopen(lpLibName, 1);
		if ( libc )
		{
			lpFunAddr = dlsym(libc, lpFunName);	
		}
	}
	if ( lpFunAddr )
	{
		MSHookFunction(lpFunAddr, lpFunHookAddr, lpFunOldAddr);	
		LOGI("Hook %s:%02X,%02X",lpFunName,lpFunAddr,lpFunOldAddr);
	}
}

// private native String native_newmakeUrl(Context p1, String p2, 
//String[] p3, String[] p4, String[] p5, String[] p6, int p7, int p8);
typedef void*    (*proto_newmakeUrl)(void*,void*,void* p1, void* p2,
void* p3, void* p4,void* p5,void* p6, int, int );

proto_newmakeUrl origin_newmakeUrl = NULL;


typedef void*    (*proto_makeUrl)(void*,void*,void* p1, void* p2,
void* p3, void* p4,void* p5,void* p6, int, int );

proto_makeUrl origin_makeUrl = NULL;

//  private native String native_authcodeEncode(Context p1, String p2, String p3);
typedef void*    (*proto_authcodeEncode)(void*,void*,void* p1, void* p2, void* p3);
proto_authcodeEncode origin_authcodeEncode = NULL;

void*   my_authcodeEncode(void* env,void* thiz,void* ctx, void* p2,
void* p3) {
	LOGI("my_authcodeEncode called");

	if (origin_authcodeEncode) {
		jstring ret = (jstring)(origin_authcodeEncode(env, thiz, ctx, p2, p3));
		char* strRet = jstringTochar((JNIEnv*)env,ret );
		char* strP2 = jstringTochar((JNIEnv*)env,(jstring)p2 );
		char* strP3 = jstringTochar((JNIEnv*)env,(jstring)p3 );

		LOGI("ret = %s", strRet);
		LOGI("p2 = %s", strP2);
		LOGI("p3 = %s", strP3);
		
		free(strRet);
		free(strP2);
		free(strP3);

		return ret;
	}
	else {
		return NULL;
	}
}

void*   my_newmakeUrl(void* env,void* thiz,void* ctx, void* p2,
void* p3, void* p4,void* p5,void* p6, int p7, int p8) {
	LOGI("my_newmakeUrl called");

	if (origin_newmakeUrl)
	{
		jstring ret = (jstring)(origin_newmakeUrl(env, thiz, ctx, p2, p3, p4, p5, p6, p7, p8));
		char *strRet = jstringTochar((JNIEnv *)env, ret);
		LOGI("ret = %s", strRet);
		free(strRet);
		return ret;
	}
	else
	{
		return NULL;
	}
}


void*   my_makeUrl(void* env,void* thiz,void* ctx, void* p2,
void* p3, void* p4,void* p5,void* p6, int p7, int p8) {
	LOGI("my_makeUrl called");

	if (origin_makeUrl)
	{
		jstring ret = (jstring)(origin_makeUrl(env, thiz, ctx, p2, p3, p4, p5, p6, p7, p8));
		char *strRet = jstringTochar((JNIEnv *)env, ret);
		char* strP2 = jstringTochar((JNIEnv*)env,(jstring)p2 );
		LOGI("p2 = %s", strP2);
		LOGI("ret = %s", strRet);
		free(strRet);
		free(strP2);
		return ret;
	}
	else
	{
		return NULL;
	}
}

//sub_EC48((char *)v46, v54, (int)v47, v53, (int)url, (char **)&final_result, a9, a10, &auth_value);
typedef int (*proto_truemakeUrl)(char* salt1, int salt1_len,char* salt2, int salt2_len,
char* url, char** final_result,int a9, int a10, char* auth);
proto_truemakeUrl origin_truemakeUrl = NULL;

void print_hex(char* buf_name, char* buf, int len) {
	char log_str[100] = {0};
	char hex_str[100] = {0};

	for(int i = 0; i < len; i++) {
		snprintf(&hex_str[2*i],100-2*i,"%02x",buf[i]);
	}
	snprintf(log_str, 100, "%s(%d)=%s",buf_name,len,hex_str);
	LOGI("%s",log_str);
}

int my_truemakeUrl(char* salt1, int salt1_len,char* salt2, int salt2_len,
char* url, char** final_result,int a9, int a10, char* auth) {
	int ret = 0;
	LOGI("--------------");

	print_hex("salt1",salt1, salt1_len);
	print_hex("salt2",salt2, salt2_len);
	LOGI("url=%s", url);
	LOGI("a9=%d", a9);
	LOGI("a10=%d", a10);
	ret = origin_truemakeUrl(salt1,salt1_len, salt2,salt2_len,url,final_result,a9, a10, auth  );
	LOGI("");
	LOGI("final_result=%s", *final_result);
	LOGI("auth=%s", *auth);

	LOGI("--------------");
	return ret;
}

unsigned int func_makeUrl_offset = 0x495c + 1;
unsigned int func_newmakeUrl_offset = 0x4eb0 + 1;
unsigned int func_authcodeEncode_offset = 0x5a10 + 1;
unsigned int func_stringFromJNI_offset = 0x47c1;
unsigned int func_truemakeUrl_offset = 0xec48 + 1;

//public native String stringFromJNI();
typedef void*    (*proto_stringFromJNI)(void*,void*);
proto_stringFromJNI origin_stringFromJNI = NULL;

void*   my_stringFromJNI(void* env,void* thiz) {
	LOGI("my_stringFromJNI called");

	if (origin_stringFromJNI) {
		return origin_stringFromJNI(env, thiz);
	}
	else {
		return NULL;
	}
}

void output_addr_func1(char *libname, char *funcname)
{
	void *libc = (void *)MSGetImageByName(libname);
	void *lpFunAddr = 0;

	if (libc)
	{
		lpFunAddr = MSFindSymbol(libc, funcname);
	}
	else
	{
		libc = dlopen(libname, 1);
		if (libc)
		{
			lpFunAddr = dlsym(libc, funcname);
		}
	}
	if (lpFunAddr)
	{
		LOGI("output_addr_func1  %s-%s:%02X,%02X",libname, funcname, libc, lpFunAddr);
	}
}

void init()
{	
	//g_file = fopen("/data/data/com.UCMobile/Temp/xxxx.txt","w+");
	//fprintf(g_file,"%d:%s\n",1,"xxxxxxxx");
	
	LOGI("inject ok");
	g_liblist = (liblist*)malloc(sizeof(liblist)*256);
	memset(g_liblist,0,sizeof(liblist)*256);
	GetLibList();
	void * libc = dlopen("/data/local/tmp/libsubstrate.so", 1);
	MSGetImageByName = 0;
	MSFindSymbol = 0;
	MSHookFunction = 0;
	if (libc){
		MSGetImageByName = (MSGetImageByName_f)dlsym(libc, "MSGetImageByName");
 		MSFindSymbol = (MSFindSymbol_f)dlsym(libc, "MSFindSymbol");
		MSHookFunction = (MSHookFunction_f)dlsym(libc, "MSHookFunction");
		if (MSGetImageByName && MSFindSymbol && MSHookFunction){		
			LOGI("MSGetImageByName MSFindSymbol MSHookFunction: %p,%p,%p\n",MSGetImageByName, MSFindSymbol, MSHookFunction);
		}
	}
	char libName[] = "libmakeurl.so";
	unsigned int libmakeurl_base = 0;
	if (GetModuleBase(libName, libmakeurl_base)) {
			LOGI("find %s ok(%08x)", libName, libmakeurl_base);
	}
	else {
		LOGI("not find %s",libName );
		return ;
	}

void* func_truemakeUrl = (void*)(libmakeurl_base + func_truemakeUrl_offset);
	MSHookFunction(func_truemakeUrl, (void *)my_truemakeUrl, (void **)&origin_truemakeUrl);
	LOGI("print msg after call MSHookFunction");

/*output_addr_func1(libName, "Java_com_venustv_myapplication_MainActivity_stringFromJNI");


	void* func_stringFromJNI = (void*)(libmakeurl_base + func_stringFromJNI_offset);

LOGI("use hardcode offset  %s-%s:%02X,%02X",libName,"Java_com_venustv_myapplication_MainActivity_stringFromJNI",
 libmakeurl_base, func_stringFromJNI);

	MSHookFunction(func_stringFromJNI, (void *)my_stringFromJNI, (void **)&origin_stringFromJNI);
	LOGI("print msg after call MSHookFunction");

HookFun(libName,"Java_com_venustv_myapplication_MainActivity_stringFromJNI", (void*)my_stringFromJNI,(void**)&origin_stringFromJNI);


	


void* func_newmakeUrl = (void*)(libmakeurl_base + func_newmakeUrl_offset);
	MSHookFunction(func_newmakeUrl, (void *)my_newmakeUrl, (void **)&origin_newmakeUrl);
 
void* func_makeUrl = (void*)(libmakeurl_base + func_makeUrl_offset);
	MSHookFunction(func_makeUrl, (void *)my_makeUrl, (void **)&origin_makeUrl);
 

	void* func_authcodeEncode = (void*)(libmakeurl_base + func_authcodeEncode_offset);
	MSHookFunction(func_authcodeEncode, (void *)my_authcodeEncode, (void **)&origin_authcodeEncode);
	LOGI("print msg after call MSHookFunction (%08x)",origin_authcodeEncode );*/

	/*
	libc = (void *)MSGetImageByName(libName);
	char funDvmPlatformInvokeName[] = "dvmPlatformInvoke";
	void * fDvmPlatformInvoke = 0;
	if ( libc )
	{
		LOGI("MSGetImageByName ok");
		fDvmPlatformInvoke = MSFindSymbol(libc, funDvmPlatformInvokeName);	
	}
	else
	{
		libc = dlopen(libName, 1);
		if ( libc )
		{
			LOGI("dlopen ok");
			//fDvmPlatformInvoke = dlsym(libc, funDvmPlatformInvokeName);	
		}
	}
	//LOGI("fDvmPlatformInvoke:%02X",fDvmPlatformInvoke);
	//if ( fDvmPlatformInvoke )
	//{
		//MSHookFunction(fDvmPlatformInvoke, (void *)dvmPlatformInvoke_hook, (void **)&g_dvmPlatformInvoke_old);	
//	}	*/
}
//#pragma obfuscate off