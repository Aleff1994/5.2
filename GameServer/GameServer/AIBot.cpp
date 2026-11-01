#include "stdafx.h"
#include "AIBot.h"
#include "BotPlayer.h"
#include "FakeBot.h"
#include "HttpClient.h"
#include "Log.h"
#include "Notice.h"
#include "Protocol.h"
#include "Util.h"
#include <cctype>

#if(AI_BOT == 1)

CAIBot gAIBot;

CAIBot::CAIBot()
{
	this->Enabled = 0;
	this->UseApi = 0;
	this->ResponseDelayMs = 500;
	this->MaxMessageLength = 90;
	this->TimeoutMs = 15000;
	this->SearchRange = 15;
	strcpy_s(this->ApiUrl,"https://api.openai.com/v1/chat/completions");
	memset(this->ApiKey,0,sizeof(this->ApiKey));
	strcpy_s(this->Model,"gpt-3.5-turbo");
	strcpy_s(this->BotName,"AIHelper");
	strcpy_s(this->SystemPrompt,"Voce e um assistente de MU Online. Responda curto em portugues.");
	strcpy_s(this->FallbackMessage,"Estou online! Configure a API no AIBot.txt para eu responder com inteligencia artificial.");
	InitializeCriticalSection(&this->Critical);
}

void CAIBot::Trim(char* text)
{
	if(text == 0){ return; }
	int len = (int)strlen(text);
	while(len > 0 && (text[len-1] == '\r' || text[len-1] == '\n' || text[len-1] == ' ' || text[len-1] == '\t'))
	{
		text[--len] = 0;
	}
	char* start = text;
	while(*start == ' ' || *start == '\t')
	{
		start++;
	}
	if(start != text)
	{
		memmove(text,start,strlen(start)+1);
	}
}

void CAIBot::LoadValue(char* path, const char* key, char* out, int outSize, const char* def)
{
	FILE* fp = fopen(path,"rt");
	if(fp == 0)
	{
		strncpy_s(out,outSize,def,_TRUNCATE);
		return;
	}

	char line[1024];
	char prefix[128];
	wsprintf(prefix,"%s=",key);
	int found = 0;

	while(fgets(line,sizeof(line),fp) != 0)
	{
		this->Trim(line);
		if(line[0] == 0 || line[0] == ';')
		{
			continue;
		}
		if(_strnicmp(line,prefix,(int)strlen(prefix)) == 0)
		{
			strncpy_s(out,outSize,&line[strlen(prefix)],_TRUNCATE);
			found = 1;
			break;
		}
	}

	fclose(fp);
	if(found == 0)
	{
		strncpy_s(out,outSize,def,_TRUNCATE);
	}
}

int CAIBot::LoadNumber(char* path, const char* key, int def)
{
	char value[64];
	this->LoadValue(path,key,value,sizeof(value),"");
	return (value[0] == 0) ? def : atoi(value);
}

void CAIBot::Load(char* path)
{
	this->Enabled = this->LoadNumber(path,"Enabled",0);
	this->UseApi = this->LoadNumber(path,"UseApi",0);
	this->ResponseDelayMs = this->LoadNumber(path,"ResponseDelayMs",500);
	this->MaxMessageLength = this->LoadNumber(path,"MaxMessageLength",90);
	if (this->MaxMessageLength < 1) this->MaxMessageLength = 1;
	if (this->MaxMessageLength >= MAX_AIBOT_MESSAGE) this->MaxMessageLength = MAX_AIBOT_MESSAGE - 1;
	this->TimeoutMs = this->LoadNumber(path,"TimeoutMs",15000);
	this->SearchRange = this->LoadNumber(path,"SearchRange",15);
	if (this->SearchRange < 1) this->SearchRange = 1;
	if (this->SearchRange > 30) this->SearchRange = 30;
	this->LoadValue(path,"ApiUrl",this->ApiUrl,sizeof(this->ApiUrl),"https://api.openai.com/v1/chat/completions");
	this->LoadValue(path,"ApiKey",this->ApiKey,sizeof(this->ApiKey),"");
	this->LoadValue(path,"Model",this->Model,sizeof(this->Model),"gpt-3.5-turbo");
	this->LoadValue(path,"BotName",this->BotName,sizeof(this->BotName),"AIHelper");
	this->LoadValue(path,"SystemPrompt",this->SystemPrompt,sizeof(this->SystemPrompt),"Voce e um assistente de MU Online. Responda curto em portugues.");
	this->LoadValue(path,"FallbackMessage",this->FallbackMessage,sizeof(this->FallbackMessage),"Estou online! Configure a API no AIBot.txt para eu responder com inteligencia artificial.");

	EnterCriticalSection(&this->Critical);
	this->Queue.clear();
	this->LastChatRequest.clear();
	LeaveCriticalSection(&this->Critical);

	LogAdd(LOG_GREEN,"[AIBot] Loaded. Enabled=%d UseApi=%d",this->Enabled,this->UseApi);
}

