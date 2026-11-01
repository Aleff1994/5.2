#pragma once

#if(AI_BOT == 1)

#include "User.h"
#include <deque>
#include <string>
#include <map>
#include <utility>

#define MAX_AIBOT_MESSAGE 90

struct AIBOT_REQUEST
{
	int PlayerIndex;
	int BotIndex;
	DWORD ReadyTick;
	char PlayerName[11];
	char BotName[11];
	char Question[180];
};

class CAIBot
{
public:
	CAIBot();
	void Load(char* path);
	bool IsMessageForBot(LPOBJ lpObj, char* message);
	void ProcessPlayerMessage(LPOBJ lpObj, char* message);
	void ProcessPendingResponses();

private:
	int Enabled;
	int UseApi;
	int ResponseDelayMs;
	int MaxMessageLength;
	int TimeoutMs;
	int SearchRange;
	char ApiUrl[256];
	char ApiKey[256];
	char Model[64];
	char BotName[11];
	char SystemPrompt[512];
	char FallbackMessage[180];
	CRITICAL_SECTION Critical;
	std::deque<AIBOT_REQUEST> Queue;
	std::map<int, std::pair<std::string, DWORD>> LastChatRequest;

	bool StartsWithPrefix(char* message);
	void Trim(char* text);
	int FindTargetBot(LPOBJ lpObj, char* question);
	int FindAddressedBot(LPOBJ lpObj, const char* message);
	void QueueRequest(LPOBJ lpObj, int botIndex, char* question);
	void BuildLocalAnswer(char* question, char* output, int outputSize);
	bool RequestApi(char* question, char* output, int outputSize);
	void SendAnswer(AIBOT_REQUEST* request, char* answer);
	void LimitText(char* text, int maxLen);
	void LoadValue(char* path, const char* key, char* out, int outSize, const char* def);
	int LoadNumber(char* path, const char* key, int def);
};

extern CAIBot gAIBot;

#endif
