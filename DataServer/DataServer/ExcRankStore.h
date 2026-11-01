#pragma once
#include "../../Utilities/ExcRank/ExcRankCore.h"
#include "QueryManager.h"
#include "Util.h"
// Must run on the DataServer database execution thread, with no active query.
// Not registered in network protocol yet: client requests must never call Create.
namespace ExcRankStore
{
enum class Status
{
    Error = -1,
    Missing = 0,
    Created = 1,
    Existing = 2,
    Conflict = 3,
    Found = 4
};
struct Record
{
    int originalItemId = 0;
    ExcRank::ItemOptions metadata;
};
struct CloseQuery
{
    CQueryManager &query;
    ~CloseQuery()
    {
        query.Close();
    }
};
inline bool Row(SQLRETURN result)
{
    return result == SQL_SUCCESS || result == SQL_SUCCESS_WITH_INFO;
}
inline Status Read(CQueryManager &query, std::uint32_t serial, Record &output)
{
    if (!serial)
        return Status::Error;
    CloseQuery close = {query};
    if (!query.ExecQuery("EXEC dbo.NeonExcRankRead @ItemSerial=%I64u", static_cast<unsigned __int64>(serial)))
        return Status::Error;
    if (!Row(query.Fetch()))
        return Status::Error;
    const int status = query.GetAsInteger("TierStatus");
    if (status == 0)
        return Status::Missing;
    if (status != 1)
        return Status::Error;
    const int size = query.GetAsInteger("PayloadSize");
    const int item = query.GetAsInteger("ItemID");
    if (item < 0 || item > 8191)
        return Status::Error;
    if (size < 3 || size > 17)
    {
        output = {};
        output.originalItemId = item;
        return Status::Found;
    }
    BYTE bytes[17] = {0};
    query.GetAsBinary("Metadata", bytes, size);
    Record result;
    result.originalItemId = item;
    if (!ExcRank::Decode(bytes, static_cast<std::size_t>(size), result.metadata))
    {
        result.metadata = {};
        LogAdd(LOG_RED, "[ExcRank] Invalid blob ignored, serial=%lu", static_cast<unsigned long>(serial));
    }
    output = result;
    return Status::Found;
}
inline Status Create(CQueryManager &query, std::uint32_t serial, int itemId,
                     const ExcRank::ItemOptions &metadata)
{
    if (!serial || itemId < 0 || itemId > 8191)
        return Status::Error;
    std::vector<std::uint8_t> bytes;
    if (!ExcRank::Encode(metadata, bytes))
        return Status::Error;
    // Fixed alphabet generated from validated bytes: no interpolated user text.
    const char digits[] = "0123456789ABCDEF";
    char hex[263] = {0};
    for (std::size_t i = 0; i < bytes.size(); ++i)
    {
        hex[2 * i] = digits[bytes[i] >> 4];
        hex[2 * i + 1] = digits[bytes[i] & 15];
    }
    CloseQuery close = {query};
    if (!query.ExecQuery("EXEC dbo.NeonExcRankCreate @ItemSerial=%I64u,@ItemID=%d,@Metadata=0x%s",
                         static_cast<unsigned __int64>(serial), itemId, hex))
        return Status::Error;
    if (!Row(query.Fetch()))
        return Status::Error;
    switch (query.GetAsInteger("TierStatus"))
    {
    case 1:
        return Status::Created;
    case 2:
        return Status::Existing;
    case 3:
        return Status::Conflict;
    default:
        return Status::Error;
    }
}
} // namespace ExcRankStore