bool CAIBot::StartsWithPrefix(char* message)
{
	return (_strnicmp(message,"#ai",3) == 0 || _strnicmp(message,"!ai",3) == 0 || _strnicmp(message,".ai",3) == 0);
}

bool CAIBot::IsMessageForBot(LPOBJ lpObj, char* message)
{
	return this->Enabled != 0 && lpObj != 0 && message != 0 &&
		(this->StartsWithPrefix(message) || OBJECT_RANGE(this->FindAddressedBot(lpObj, message)));
}

int CAIBot::FindAddressedBot(LPOBJ lpObj, const char* message)
{
	if (lpObj == 0 || message == 0 || !gObjIsConnectedGP(lpObj->Index)) return -1;
	// A whole name anywhere in local chat: "ae BKMASTER", "BKMASTER, vem pt".
	// Other channels and commands are handled by their existing systems.
	if (message[0] == '~' || message[0] == '@' || message[0] == '$' || message[0] == '/') return -1;
	const char* cursor = message;
	while (*cursor)
	{
		const unsigned char c = static_cast<unsigned char>(*cursor);
		if (!(isalnum(c) || c == '_' || c >= 128)) { ++cursor; continue; }
		const char* start = cursor;
		while (*cursor)
		{
			const unsigned char next = static_cast<unsigned char>(*cursor);
			if (!(isalnum(next) || next == '_' || next >= 128)) break;
			++cursor;
		}
		const size_t length = cursor-start;
		if (length == 0 || length > 10) continue;
		char name[11] = {};
		memcpy(name, start, length);
		int index = gBotPlayer.FindBotByName(name);
		if (OBJECT_RANGE(index) && gObjIsConnected(index) && gObj[index].Live &&
			gObj[index].Map == lpObj->Map)
		{
			int dx = lpObj->X-gObj[index].X;
			int dy = lpObj->Y-gObj[index].Y;
			if (dx*dx+dy*dy <= this->SearchRange*this->SearchRange) return index;
		}
		index = gFakeBot.FindChatBot(lpObj->Index, name, this->SearchRange);
		if (OBJECT_RANGE(index)) return index;
	}
	return -1;
}

int CAIBot::FindTargetBot(LPOBJ lpObj, char* question)
{
	char copy[180];
	strncpy_s(copy,question,_TRUNCATE);
	this->Trim(copy);

	char first[32] = {0};
	sscanf_s(copy,"%31s",first,(unsigned int)sizeof(first));
	if(first[0] != 0)
	{
		int named = gBotPlayer.FindBotByName(first);
		if (!OBJECT_RANGE(named)) named = gFakeBot.FindChatBot(lpObj->Index, first, this->SearchRange);
		if(OBJECT_RANGE(named) != 0 && gObjIsConnected(named) != 0 &&
			gObj[named].Live != 0 && gObj[named].Map == lpObj->Map &&
			(lpObj->X-gObj[named].X)*(lpObj->X-gObj[named].X) +
			(lpObj->Y-gObj[named].Y)*(lpObj->Y-gObj[named].Y) <= this->SearchRange*this->SearchRange)
		{
			char* rest = copy + strlen(first);
			while(*rest == ' ' || *rest == '\t'){ rest++; }
			strncpy_s(question,180,rest,_TRUNCATE);
			this->Trim(question);
			return named;
		}
	}

	int nearest = gBotPlayer.FindNearestBot(lpObj->Index);
	if(OBJECT_RANGE(nearest) != 0)
	{
		int distance = (int)sqrt((float)(((lpObj->X-gObj[nearest].X)*(lpObj->X-gObj[nearest].X))+((lpObj->Y-gObj[nearest].Y)*(lpObj->Y-gObj[nearest].Y))));
		if(distance <= this->SearchRange)
		{
			return nearest;
		}
	}
	return gFakeBot.FindChatBot(lpObj->Index, 0, this->SearchRange);
}

