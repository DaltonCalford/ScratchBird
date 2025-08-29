#include "scratchbird/engine/ddl/tablespace_ddl.h"

#include "scratchbird/engine/config.h"
#include "scratchbird/engine/tablespace_manager.h"

#include <algorithm>

namespace scratchbird::engine
{

    bool execute_tablespace_ddl(const Ast& ast, const std::string& db_path, std::string& error)
    {
        error.clear();
        const auto& cfg = get_engine_config();
        if (!cfg.tablespaces_enabled)
            return true; // feature disabled; accept but no-op
        if (ast.kind != NodeKind::DdlTablespace)
            return false;
        auto action = ast.ddlTablespace.action;
        std::transform(action.begin(), action.end(), action.begin(),
                       [](unsigned char c) { return char(std::tolower(c)); });
        TablespaceManager tsm(db_path, /*page_size*/ 4096);
        tsm.load_from_catalog();
        if (action == "create") {
            // Basic minimal create: register in catalog; LOCATION parsing is deferred
            TablespaceSpec spec{};
            spec.id = 1; // placeholder; real id assignment in later milestones
            spec.name = ast.ddlTablespace.name;
            spec.state = TablespaceState::Online;
            tsm.upsert(spec);
            if (!tsm.persist_to_catalog(spec)) {
                error = "Failed to persist tablespace";
                return false;
            }
            return true;
        } else if (action == "alter") {
            // Placeholder: accept SET options no-op
            return true;
        } else if (action == "drop") {
            // Placeholder: refuse dropping default; otherwise accept (no objects yet)
            auto nm = ast.ddlTablespace.name;
            std::string up = nm;
            std::transform(up.begin(), up.end(), up.begin(), ::toupper);
            if (up == "SDB$DEFAULT") {
                error = "Cannot drop default tablespace";
                return false;
            }
            return true;
        } else if (action == "detach" || action == "attach") {
            // Future milestones
            return true;
        }
        return false;
    }

} // namespace scratchbird::engine
