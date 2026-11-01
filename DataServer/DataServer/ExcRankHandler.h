#pragma once
#include "../../Utilities/ExcRank/ExcRankConfig.h"
#include "../../Utilities/ExcRank/ExcRankProtocol.h"
#include "../../Utilities/IndependentTierControl.h"
#include "ExcRankStore.h"
#include "ServerManager.h"
#include "SocketManager.h"
namespace ExcRankHandler
{
inline void Receive(int index, const BYTE *bytes, int size)
{
    const bool enabled = IndependentTierControl::Enabled();
    if (!enabled || !SERVER_RANGE(index) || !gServerManager[index].CheckState() || size < 0)
        return;
    using namespace ExcRankProtocol;
    Message request;
    if (!Decode(bytes, static_cast<std::size_t>(size), request) || Response(request.operation))
        return;
    Message reply;
    reply.requestId = request.requestId;
    reply.operation = request.operation == Operation::Read ? Operation::ReadResult : Operation::CreateResult;
    for (const auto &in : request.entries)
    {
        Entry out;
        out.serial = in.serial;
        out.status = Status::Error;
        if (request.operation == Operation::Read)
        {
            ExcRankStore::Record record;
            const auto status = ExcRankStore::Read(gQueryManager, in.serial, record);
            if (status == ExcRankStore::Status::Missing)
                out.status = Status::Missing;
            else if (status == ExcRankStore::Status::Found)
            {
                out.status = Status::Found;
                out.itemId = static_cast<std::uint16_t>(record.originalItemId);
                out.metadata = record.metadata;
            }
        }
        else
        {
            const auto cfg = ExcRank::Policy();
            const auto status = cfg->enabled && ExcRank::ValidatePolicy(in.metadata, in.itemId, *cfg)
                                    ? ExcRankStore::Create(gQueryManager, in.serial, in.itemId, in.metadata)
                                    : ExcRankStore::Status::Error;
            if (status == ExcRankStore::Status::Created)
                out.status = Status::Created;
            else if (status == ExcRankStore::Status::Existing)
                out.status = Status::Existing;
            else if (status == ExcRankStore::Status::Conflict)
                out.status = Status::Conflict;
        }
        reply.entries.push_back(out);
    }
    Bytes response;
    if (Encode(reply, response))
        gSocketManager.DataSend(index, response.data(), static_cast<int>(response.size()));
}
} // namespace ExcRankHandler
