#pragma once
#include "ExcRankProtocol.h"
#include <deque>
#include <map>
#include <mutex>
namespace ExcRankCache
{
using namespace ExcRankProtocol;
enum class State
{
    Unknown,
    Queued,
    Pending,
    Ready,
    Legacy,
    Error
};
struct Record
{
    State state = State::Unknown;
    unsigned itemId = 0, retries = 0;
    bool poisoned = false;
    std::uint32_t stamp = 0;
    ExcRank::ItemOptions metadata;
};
struct Batch
{
    std::uint32_t started = 0;
    std::vector<std::uint32_t> serials;
};
template <class Backend> struct Cache
{
    Backend &backend;
    explicit Cache(Backend &source) : backend(source)
    {
    }
    std::mutex mutex;
    std::map<std::uint32_t, Record> records;
    std::deque<std::uint32_t> queue;
    std::vector<std::uint32_t> readySerials;
    std::map<std::uint32_t, Batch> batches;
    std::uint32_t sweepAfter = 0;
    // Can be called during inventory/warehouse deserialization. No DB/network here.
    void Request(std::uint32_t serial, unsigned itemId)
    {
        if (!serial || itemId > 8191 || !backend.Enabled())
            return;
        std::lock_guard<std::mutex> lock(mutex);
        auto it = records.find(serial);
        if (it != records.end())
        {
            if (it->second.itemId != itemId)
            {
                it->second.state = State::Error;
                it->second.poisoned = true;
                return;
            }
            if (it->second.poisoned || it->second.state != State::Error ||
                backend.Now() - it->second.stamp < 5000)
                return;
        }
        else if (records.size() >= 65536)
            return;
        Record record;
        record.state = State::Queued;
        record.itemId = itemId;
        record.stamp = backend.Now();
        records[serial] = record;
        queue.push_back(serial);
    }
    State Lookup(std::uint32_t serial, unsigned itemId, ExcRank::ItemOptions &metadata)
    {
        std::lock_guard<std::mutex> lock(mutex);
        auto i = records.find(serial);
        if (i == records.end())
            return State::Unknown;
        if (i->second.itemId != itemId)
            return State::Error;
        if (i->second.state == State::Ready)
        {
            metadata = i->second.metadata;
            i->second.stamp = backend.Now();
        }
        return i->second.state;
    }
    void Touch(std::uint32_t serial)
    {
        std::lock_guard<std::mutex> lock(mutex);
        auto i = records.find(serial);
        if (i != records.end() && i->second.state == State::Ready)
            i->second.stamp = backend.Now();
    }
    // Called on the Win32 message thread (DS dispatch and maintenance timer).
    void Pump()
    {
        if (!backend.Enabled())
            return;
        std::lock_guard<std::mutex> lock(mutex);
        const std::uint32_t now = backend.Now();
        for (auto i = batches.begin(); i != batches.end();)
        {
            Message response;
            if (backend.Take(i->first, response))
            {
                for (const auto &e : response.entries)
                {
                    auto record = records.find(e.serial);
                    if (record == records.end() || record->second.state != State::Pending)
                        continue;
                    record->second.stamp = now;
                    if (e.status == Status::Missing)
                    {
                        record->second.state = State::Legacy;
                        readySerials.push_back(e.serial);
                    }
                    else if (e.status == Status::Found && e.itemId == record->second.itemId)
                    {
                        record->second.state = State::Ready;
                        record->second.metadata = e.metadata;
                        readySerials.push_back(e.serial);
                    }
                    else
                        record->second.state = State::Error;
                }
                i = batches.erase(i);
            }
            else if (now - i->second.started >= 30000)
            {
                for (auto serial : i->second.serials)
                {
                    auto record = records.find(serial);
                    if (record != records.end())
                    {
                        record->second.state = State::Error;
                        record->second.stamp = now;
                    }
                }
                i = batches.erase(i);
            }
            else
                ++i;
        }
        // Bounded maintenance: retry transient failures even without more item moves.
        auto sweep = records.upper_bound(sweepAfter);
        for (unsigned budget = 0; budget < 256 && !records.empty(); ++budget)
        {
            if (sweep == records.end())
            {
                sweepAfter = 0;
                break;
            }
            sweepAfter = sweep->first;
            auto current = sweep++;
            auto &record = current->second;
            if (record.state == State::Error && !record.poisoned && record.retries < 5 &&
                now - record.stamp >= 5000)
            {
                ++record.retries;
                record.state = State::Queued;
                queue.push_back(current->first);
            }
            else if ((record.state == State::Ready || record.state == State::Legacy ||
                      record.state == State::Error) &&
                     now - record.stamp >= 600000)
                records.erase(current);
        }
        // Bound work per dispatch; subsequent replies drain the remaining queue.
        for (unsigned budget = 0; budget < 4 && !queue.empty() && batches.size() < 128; ++budget)
        {
            Message request;
            request.operation = Operation::Read;
            Batch batch;
            batch.started = now;
            while (!queue.empty() && request.entries.size() < MaxRecords)
            {
                auto serial = queue.front();
                queue.pop_front();
                auto record = records.find(serial);
                if (record == records.end() || record->second.state != State::Queued)
                    continue;
                Entry e;
                e.serial = serial;
                request.entries.push_back(e);
                batch.serials.push_back(serial);
            }
            if (request.entries.empty())
                continue;
            const auto token = backend.Send(request);
            if (!token)
            {
                for (auto serial : batch.serials)
                    queue.push_back(serial);
                break;
            }
            for (auto serial : batch.serials)
                records[serial].state = State::Pending;
            batches.emplace(token, batch);
        }
    }
    // Drain outside the cache lock before recalculating gameplay attributes.
    std::vector<std::uint32_t> TakeReady()
    {
        std::lock_guard<std::mutex> lock(mutex);
        std::vector<std::uint32_t> result;
        result.swap(readySerials);
        return result;
    }
    void Clear()
    {
        std::lock_guard<std::mutex> lock(mutex);
        records.clear();
        queue.clear();
        batches.clear();
        readySerials.clear();
        sweepAfter = 0;
    }
};
} // namespace ExcRankCache
