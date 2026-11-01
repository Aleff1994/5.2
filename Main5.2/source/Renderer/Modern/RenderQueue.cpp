#include "stdafx.h"
#include "RenderQueue.h"

#include <algorithm>

RenderQueue g_ModernRenderQueue;

RenderQueue::RenderQueue() : m_NextSequence(0)
{
}

void RenderQueue::BeginFrame()
{
	Clear();
}

void RenderQueue::Submit(RenderQueueType type, unsigned long long sortKey, const Command& command)
{
	if (!command) return;
	Entry entry;
	entry.Type = type;
	entry.SortKey = sortKey;
	entry.Sequence = m_NextSequence++;
	entry.ExecuteCommand = command;
	m_Entries.push_back(entry);
}

unsigned long long RenderQueue::MakeBatchKey(unsigned int shader, unsigned int texture,
	unsigned int model, unsigned int material, unsigned int state)
{
	unsigned long long hash = 1469598103934665603ULL;
	const unsigned int values[5] = { shader, texture, model, material, state };
	for (int i = 0; i < 5; ++i)
	{
		hash ^= values[i];
		hash *= 1099511628211ULL;
	}
	return hash;
}
bool RenderQueue::CompareEntries(const Entry& left, const Entry& right)
{
	if (left.Type != right.Type)
		return (int)left.Type < (int)right.Type;
	if (left.Type == RenderQueueType::Opaque && left.SortKey != right.SortKey)
		return left.SortKey < right.SortKey;
	if (left.Type == RenderQueueType::Transparent && left.SortKey != right.SortKey)
		return left.SortKey > right.SortKey;
	return left.Sequence < right.Sequence;
}

void RenderQueue::Execute()
{
	std::stable_sort(m_Entries.begin(), m_Entries.end(), CompareEntries);
	for (size_t i = 0; i < m_Entries.size(); ++i)
		m_Entries[i].ExecuteCommand();
	Clear();
}

void RenderQueue::Clear()
{
	m_Entries.clear();
	m_NextSequence = 0;
}

size_t RenderQueue::GetCount() const
{
	return m_Entries.size();
}

size_t RenderQueue::GetCount(RenderQueueType type) const
{
	size_t count = 0;
	for (size_t i = 0; i < m_Entries.size(); ++i)
		if (m_Entries[i].Type == type) ++count;
	return count;
}