void CAIBot::ProcessPlayerMessage(LPOBJ lpObj, char* message)
{
	if (!this->Enabled || lpObj == 0 || message == 0) return;
	const bool prefixed = this->StartsWithPrefix(message);
	char question[180];
	strncpy_s(question, prefixed ? message+3 : message, _TRUNCATE);
	this->Trim(question);
	if (question[0] == 0) return;
	const int botIndex = prefixed ? this->FindTargetBot(lpObj, question) :
		this->FindAddressedBot(lpObj, message);
	if (!prefixed && !OBJECT_RANGE(botIndex)) return;
	if (OBJECT_RANGE(botIndex) && _stricmp(question, gObj[botIndex].Name) == 0)
		strcpy_s(question, "oi");
	this->QueueRequest(lpObj,botIndex,question);
}

void CAIBot::QueueRequest(LPOBJ lpObj, int botIndex, char* question)
{
	AIBOT_REQUEST request;
	memset(&request,0,sizeof(request));
	request.PlayerIndex = lpObj->Index;
	request.BotIndex = botIndex;
	request.ReadyTick = GetTickCount() + this->ResponseDelayMs;
	strncpy_s(request.PlayerName,lpObj->Name,_TRUNCATE);
	if(OBJECT_RANGE(botIndex) != 0)
	{
		strncpy_s(request.BotName,gObj[botIndex].Name,_TRUNCATE);
	}
	else
	{
		strncpy_s(request.BotName,this->BotName,_TRUNCATE);
	}
	strncpy_s(request.Question,question,_TRUNCATE);

	EnterCriticalSection(&this->Critical);
	bool queued = this->Queue.size() < 50;
	const DWORD now = GetTickCount();
	auto recent = this->LastChatRequest.find(request.PlayerIndex);
	if (recent != this->LastChatRequest.end() && recent->second.first == request.PlayerName &&
		now-recent->second.second < 3000) queued = false;
	for (const auto& pending : this->Queue)
	{
		if (pending.PlayerIndex == request.PlayerIndex &&
			strcmp(pending.PlayerName, request.PlayerName) == 0)
		{
			queued = false;
			break;
		}
	}
	if (queued)
	{
		this->Queue.push_back(request);
		this->LastChatRequest[request.PlayerIndex] = std::make_pair(std::string(request.PlayerName), now);
	}
	LeaveCriticalSection(&this->Critical);
}

void CAIBot::BuildLocalAnswer(char* question, char* output, int outputSize)
{
	char lowerQuestion[180];
	strncpy_s(lowerQuestion, question, _TRUNCATE);
	_strlwr_s(lowerQuestion, sizeof(lowerQuestion));
	question = lowerQuestion;
	if(strstr(question,"set") != 0 || strstr(question,"Set") != 0)
	{
		strncpy_s(output,outputSize,"Para set, foque defesa, vida e dano. Diga sua classe que eu ajudo melhor.",_TRUNCATE);
		return;
	}
	if(strstr(question,"joia") != 0 || strstr(question,"jewel") != 0 || strstr(question,"drop") != 0)
	{
		strncpy_s(output,outputSize,"Joias dropam melhor em spots fortes, eventos e bosses. Veja invasoes ativas.",_TRUNCATE);
		return;
	}
	char normalized[180];
	strncpy_s(normalized, question, _TRUNCATE);
	_strlwr_s(normalized, sizeof(normalized));
	if (strstr(normalized, "party") || strstr(normalized, "pt"))
		strncpy_s(output,outputSize,"Pode me convidar para a party! Se houver vaga, eu aceito.",_TRUNCATE);
	else if (strstr(normalized, "oi") || strstr(normalized, "ola") || strstr(normalized, "bom dia") || strstr(normalized, "ae ") || strstr(normalized, "eae"))
		strncpy_s(output,outputSize,"Opa! Estou por aqui. Vamos jogar juntos?",_TRUNCATE);
	else if (strstr(normalized, "pvp") || strstr(normalized, "atac"))
		strncpy_s(output,outputSize,"Estou tranquilo, mas fora da cidade eu me defendo se for atacado.",_TRUNCATE);
	else
		strncpy_s(output,outputSize,this->FallbackMessage,_TRUNCATE);
}

