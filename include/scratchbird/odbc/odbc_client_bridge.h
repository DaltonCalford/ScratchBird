#pragma once

#include <string>
#include <vector>

#include "scratchbird/core/status.h"
#include "scratchbird/odbc/odbc_types.h"
#include "scratchbird/client/network_client.h"

namespace scratchbird {
namespace odbc {

class OdbcClientBridge {
public:
    OdbcClientBridge();
    virtual ~OdbcClientBridge();

    virtual SQLRETURN connect(const ConnectionParams& params, std::string& error);
    virtual void disconnect();
    virtual bool isConnected() const;
    core::Status lastStatus() const { return last_status_; }
    const std::string& lastError() const { return last_error_; }

    virtual SQLRETURN executeSQL(const std::string& sql,
                                 std::vector<std::vector<std::string>>& results,
                                 std::vector<ColumnMetadata>& columns,
                                 SQLLEN& rows_affected);

    virtual SQLRETURN beginTransaction();
    virtual SQLRETURN commit();
    virtual SQLRETURN rollback();

private:
    static client::NetworkClientConfig buildConfig(const ConnectionParams& params);
    static ColumnMetadata mapColumn(const client::NetworkColumn& col);
    static SQLSMALLINT mapWireType(protocol::WireType type);
    static std::string stringifyValue(const protocol::ProtocolCodec::ColumnValue& val,
                                      protocol::WireType type);

    client::NetworkClient client_;
    core::Status last_status_{core::Status::OK};
    std::string last_error_;
};

} // namespace odbc
} // namespace scratchbird
