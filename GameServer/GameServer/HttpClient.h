#pragma once

#if(AI_BOT == 1)

#include <string>
#include <winhttp.h>

class CHttpClient
{
public:
	CHttpClient();
	bool PostJson(const char* url, const char* apiKey, const char* body, int timeoutMs, std::string& response);

private:
	bool ParseUrl(const char* url, std::wstring& host, std::wstring& path, INTERNET_PORT& port, bool& secure);
};

#endif