static void AIBotEscapeJson(const char* input, std::string& output)
{
	output.clear();
	for(const char* p=input; *p; p++)
	{
		if(*p == '"' || *p == '\\') { output += '\\'; output += *p; }
		else if(*p == '\r' || *p == '\n') { output += ' '; }
		else { output += *p; }
	}
}

static bool AIBotExtractContent(const std::string& response, char* output, int outputSize)
{
	size_t pos = response.find("\"content\"");
	if(pos == std::string::npos){ return false; }
	pos = response.find(':',pos);
	if(pos == std::string::npos){ return false; }
	pos = response.find('"',pos);
	if(pos == std::string::npos){ return false; }
	pos++;
	std::string value;
	for(size_t n=pos; n<response.size(); n++)
	{
		char c = response[n];
		if(c == '\\' && (n+1) < response.size())
		{
			char next = response[++n];
			value += (next == 'n' || next == 'r') ? ' ' : next;
		}
		else if(c == '"') { break; }
		else { value += c; }
	}
	if(value.empty()){ return false; }
	strncpy_s(output,outputSize,value.c_str(),_TRUNCATE);
	return true;
}

bool CAIBot::RequestApi(char* question, char* output, int outputSize)
{
	if(this->UseApi == 0 || this->ApiKey[0] == 0)
	{
		return false;
	}
	std::string prompt,q;
	AIBotEscapeJson(this->SystemPrompt,prompt);
	AIBotEscapeJson(question,q);
	char body[2048];
	wsprintf(body,"{\"model\":\"%s\",\"messages\":[{\"role\":\"system\",\"content\":\"%s\"},{\"role\":\"user\",\"content\":\"%s\"}],\"max_tokens\":80,\"temperature\":0.7}",this->Model,prompt.c_str(),q.c_str());
	std::string response;
	CHttpClient http;
	if(http.PostJson(this->ApiUrl,this->ApiKey,body,this->TimeoutMs,response) == false)
	{
		return false;
	}
	return AIBotExtractContent(response,output,outputSize);
}

void CAIBot::LimitText(char* text, int maxLen)
{
	if (text == 0) return;
	if (maxLen < 0) maxLen = 0;
	if((int)strlen(text) > maxLen)
	{
		text[maxLen] = 0;
	}
}

void CAIBot::SendAnswer(AIBOT_REQUEST* request, char* answer)
{
	this->LimitText(answer,this->MaxMessageLength);
	if(gObjIsConnectedGP(request->PlayerIndex) == 0 ||
		strcmp(gObj[request->PlayerIndex].Name, request->PlayerName) != 0)
	{
		return;
	}
	if(OBJECT_RANGE(request->BotIndex) != 0)
	{
		const LPOBJ bot = &gObj[request->BotIndex];
		const LPOBJ player = &gObj[request->PlayerIndex];
		int dx = bot->X-player->X;
		int dy = bot->Y-player->Y;
		if (!gObjIsConnected(request->BotIndex) || !bot->Live ||
			strcmp(bot->Name, request->BotName) != 0 || bot->Map != player->Map ||
			dx*dx+dy*dy > this->SearchRange*this->SearchRange) return;
		GCChatTargetSend(player,request->BotIndex,answer);
	}
	else
	{
		GCChatWhisperSend(request->PlayerIndex,request->BotName,answer);
	}
}

void CAIBot::ProcessPendingResponses()
{
	if(this->Enabled == 0)
	{
		return;
	}
	AIBOT_REQUEST request;
	memset(&request,0,sizeof(request));
	bool hasRequest = false;
	EnterCriticalSection(&this->Critical);
	if(!this->Queue.empty() && static_cast<LONG>(GetTickCount() - this->Queue.front().ReadyTick) >= 0)
	{
		request = this->Queue.front();
		this->Queue.pop_front();
		hasRequest = true;
	}
	LeaveCriticalSection(&this->Critical);
	if(hasRequest == false)
	{
		return;
	}
	char answer[MAX_AIBOT_MESSAGE];
	memset(answer,0,sizeof(answer));
	if(this->RequestApi(request.Question,answer,sizeof(answer)) == false)
	{
		this->BuildLocalAnswer(request.Question,answer,sizeof(answer));
	}
	this->SendAnswer(&request,answer);
}

#endif
