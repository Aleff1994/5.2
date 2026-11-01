#pragma once

#include <cstddef>
#include <functional>
#include <vector>

enum class RenderQueueType
{
	Sky = 0,
	Terrain,
	Opaque,
	Transparent,
	Particles,
	Effects,
	Text,
	Interface,
	Count
};

class RenderQueue
{
public:
	typedef std::function<void()> Command;

	RenderQueue();
	void BeginFrame();
	void Submit(RenderQueueType type, unsigned long long sortKey, const Command& command);
	static unsigned long long MakeBatchKey(unsigned int shader, unsigned int texture,
		unsigned int model, unsigned int material, unsigned int state);
	void Execute();
	void Clear();
	size_t GetCount() const;
	size_t GetCount(RenderQueueType type) const;

private:
	struct Entry
	{
		RenderQueueType Type;
		unsigned long long SortKey;
		unsigned long long Sequence;
		Command ExecuteCommand;
	};

	static bool CompareEntries(const Entry& left, const Entry& right);
	std::vector<Entry> m_Entries;
	unsigned long long m_NextSequence;
};

extern RenderQueue g_ModernRenderQueue;
