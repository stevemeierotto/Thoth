/*
 * Copyright (c) 2025 Steve Meierotto
 *
 * Thoth — ALP-E local note file helpers for create/intent POST (GUI layer; pure)
 *
 * Licensed under the MIT License (see LICENSE in project root)
 */
#ifndef THOTH_CORPUS_CREATE_LOCAL_H
#define THOTH_CORPUS_CREATE_LOCAL_H

#include "alp_sha256.h"
#include "corpus_create.h"
#include "engine_error.h"
#include "operation_result.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>

namespace Thoth {
namespace CorpusCreateLocal {

struct LocalNoteFilePayload {
    std::string content;
    std::string suggested_name;
    std::string content_hash;
    std::int64_t local_source_mtime_sec = 0;
};

inline std::optional<LocalNoteFilePayload> readLocalNoteFile(const std::string& source_path,
                                                             std::string& error_out) {
    error_out.clear();
    if (source_path.empty()) {
        error_out = "empty source path";
        return std::nullopt;
    }
    std::error_code ec;
    if (!std::filesystem::exists(source_path, ec)) {
        error_out = "Local Note file not found";
        return std::nullopt;
    }
    std::ifstream in(source_path, std::ios::binary);
    if (!in) {
        error_out = "Could not read Local Note";
        return std::nullopt;
    }
    std::ostringstream buffer;
    buffer << in.rdbuf();
    LocalNoteFilePayload payload;
    payload.content = buffer.str();
    if (payload.content.empty()) {
        error_out = "Local Note is empty";
        return std::nullopt;
    }
    payload.suggested_name = std::filesystem::path(source_path).filename().string();
    payload.content_hash = Thoth::sha256Hex(payload.content);
    std::error_code mtime_ec;
    const auto ftime = std::filesystem::last_write_time(source_path, mtime_ec);
    if (!mtime_ec) {
        // file_time_type does not use the Unix epoch on every standard library.
        // clock_cast yields system_clock time without assuming the clocks match.
        const auto system_time =
            std::chrono::clock_cast<std::chrono::system_clock>(ftime);
        payload.local_source_mtime_sec = static_cast<std::int64_t>(
            std::chrono::duration_cast<std::chrono::seconds>(system_time.time_since_epoch())
                .count());
    }
    return payload;
}

inline CorpusCreate::CreateDocumentRequest makeCreateRequest(
    const LocalNoteFilePayload& payload,
    const std::string& owner_context_id,
    const std::string& local_source_path,
    const CorpusCreateGuiOptions& options,
    bool dry_run) {
    CorpusCreate::CreateDocumentRequest req;
    req.suggested_name = payload.suggested_name;
    req.content = payload.content;
    req.owner_context_id = owner_context_id;
    req.content_hash = payload.content_hash;
    req.local_source_mtime_sec = payload.local_source_mtime_sec;
    req.local_source_path = local_source_path;
    req.force_replace = options.force_replace;
    req.dry_run = dry_run;
    return req;
}

inline std::string parseHttpErrorMachineCode(const std::string& body) {
    if (body.empty()) {
        return {};
    }
    try {
        const auto j = nlohmann::json::parse(body);
        if (j.contains("error") && j["error"].is_object()) {
            return j["error"].value("machine_code", std::string{});
        }
    } catch (...) {
    }
    return {};
}

inline OperationResult operationResultFromJsonBody(const nlohmann::json& body,
                                                   const std::string& host_path,
                                                   bool dry_run) {
    std::string err;
    if (!CorpusCreate::hasRequiredAcceptedFields(body, err)) {
        return makeFailure(CorpusCreate::kOperationName,
                           dry_run ? "Could not query send intent"
                                   : "Document acceptance response invalid",
                           err);
    }

    if (dry_run || body.value("dry_run", false)) {
        auto result = makeSuccess(kOpQueryDocumentIntent,
                                  "Send intent resolved");
        result.ingest_host_path = host_path;
        result.ingest_action = body["action"].get<std::string>();
        if (body.contains("reason") && body["reason"].is_string()) {
            result.ingest_reason = body["reason"].get<std::string>();
        }
        if (body.contains("document") && body["document"].is_object()) {
            const auto& doc = body["document"];
            if (doc.contains("id") && doc["id"].is_string()) {
                result.ingest_document_id = doc["id"].get<std::string>();
            }
            if (doc.contains("name") && doc["name"].is_string()) {
                result.ingest_document_name = doc["name"].get<std::string>();
            }
        }
        return result;
    }

    const std::string doc_name = body["document"]["name"].get<std::string>();
    const std::string doc_id = body["document"]["id"].get<std::string>();
    auto result = makeSuccess(CorpusCreate::kOperationName, "Document accepted: " + doc_name);
    result.ingest_host_path = host_path;
    result.ingest_document_id = doc_id;
    result.ingest_document_name = doc_name;
    if (body.contains("action") && body["action"].is_string()) {
        result.ingest_action = body["action"].get<std::string>();
    }
    if (body["document"].contains("revision_id")
        && body["document"]["revision_id"].is_string()) {
        result.ingest_revision_id = body["document"]["revision_id"].get<std::string>();
    }
    return result;
}

inline OperationResult operationResultFromEngineException(const EngineException& ex,
                                                          const std::string& host_path) {
    const EngineError& err = ex.error();
    const long http_status = engineErrorHttpStatus(err.code);
    auto result = makeFailure(CorpusCreate::kOperationName,
                              "Document could not be sent to Engine",
                              err.message,
                              err.code == EngineErrorCode::ENGINE_BUSY || http_status >= 500,
                              http_status);
    result.ingest_host_path = host_path;
    if (!err.machine_code.empty()) {
        result.ingest_machine_code = err.machine_code;
        if (err.machine_code == "content_conflict") {
            result.ingest_content_conflict = true;
        }
    }
    if (err.details.contains("document_id") && err.details["document_id"].is_string()) {
        result.ingest_document_id = err.details["document_id"].get<std::string>();
    }
    return result;
}

} // namespace CorpusCreateLocal
} // namespace Thoth

#endif // THOTH_CORPUS_CREATE_LOCAL_H
