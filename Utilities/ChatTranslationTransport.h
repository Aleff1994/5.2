#pragma once
#include <windows.h>
#include <winhttp.h>
#include <string>
#pragma comment(lib,"winhttp.lib")
namespace ChatTranslationTransport {
struct Handle {HINTERNET value;explicit Handle(HINTERNET h):value(h){}~Handle(){if(value)WinHttpCloseHandle(value);}operator HINTERNET()const{return value;}private:Handle(const Handle&);Handle& operator=(const Handle&);};
inline std::string Convert(const std::string& text,unsigned from,unsigned to){
 if(text.empty())return text;
 const int count=MultiByteToWideChar(from,from==CP_UTF8?MB_ERR_INVALID_CHARS:0,text.data(),static_cast<int>(text.size()),NULL,0);
 if(count<=0)return std::string();
 std::wstring wide(count,L'\0');MultiByteToWideChar(from,from==CP_UTF8?MB_ERR_INVALID_CHARS:0,text.data(),static_cast<int>(text.size()),&wide[0],count);
 int bytes=WideCharToMultiByte(to,0,wide.data(),count,NULL,0,NULL,NULL);if(bytes<=0)return std::string();
 std::string result(bytes,'\0');WideCharToMultiByte(to,0,wide.data(),count,&result[0],bytes,NULL,NULL);return result;
}
inline bool Translate(unsigned short port,int timeout,const std::string& source,const std::string& target,const std::string& preserve,const std::string& text,std::string& translated){
 const std::string body=source+"\n"+target+"\n"+Convert(preserve,1252,CP_UTF8)+"\n"+Convert(text,1252,CP_UTF8);
 Handle session(WinHttpOpen(L"NeonMU-Chat/1.0",WINHTTP_ACCESS_TYPE_NO_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0));if(!session.value)return false;
 WinHttpSetTimeouts(session,timeout,timeout,timeout,timeout);
 Handle connection(WinHttpConnect(session,L"127.0.0.1",port,0));if(!connection.value)return false;
 Handle request(WinHttpOpenRequest(connection,L"POST",L"/translate",NULL,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,0));if(!request.value)return false;
 DWORD redirect=WINHTTP_OPTION_REDIRECT_POLICY_NEVER;WinHttpSetOption(request,WINHTTP_OPTION_REDIRECT_POLICY,&redirect,sizeof(redirect));
 if(!WinHttpSendRequest(request,L"Content-Type: text/plain; charset=utf-8\r\n",static_cast<DWORD>(-1),const_cast<char*>(body.data()),static_cast<DWORD>(body.size()),static_cast<DWORD>(body.size()),0) || !WinHttpReceiveResponse(request,NULL))return false;
 DWORD status=0,length=sizeof(status);if(!WinHttpQueryHeaders(request,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,WINHTTP_HEADER_NAME_BY_INDEX,&status,&length,WINHTTP_NO_HEADER_INDEX)||status!=200)return false;
 std::string response;char buffer[512];DWORD read=0;
 do{if(!WinHttpReadData(request,buffer,sizeof(buffer),&read))return false;if(response.size()+read>2048)return false;response.append(buffer,read);}while(read);
 if(response.empty() || response.find('\0')!=std::string::npos || response.find('\r')!=std::string::npos || response.find('\n')!=std::string::npos)return false;
 translated=Convert(response,CP_UTF8,1252);return !translated.empty();
}
}
