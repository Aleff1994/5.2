#include "stdafx.h"
#include "HttpClient.h"

#if(AI_BOT == 1)

#pragma comment(lib,"winhttp.lib")

static std::wstring AIBotAnsiToWide(const char* text)
{
	int size = MultiByteToWideChar(CP_UTF8,0,text,-1,0,0);
	UINT codePage = CP_UTF8;
	if(size <= 0)
	{
		codePage = CP_ACP;
		size = MultiByteToWideChar(codePage,0,text,-1,0,0);
	}
	if(size <= 0)
	{
		return L"";
	}
	std::wstring out(size,0);
	MultiByteToWideChar(codePage,0,text,-1,&out[0],size);
	if(!out.empty() && out[out.size()-1] == 0){ out.resize(out.size()-1); }
	return out;
}

CHttpClient::CHttpClient()
{
}

bool CHttpClient::ParseUrl(const char* url, std::wstring& host, std::wstring& path, INTERNET_PORT& port, bool& secure)
{
	std::string value = ((url == 0) ? "" : url);
	secure = false;
	port = INTERNET_DEFAULT_HTTP_PORT;

	size_t offset = 0;
	if(value.find("https://") == 0)
	{
		secure = true;
		port = INTERNET_DEFAULT_HTTPS_PORT;
		offset = 8;
	}
	else if(value.find("http://") == 0)
	{
		offset = 7;
	}
	else
	{
		return false;
	}

	size_t slash = value.find('/',offset);
	std::string hostPort = (slash == std::string::npos) ? value.substr(offset) : value.substr(offset,slash-offset);
	std::string pathText = (slash == std::string::npos) ? "/" : value.substr(slash);

	size_t colon = hostPort.find(':');
	if(colon != std::string::npos)
	{
		port = (INTERNET_PORT)atoi(hostPort.substr(colon+1).c_str());
		hostPort = hostPort.substr(0,colon);
	}

	host = AIBotAnsiToWide(hostPort.c_str());
	path = AIBotAnsiToWide(pathText.c_str());
	return (!host.empty());
}

bool CHttpClient::PostJson(const char* url, const char* apiKey, const char* body, int timeoutMs, std::string& response)
{
	response.clear();
	std::wstring host,path;
	INTERNET_PORT port = 0;
	bool secure = false;

	if(this->ParseUrl(url,host,path,port,secure) == false)
	{
		return false;
	}

	HINTERNET session = WinHttpOpen(L"MU-AIBot/1.0",WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0);
	if(session == 0)
	{
		return false;
	}

	WinHttpSetTimeouts(session,timeoutMs,timeoutMs,timeoutMs,timeoutMs);
	HINTERNET connect = WinHttpConnect(session,host.c_str(),port,0);
	if(connect == 0)
	{
		WinHttpCloseHandle(session);
		return false;
	}

	HINTERNET request = WinHttpOpenRequest(connect,L"POST",path.c_str(),0,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,(secure ? WINHTTP_FLAG_SECURE : 0));
	if(request == 0)
	{
		WinHttpCloseHandle(connect);
		WinHttpCloseHandle(session);
		return false;
	}

	std::wstring headers = L"Content-Type: application/json\r\n";
	if(apiKey != 0 && apiKey[0] != 0)
	{
		headers += L"Authorization: Bearer ";
		headers += AIBotAnsiToWide(apiKey);
		headers += L"\r\n";
	}

	DWORD bodySize = (DWORD)strlen(body);
	BOOL ok = WinHttpSendRequest(request,headers.c_str(),(DWORD)-1,(LPVOID)body,bodySize,bodySize,0);
	if(ok != FALSE)
	{
		ok = WinHttpReceiveResponse(request,0);
	}

	if(ok != FALSE)
	{
		DWORD size = 0;
		do
		{
			size = 0;
			if(WinHttpQueryDataAvailable(request,&size) == FALSE || size == 0)
			{
				break;
			}
			std::string buffer(size+1,0);
			DWORD read = 0;
			if(WinHttpReadData(request,&buffer[0],size,&read) == FALSE)
			{
				break;
			}
			buffer.resize(read);
			response += buffer;
		}
		while(size > 0);
	}

	WinHttpCloseHandle(request);
	WinHttpCloseHandle(connect);
	WinHttpCloseHandle(session);
	return (ok != FALSE && !response.empty());
}

#endif
