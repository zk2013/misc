/*
 * common.h
 *
 *  Created on: 2014年9月18日
 *      Author: boyliang
 */

#ifndef COMMON_H_
#define COMMON_H_

#include <android/log.h> 
 
#define DISPLAY_LOG 
//#define LOG_PRINT 
#define LOG_TAG "MyPrint" 
 
 
#ifdef DISPLAY_LOG 
#define LOGV(...)  __android_log_print(ANDROID_LOG_VERBOSE,LOG_TAG,__VA_ARGS__) 
#define LOGD(...)  __android_log_print(ANDROID_LOG_DEBUG,LOG_TAG,__VA_ARGS__) 
#define LOGI(...)  __android_log_print(ANDROID_LOG_INFO,LOG_TAG,__VA_ARGS__) 
#define LOGW(...)  __android_log_print(ANDROID_LOG_WARN,LOG_TAG,__VA_ARGS__) 
#define LOGE(...)  __android_log_print(ANDROID_LOG_ERROR,LOG_TAG,__VA_ARGS__) 
#define LOGF(...)  __android_log_print(ANDROID_LOG_FATAL,LOG_TAG,__VA_ARGS__) 
 
#else 
#ifdef LOG_PRINT 
#define LOGV(...)  while(0){}
#define LOGD(...)  while(0){}
#define LOGI(...)  while(0){}
#define LOGW(...)  while(0){}
#define LOGE(...)  while(0){}
#define LOGF(...)  while(0){}
#else 
#define LOGV 
#define LOGD 
#define LOGI 
#define LOGW 
#define LOGE 
#define LOGF 
#endif 
#endif 

#endif /* COMMON_H_ */
