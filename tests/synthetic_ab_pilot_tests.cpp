/*
 * Copyright (c) 2026 Steve Meierotto
 *
 * Thoth — synthetic EGAR-LX A/B orchestration tests
 *
 * Each CTest invocation runs one case in a fresh process.
 *
 * Licensed under the MIT License (see LICENSE in project root)
 */
#include "../external/basic_agent/include/alp_sha256.h"
#include "../external/basic_agent/include/json.hpp"
#include "../external/basic_agent/include/memory_snapshot.h"
#include "../external/basic_agent/include/synthetic_ab_pilot.h"

#include <chrono>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include <unistd.h>

namespace {

namespace fs = std::filesystem;

void fail(const std::string& message) {
    std::cerr << message << "\n";
}

std::string fileBytes(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return {};
    }
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

bool freezeHasNoJournal(const fs::path& frozen) {
    return !fs::exists(fs::path(frozen.string() + "-wal"))
        && !fs::exists(fs::path(frozen.string() + "-shm"))
        && !fs::exists(fs::path(frozen.string() + "-journal"));
}

constexpr const char* kDefaultEgarR3PilotRoot = "/home/steve/egar_lx_r3_pilot_v121";

struct EgarR3PilotBundle {
    fs::path pilot_root;
    fs::path retrieval_config;
    std::vector<std::string> acquisition_ids;
    std::vector<std::string> evaluation_ids;
    std::vector<Thoth::SyntheticCase> acquisition;
    std::vector<Thoth::SyntheticCase> evaluation;
};

fs::path resolveEgarR3PilotRoot(const std::string& explicit_root) {
    if (!explicit_root.empty()) {
        return fs::path(explicit_root);
    }
    if (const char* env = std::getenv("EGAR_PILOT_ROOT")) {
        if (env[0] != '\0') {
            return fs::path(env);
        }
    }
    return fs::path(kDefaultEgarR3PilotRoot);
}

bool parseArmOrderSeed(const std::string& text, std::uint64_t& seed, std::string& error) {
    if (text.empty()) {
        error = "arm_order_seed is required";
        return false;
    }
    try {
        std::size_t consumed = 0;
        const unsigned long long value = std::stoull(text, &consumed, 10);
        if (consumed != text.size()) {
            error = "arm_order_seed is invalid";
            return false;
        }
        seed = static_cast<std::uint64_t>(value);
        error.clear();
        return true;
    } catch (const std::exception&) {
        error = "arm_order_seed is invalid";
        return false;
    }
}

bool loadEgarR3PilotBundle(const fs::path& pilot_root,
                           EgarR3PilotBundle& bundle,
                           std::string& error) {
    bundle = EgarR3PilotBundle{};
    bundle.pilot_root = pilot_root;
    const fs::path manifest_path = pilot_root / "PILOT_MANIFEST.json";
    std::ifstream in(manifest_path);
    if (!in) {
        error = "PILOT_MANIFEST.json is missing: " + manifest_path.string();
        return false;
    }
    nlohmann::json body;
    try {
        in >> body;
    } catch (const std::exception& ex) {
        error = std::string("PILOT_MANIFEST.json is invalid JSON: ") + ex.what();
        return false;
    }
    if (!body.is_object()) {
        error = "PILOT_MANIFEST.json root is not an object";
        return false;
    }
    if (body.value("sealed_copied_into_workspace", true)) {
        error = "PILOT_MANIFEST.json reports sealed material inside the workspace";
        return false;
    }
    const auto assignments = body.find("assignments");
    if (assignments == body.end() || !assignments->is_object()) {
        error = "PILOT_MANIFEST.json assignments are missing";
        return false;
    }
    for (const auto& id : (*assignments)["acquisition"]) {
        if (!id.is_string()) {
            error = "PILOT_MANIFEST.json acquisition assignment is invalid";
            return false;
        }
        bundle.acquisition_ids.push_back(id.get<std::string>());
    }
    for (const auto& id : (*assignments)["evaluation"]) {
        if (!id.is_string()) {
            error = "PILOT_MANIFEST.json evaluation assignment is invalid";
            return false;
        }
        bundle.evaluation_ids.push_back(id.get<std::string>());
    }
    if (bundle.acquisition_ids.empty() || bundle.evaluation_ids.empty()) {
        error = "PILOT_MANIFEST.json assignments are empty";
        return false;
    }

    const std::string retrieval_rel = body.value("retrieval_config", "config/retrieval_config.json");
    bundle.retrieval_config = pilot_root / retrieval_rel;
    if (!fs::is_regular_file(bundle.retrieval_config)) {
        error = "pilot retrieval config is missing: " + bundle.retrieval_config.string();
        return false;
    }

    std::unordered_map<std::string, nlohmann::json> by_id;
    const auto cases = body.find("cases");
    if (cases == body.end() || !cases->is_array()) {
        error = "PILOT_MANIFEST.json cases are missing";
        return false;
    }
    for (const auto& row : *cases) {
        const std::string case_id = row.value("case_id", "");
        if (case_id.empty()) {
            error = "PILOT_MANIFEST.json case_id is missing";
            return false;
        }
        by_id.emplace(case_id, row);
    }

    auto loadPartition = [&](const std::vector<std::string>& ids,
                             const std::string& expected_partition,
                             std::vector<Thoth::SyntheticCase>& out) -> bool {
        out.clear();
        for (const std::string& case_id : ids) {
            const auto found = by_id.find(case_id);
            if (found == by_id.end()) {
                error = "PILOT_MANIFEST.json is missing staged case: " + case_id;
                return false;
            }
            const nlohmann::json& row = found->second;
            if (row.value("partition", "") != expected_partition) {
                error = "case partition mismatch for " + case_id;
                return false;
            }
            const std::string evidence_rel = row.value("evidence_relpath", "");
            const std::string goal_rel = row.value("goal_relpath", "");
            const std::string expect_evidence = row.value("evidence_sha256", "");
            const std::string expect_goal = row.value("goal_sha256", "");
            if (evidence_rel.find("sealed") != std::string::npos
                || goal_rel.find("sealed") != std::string::npos
                || evidence_rel.find("Sealed") != std::string::npos
                || goal_rel.find("Sealed") != std::string::npos) {
                error = "refusing sealed path in staged case: " + case_id;
                return false;
            }
            const fs::path evidence_path = pilot_root / evidence_rel;
            const fs::path goal_path = pilot_root / goal_rel;
            if (!fs::is_regular_file(evidence_path) || !fs::is_regular_file(goal_path)) {
                error = "staged files missing for " + case_id;
                return false;
            }
            const std::string evidence_bytes = fileBytes(evidence_path);
            const std::string goal_bytes = fileBytes(goal_path);
            if (evidence_bytes.empty() || goal_bytes.empty()) {
                error = "staged case is empty: " + case_id;
                return false;
            }
            const std::string actual_evidence = Thoth::sha256Hex(evidence_bytes);
            const std::string actual_goal = Thoth::sha256Hex(goal_bytes);
            if (actual_evidence.empty() || actual_goal.empty()) {
                error = "could not hash staged files for " + case_id;
                return false;
            }
            if (actual_evidence != expect_evidence) {
                error = "evidence hash mismatch for " + case_id;
                return false;
            }
            if (actual_goal != expect_goal) {
                error = "goal hash mismatch for " + case_id;
                return false;
            }
            // Verbatim staged short goals — do not wrap or rewrite.
            Thoth::SyntheticCase item;
            item.name = case_id;
            item.evidence_name = "evidence.md";
            item.goal_text = goal_bytes;
            item.evidence_text = evidence_bytes;
            out.push_back(item);
        }
        return true;
    };

    if (!loadPartition(bundle.acquisition_ids, "acquisition", bundle.acquisition)
        || !loadPartition(bundle.evaluation_ids, "evaluation", bundle.evaluation)) {
        return false;
    }
    error.clear();
    return true;
}

fs::path createUniqueEgarR3RunDir(const fs::path& pilot_root, std::string& error) {
    const fs::path runs = pilot_root / "runs";
    std::error_code ec;
    fs::create_directories(runs, ec);
    if (ec) {
        error = "could not create runs directory: " + ec.message();
        return {};
    }
    const fs::path root = runs
        / ("acq_" + std::to_string(static_cast<long long>(::getpid())) + "_"
           + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
                                std::chrono::system_clock::now().time_since_epoch())
                                .count()));
    if (fs::exists(root)) {
        error = "refusing to reuse existing run directory: " + root.string();
        return {};
    }
    fs::create_directories(root, ec);
    if (ec) {
        error = "could not create run directory: " + ec.message();
        return {};
    }
    error.clear();
    return root;
}

void fillEgarR3LiveRequest(Thoth::SyntheticAbRequest& request,
                           const std::string& binary,
                           const EgarR3PilotBundle& bundle,
                           const fs::path& work_root) {
    request.execution = Thoth::SyntheticExecution::Live;
    request.engine_binary = binary;
    request.work_root = work_root.string();
    request.retrieval_config_path = bundle.retrieval_config.string();
    request.inference_backend = "llama_cpp";
    request.inference_base_url = "http://127.0.0.1:8082";
    request.embed_base_url = "http://127.0.0.1:8081";
    request.require_engine_image = false;
    request.engine_timeout_seconds = 900;
}

Thoth::ArmIdentity sampleIdentity() {
    Thoth::ArmIdentity identity;
    identity.engine_binary_sha256 = std::string(64, 'a');
    identity.engine_image_digest = "sha256:abc";
    identity.retrieval_config_sha256 = std::string(64, 'b');
    identity.model_files.push_back({"chat.gguf", std::string(64, 'c')});
    identity.model_files.push_back({"embed.gguf", std::string(64, 'd')});
    identity.evidence_files.push_back({"eval-1/note.txt", std::string(64, 'e')});
    identity.goal_files.push_back({"eval-1", std::string(64, 'f')});
    return identity;
}

bool expectMismatch(const Thoth::ArmIdentity& arm_b, const std::string& needle) {
    std::string error;
    const bool matched = Thoth::armIdentitiesMatch(sampleIdentity(), arm_b, error);
    if (matched || error.find(needle) == std::string::npos) {
        fail("expected " + needle + " but error=" + error);
        return false;
    }
    return true;
}

bool testIdentityMatch() {
    std::string error;
    if (!Thoth::armIdentitiesMatch(sampleIdentity(), sampleIdentity(), error)) {
        fail("identity match failed: " + error);
        return false;
    }
    return true;
}

bool testIdentityDigest() {
    Thoth::ArmIdentity other = sampleIdentity();
    other.engine_image_digest = "sha256:different";
    return expectMismatch(other, "engine image digest mismatch");
}

bool testIdentityModel() {
    Thoth::ArmIdentity other = sampleIdentity();
    other.model_files[0].sha256 = std::string(64, '9');
    return expectMismatch(other, "model hash mismatch");
}

bool testIdentityRetrieval() {
    Thoth::ArmIdentity other = sampleIdentity();
    other.retrieval_config_sha256 = std::string(64, '8');
    return expectMismatch(other, "retrieval configuration mismatch");
}

bool testIdentityEvidence() {
    Thoth::ArmIdentity other = sampleIdentity();
    other.evidence_files[0].sha256 = std::string(64, '7');
    return expectMismatch(other, "evidence hash mismatch");
}

bool testIdentityGoal() {
    Thoth::ArmIdentity other = sampleIdentity();
    other.goal_files[0].sha256 = std::string(64, '6');
    return expectMismatch(other, "goal hash mismatch");
}

bool testExitRequired() {
    const fs::path root = fs::temp_directory_path() / "thoth_lx_ab_exit";
    fs::remove_all(root);
    fs::create_directories(root);
    const fs::path database = root / "memory.db";
    {
        std::ofstream out(database, std::ios::binary);
        out << "source-db";
    }
    const std::string before = "source-db";
    Thoth::EngineProcessStatus live;
    Thoth::MemorySnapshot snapshot;
    std::string error;
    const bool refused = !Thoth::snapshotAfterEngineExit(
        live, database.string(), (root / "frozen").string(), nullptr, snapshot, error);
    std::ifstream after_live(database, std::ios::binary);
    const std::string live_bytes{std::istreambuf_iterator<char>(after_live),
                                 std::istreambuf_iterator<char>()};
    Thoth::EngineProcessStatus failed;
    failed.exited = true;
    failed.exit_code = 1;
    std::string failed_error;
    const bool refused_failed = !Thoth::snapshotAfterEngineExit(
        failed, database.string(), (root / "frozen").string(), nullptr, snapshot, failed_error);
    const bool ok = refused && refused_failed
        && error.find("engine has not exited") != std::string::npos
        && failed_error.find("engine exit failed") != std::string::npos
        && live_bytes == before
        && snapshot.database_path.empty()
        && !fs::exists(root / "frozen" / "memory.db")
        && !fs::exists(fs::path(database.string() + "-wal"));
    if (!ok) {
        fail("exit_required: live=" + error + " failed=" + failed_error);
    }
    fs::remove_all(root);
    return ok;
}

Thoth::SyntheticCase makeCase(const std::string& name, const std::string& marker) {
    Thoth::SyntheticCase item;
    item.name = name;
    item.goal_text = "Synthetic " + marker + " goal for " + name + ".\n";
    item.evidence_name = "note.txt";
    item.evidence_text = "Synthetic " + marker + " evidence for " + name + ".\n";
    return item;
}

bool testPilot(const std::string& binary) {
    const fs::path root = fs::temp_directory_path() / "thoth_lx_ab_pilot";
    fs::remove_all(root);
    fs::create_directories(root / "models");
    {
        std::ofstream chat(root / "models" / "chat.gguf", std::ios::binary);
        chat << "synthetic-chat-model";
        std::ofstream embed(root / "models" / "embed.gguf", std::ios::binary);
        embed << "synthetic-embed-model";
    }
    Thoth::SyntheticAbRequest request;
    request.engine_binary = binary;
    request.work_root = root.string();
    request.retrieval_config_path = "docker/experiment/retrieval_config.json";
    request.model_files = {
        (root / "models" / "chat.gguf").string(),
        (root / "models" / "embed.gguf").string(),
    };
    request.acquisition = {makeCase("acq-1", "acquisition-alpha"), makeCase("acq-2", "acquisition-beta")};
    request.evaluation = {makeCase("eval-1", "evaluation-gamma"), makeCase("eval-2", "evaluation-delta")};
    request.engine_timeout_seconds = 45;

    Thoth::SyntheticAbReport report;
    std::string error;
    const bool ran = Thoth::runSyntheticAbPilot(request, report, error);
    if (!ran) {
        fail("pilot failed: " + error);
        return false;
    }
    const bool orders_legacy =
        !report.arm_order_seeded
        && report.arm_order_procedure == "legacy_fixed_a_then_b"
        && report.evaluation_orders.size() == 2
        && report.evaluation_orders[0].assigned_order == Thoth::ArmRunOrder::AThenB
        && report.evaluation_orders[1].assigned_order == Thoth::ArmRunOrder::AThenB
        && report.evaluation_orders[0].actual_starts.size() == 2
        && report.evaluation_orders[0].actual_starts[0] == "A"
        && report.evaluation_orders[0].actual_starts[1] == "B"
        && report.evaluation_orders[1].actual_starts.size() == 2
        && report.evaluation_orders[1].actual_starts[0] == "A"
        && report.evaluation_orders[1].actual_starts[1] == "B";
    const bool ok = report.execution == "mock"
        && report.phase == "full"
        && report.frozen_unchanged && report.identities_match
        && report.acquisition_sha256.size() == 64
        && report.frozen_sha256_after_evaluation == report.acquisition_sha256
        && report.acquisition_counts.trajectories == 2
        && report.acquisition_counts.past_plans == 2
        && report.empty_experience_sha256.size() == 64
        && !report.engine_image_digest.empty()
        && report.engine_library_sha256.empty()
        && report.service_models.empty()
        && report.arm_a.evidence_files.size() == 2
        && report.arm_b.goal_files.size() == 2
        && fs::is_regular_file(report.report_path)
        && fs::is_regular_file(report.acquisition_manifest_path)
        && orders_legacy;
    std::cout << "ACQUISITION_SHA256 " << report.acquisition_sha256 << "\n";
    std::cout << "ACQUISITION_COUNTS trajectories=" << report.acquisition_counts.trajectories
              << " past_plans=" << report.acquisition_counts.past_plans
              << " strategies=" << report.acquisition_counts.strategies
              << " graph_edges=" << report.acquisition_counts.graph_edges
              << " facts=" << report.acquisition_counts.facts << "\n";
    std::cout << "EMPTY_EXPERIENCE_SHA256 " << report.empty_experience_sha256 << "\n";
    std::cout << "FROZEN_SHA256_AFTER " << report.frozen_sha256_after_evaluation << "\n";
    std::cout << "ENGINE_IMAGE_DIGEST " << report.engine_image_digest << "\n";
    std::cout << "ENGINE_BINARY_SHA256 " << report.arm_a.engine_binary_sha256 << "\n";
    std::cout << "RETRIEVAL_CONFIG_SHA256 " << report.arm_a.retrieval_config_sha256 << "\n";
    if (!ok) {
        fail("pilot report is incomplete");
        return false;
    }
    fs::remove_all(root);
    return true;
}

Thoth::ArmIdentity liveIdentity() {
    Thoth::ArmIdentity identity = sampleIdentity();
    identity.engine_library_sha256 = std::string(64, '1');
    identity.engine_image_digest = "unavailable";
    Thoth::ServiceModelIdentity chat;
    chat.role = "chat";
    chat.id = "/models/chat.gguf";
    chat.n_embd = 3584;
    chat.size = 4677120000LL;
    chat.ftype = "Q4_K - Medium";
    Thoth::ServiceModelIdentity embed;
    embed.role = "embed";
    embed.id = "/models/nomic-embed-text.gguf";
    embed.n_embd = 768;
    embed.size = 83349984;
    embed.ftype = "Q4_K - Medium";
    identity.service_models = {chat, embed};
    return identity;
}

bool testLiveIdentity() {
    std::string error;
    Thoth::ArmIdentity library = liveIdentity();
    library.engine_library_sha256.clear();
    if (Thoth::liveArmIdentitiesMatch(liveIdentity(), library, false, error)
        || error.find("engine library mismatch") == std::string::npos) {
        fail("expected engine library mismatch but error=" + error);
        return false;
    }
    Thoth::ArmIdentity model = liveIdentity();
    model.service_models[0].id.clear();
    if (Thoth::liveArmIdentitiesMatch(liveIdentity(), model, false, error)
        || error.find("service model identity mismatch") == std::string::npos) {
        fail("expected service model identity mismatch but error=" + error);
        return false;
    }
    if (Thoth::liveArmIdentitiesMatch(liveIdentity(), liveIdentity(), true, error)
        || error.find("engine image digest unavailable") == std::string::npos) {
        fail("expected engine image digest unavailable but error=" + error);
        return false;
    }
    Thoth::ArmIdentity files = liveIdentity();
    files.model_files[0].sha256 = std::string(64, '0');
    if (!Thoth::liveArmIdentitiesMatch(liveIdentity(), files, false, error)) {
        fail("live identity treated model files as identity: " + error);
        return false;
    }
    return true;
}

bool testLibraryMissing() {
    std::string path;
    std::string error;
    if (Thoth::resolveLoadedAgentLibrary("/bin/true", path, error) || !path.empty()
        || error.find("loaded agent library") == std::string::npos) {
        fail("expected unresolved library but path=" + path + " error=" + error);
        return false;
    }
    return true;
}

bool testLibraryResolve(const std::string& binary) {
    std::string path;
    std::string error;
    if (!Thoth::resolveLoadedAgentLibrary(binary, path, error)) {
        fail("library resolve failed: " + error);
        return false;
    }
    std::string again;
    std::string again_error;
    const bool ok = path.front() == '/'
        && fs::path(path).filename() == "libbasic_agent.so"
        && fs::is_regular_file(path)
        && Thoth::resolveLoadedAgentLibrary(binary, again, again_error)
        && again == path;
    if (!ok) {
        fail("library resolve is not the loaded file: " + path + " " + again_error);
        return false;
    }
    std::cout << "LOADED_LIBRARY " << path << "\n";
    return true;
}

Thoth::SyntheticCase liveCase(const std::string& name, const std::string& problem, const std::string& evidence) {
    Thoth::SyntheticCase item;
    item.name = name;
    item.evidence_name = "note.txt";
    item.goal_text = "Diagnose this synthetic service failure using only the supplied evidence.\n\nProblem:\n"
        + problem + "\n\nRecommend the next diagnostic action. Do not execute commands.\n";
    item.evidence_text = evidence;
    return item;
}

bool testLivePair(const std::string& binary) {
    const fs::path root = fs::temp_directory_path() / "thoth_lx_ab_live_pair";
    fs::remove_all(root);
    Thoth::SyntheticAbRequest request;
    request.execution = Thoth::SyntheticExecution::Live;
    request.engine_binary = binary;
    request.work_root = root.string();
    request.retrieval_config_path = "docker/experiment/retrieval_config.json";
    request.inference_backend = "llama_cpp";
    request.inference_base_url = "http://127.0.0.1:8080";
    request.embed_base_url = "http://127.0.0.1:8081";
    request.require_engine_image = false;
    request.engine_timeout_seconds = 900;
    request.acquisition = {liveCase(
        "acq-live",
        "The fictional service live-runner-alpha fails to start because its marker file is missing.",
        "Synthetic evidence: live-runner-alpha has no marker file.\n")};
    request.evaluation = {liveCase(
        "eval-live",
        "The fictional service live-runner-beta starts, writes one line, and exits because its state directory is not writable.",
        "Synthetic evidence: live-runner-beta state directory is not writable.\n")};

    Thoth::SyntheticAbReport report;
    std::string error;
    const bool ran = Thoth::runSyntheticAbPilot(request, report, error);
    if (!ran) {
        fail("live pair failed: " + error);
        return false;
    }
    const Thoth::ServiceModelIdentity* chat = nullptr;
    const Thoth::ServiceModelIdentity* embed = nullptr;
    for (const Thoth::ServiceModelIdentity& model : report.service_models) {
        if (model.role == "chat") {
            chat = &model;
        } else if (model.role == "embed") {
            embed = &model;
        }
    }
    const bool ok = report.execution == "live" && report.frozen_unchanged && report.identities_match
        && report.acquisition_sha256.size() == 64
        && report.frozen_sha256_after_evaluation == report.acquisition_sha256
        && report.acquisition_counts.trajectories == 1
        && report.acquisition_counts.past_plans == 1
        && report.empty_experience_sha256.size() == 64
        && report.engine_library_sha256.size() == 64
        && fs::path(report.engine_library_path).filename() == "libbasic_agent.so"
        && report.arm_a.model_files.empty()
        && report.arm_b.model_files.empty()
        && chat != nullptr && embed != nullptr
        && !chat->id.empty() && !embed->id.empty()
        && embed->n_embd == 768
        && chat->id == report.arm_b.service_models[0].id
        && fs::is_regular_file(report.report_path);
    std::cout << "EXECUTION " << report.execution << "\n";
    std::cout << "ENGINE_BINARY_SHA256 " << report.arm_a.engine_binary_sha256 << "\n";
    std::cout << "ENGINE_LIBRARY_PATH " << report.engine_library_path << "\n";
    std::cout << "ENGINE_LIBRARY_SHA256 " << report.engine_library_sha256 << "\n";
    std::cout << "ENGINE_IMAGE_DIGEST " << report.engine_image_digest << "\n";
    std::cout << "ENGINE_IMAGE_VERIFIED " << (report.engine_image_verified ? "true" : "false") << "\n";
    if (chat != nullptr) {
        std::cout << "CHAT_MODEL id=" << chat->id << " n_embd=" << chat->n_embd
                  << " size=" << chat->size << " ftype=" << chat->ftype << "\n";
    }
    if (embed != nullptr) {
        std::cout << "EMBED_MODEL id=" << embed->id << " n_embd=" << embed->n_embd
                  << " size=" << embed->size << " ftype=" << embed->ftype << "\n";
    }
    std::cout << "ACQUISITION_SHA256 " << report.acquisition_sha256 << "\n";
    std::cout << "ACQUISITION_COUNTS trajectories=" << report.acquisition_counts.trajectories
              << " past_plans=" << report.acquisition_counts.past_plans
              << " strategies=" << report.acquisition_counts.strategies
              << " graph_edges=" << report.acquisition_counts.graph_edges
              << " facts=" << report.acquisition_counts.facts << "\n";
    std::cout << "EMPTY_EXPERIENCE_SHA256 " << report.empty_experience_sha256 << "\n";
    std::cout << "FROZEN_SHA256_AFTER " << report.frozen_sha256_after_evaluation << "\n";
    std::cout << "REPORT " << report.report_path << "\n";
    if (!ok) {
        fail("live pair report is incomplete");
        return false;
    }
    return true;
}

bool httpGetOk(const std::string& url, std::string& body, std::string& error) {
    body.clear();
    const std::string command = "curl -sS -m 10 " + url;
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) {
        error = "could not launch curl for " + url;
        return false;
    }
    char buffer[1024];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        body.append(buffer);
    }
    const int status = pclose(pipe);
    if (status != 0 || body.empty()) {
        error = "endpoint health check failed for " + url;
        return false;
    }
    return true;
}

bool testLivePhaseSplit(const std::string& binary) {
    const auto started = std::chrono::steady_clock::now();
    std::string chat_body;
    std::string embed_body;
    std::string error;
    if (!httpGetOk("http://127.0.0.1:8082/v1/models", chat_body, error)
        || !httpGetOk("http://127.0.0.1:8081/v1/models", embed_body, error)) {
        fail("live phase-split preflight: " + error);
        return false;
    }
    if (chat_body.find("/models/chat.gguf") == std::string::npos
        || embed_body.find("/models/nomic-embed-text.gguf") == std::string::npos) {
        fail("live phase-split preflight: unexpected model identity from 8082/8081");
        return false;
    }

    // Unique disposable workspace — never reuse or wipe shared roots.
    const fs::path root = fs::temp_directory_path()
        / ("thoth_lx_ab_live_phase_smoke_" + std::to_string(static_cast<long long>(::getpid()))
           + "_"
           + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
                                std::chrono::system_clock::now().time_since_epoch())
                                .count()));
    if (fs::exists(root)) {
        fail("refusing to reuse existing workspace: " + root.string());
        return false;
    }
    fs::create_directories(root);

    Thoth::SyntheticAbRequest acq;
    acq.execution = Thoth::SyntheticExecution::Live;
    acq.phase = Thoth::SyntheticAbPhase::AcquisitionOnly;
    acq.engine_binary = binary;
    acq.work_root = root.string();
    acq.retrieval_config_path = "docker/experiment/retrieval_config.json";
    acq.inference_backend = "llama_cpp";
    acq.inference_base_url = "http://127.0.0.1:8082";
    acq.embed_base_url = "http://127.0.0.1:8081";
    acq.require_engine_image = false;
    acq.engine_timeout_seconds = 900;
    acq.acquisition = {liveCase(
        "acq-live",
        "The fictional service live-runner-alpha fails to start because its marker file is missing.",
        "Synthetic evidence: live-runner-alpha has no marker file.\n")};
    // Distinct eval material for separation validation during acquisition-only.
    acq.evaluation = {liveCase(
        "eval-live",
        "The fictional service live-runner-beta starts, writes one line, and exits because its state directory is not writable.",
        "Synthetic evidence: live-runner-beta state directory is not writable.\n")};

    Thoth::SyntheticAbReport acq_report;
    if (!Thoth::runSyntheticAbPilot(acq, acq_report, error)) {
        fail("live AcquisitionOnly failed: " + error);
        return false;
    }
    const fs::path frozen = acq_report.frozen_database_path;
    const bool acq_ok = acq_report.execution == "live"
        && acq_report.phase == "acquisition_only"
        && acq_report.acquisition_sha256.size() == 64
        && acq_report.empty_experience_sha256.size() == 64
        && acq_report.acquisition_counts.trajectories == 1
        && acq_report.acquisition_counts.past_plans == 1
        && acq_report.evaluation_orders.empty()
        && !acq_report.identities_match
        && !acq_report.frozen_unchanged
        && acq_report.frozen_sha256_after_evaluation.empty()
        && acq_report.engine_library_sha256.size() == 64
        && fs::path(acq_report.engine_library_path).filename() == "libbasic_agent.so"
        && fs::is_regular_file(acq_report.acquisition_manifest_path)
        && fs::is_regular_file(frozen)
        && fs::is_regular_file(acq_report.empty_template_path)
        && freezeHasNoJournal(frozen)
        && !fs::exists(root / "arm_a")
        && !fs::exists(root / "arm_b");
    if (!acq_ok) {
        fail("live AcquisitionOnly report is incomplete or looks like a finished A/B evaluation");
        return false;
    }
    const std::string frozen_bytes_before = fileBytes(frozen);

    Thoth::SyntheticAbRequest eval;
    eval.execution = Thoth::SyntheticExecution::Live;
    eval.phase = Thoth::SyntheticAbPhase::EvaluationFromFrozen;
    eval.engine_binary = binary;
    eval.work_root = root.string();
    eval.retrieval_config_path = "docker/experiment/retrieval_config.json";
    eval.inference_backend = "llama_cpp";
    eval.inference_base_url = "http://127.0.0.1:8082";
    eval.embed_base_url = "http://127.0.0.1:8081";
    eval.require_engine_image = false;
    eval.engine_timeout_seconds = 900;
    eval.acquisition_manifest_path = acq_report.acquisition_manifest_path;
    eval.arm_order_seed = 42;
    eval.evaluation = {liveCase(
        "eval-live",
        "The fictional service live-runner-beta starts, writes one line, and exits because its state directory is not writable.",
        "Synthetic evidence: live-runner-beta state directory is not writable.\n")};

    Thoth::SyntheticAbReport report;
    if (!Thoth::runSyntheticAbPilot(eval, report, error)) {
        fail("live EvaluationFromFrozen failed: " + error);
        return false;
    }

    const Thoth::ServiceModelIdentity* chat = nullptr;
    const Thoth::ServiceModelIdentity* embed = nullptr;
    for (const Thoth::ServiceModelIdentity& model : report.service_models) {
        if (model.role == "chat") {
            chat = &model;
        } else if (model.role == "embed") {
            embed = &model;
        }
    }
    const bool eval_ok = report.execution == "live"
        && report.phase == "evaluation_from_frozen"
        && report.frozen_unchanged
        && report.identities_match
        && report.acquisition_sha256 == acq_report.acquisition_sha256
        && report.frozen_sha256_after_evaluation == acq_report.acquisition_sha256
        && report.empty_experience_sha256 == acq_report.empty_experience_sha256
        && report.acquisition_counts.trajectories == 1
        && report.acquisition_counts.past_plans == 1
        && report.evaluation_orders.size() == 1
        && report.arm_order_seeded
        && report.arm_order_seed == 42
        && report.arm_order_procedure == "seeded_balanced_shuffle_v1"
        && report.engine_library_sha256 == acq_report.engine_library_sha256
        && fileBytes(frozen) == frozen_bytes_before
        && freezeHasNoJournal(frozen)
        && fs::is_regular_file(root / "verify_resume" / "memory.db")
        && chat != nullptr && embed != nullptr
        && !chat->id.empty() && !embed->id.empty()
        && embed->n_embd == 768
        && fs::is_regular_file(report.report_path);

    const auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                             std::chrono::steady_clock::now() - started)
                             .count();
    std::cout << "WORK_ROOT " << root.string() << "\n";
    std::cout << "ACQUISITION_MANIFEST " << acq_report.acquisition_manifest_path << "\n";
    std::cout << "FROZEN_DATABASE " << frozen.string() << "\n";
    std::cout << "ENGINE_LIBRARY_PATH " << report.engine_library_path << "\n";
    std::cout << "ENGINE_LIBRARY_SHA256 " << report.engine_library_sha256 << "\n";
    std::cout << "ENGINE_IMAGE_DIGEST " << report.engine_image_digest << "\n";
    if (chat != nullptr) {
        std::cout << "CHAT_MODEL id=" << chat->id << " n_embd=" << chat->n_embd
                  << " size=" << chat->size << " ftype=" << chat->ftype << "\n";
    }
    if (embed != nullptr) {
        std::cout << "EMBED_MODEL id=" << embed->id << " n_embd=" << embed->n_embd
                  << " size=" << embed->size << " ftype=" << embed->ftype << "\n";
    }
    std::cout << "ACQUISITION_SHA256 " << report.acquisition_sha256 << "\n";
    std::cout << "EMPTY_EXPERIENCE_SHA256 " << report.empty_experience_sha256 << "\n";
    std::cout << "FROZEN_SHA256_AFTER " << report.frozen_sha256_after_evaluation << "\n";
    std::cout << "ENGINE_LAUNCHES_EXPECTED 7\n";
    std::cout << "ELAPSED_SECONDS " << elapsed << "\n";
    std::cout << "REPORT " << report.report_path << "\n";
    if (!eval_ok) {
        fail("live EvaluationFromFrozen report is incomplete");
        return false;
    }
    return true;
}

bool scheduleBalanced(const std::vector<Thoth::ArmRunOrder>& orders) {
    std::size_t a_then_b = 0;
    std::size_t b_then_a = 0;
    for (const Thoth::ArmRunOrder order : orders) {
        if (order == Thoth::ArmRunOrder::BThenA) {
            ++b_then_a;
        } else {
            ++a_then_b;
        }
    }
    const std::size_t diff = a_then_b > b_then_a ? a_then_b - b_then_a : b_then_a - a_then_b;
    return diff <= 1 && a_then_b + b_then_a == orders.size();
}

bool testArmOrderSchedule() {
    const auto first = Thoth::buildArmOrderSchedule(4, std::optional<std::uint64_t>(42));
    const auto second = Thoth::buildArmOrderSchedule(4, std::optional<std::uint64_t>(42));
    if (first != second) {
        fail("same seed did not reproduce the schedule");
        return false;
    }
    if (!scheduleBalanced(first) || first.size() != 4) {
        fail("even schedule is not balanced");
        return false;
    }
    std::size_t b_count = 0;
    for (const Thoth::ArmRunOrder order : first) {
        if (order == Thoth::ArmRunOrder::BThenA) {
            ++b_count;
        }
    }
    if (b_count != 2) {
        fail("four-case seeded schedule must contain two B_THEN_A assignments");
        return false;
    }
    const auto odd = Thoth::buildArmOrderSchedule(5, std::optional<std::uint64_t>(7));
    if (!scheduleBalanced(odd) || odd.size() != 5) {
        fail("odd schedule is not approximately balanced");
        return false;
    }
    const auto legacy = Thoth::buildArmOrderSchedule(3, std::nullopt);
    if (legacy.size() != 3
        || legacy[0] != Thoth::ArmRunOrder::AThenB
        || legacy[1] != Thoth::ArmRunOrder::AThenB
        || legacy[2] != Thoth::ArmRunOrder::AThenB) {
        fail("unseeded schedule must stay A_THEN_B");
        return false;
    }
    if (std::string(Thoth::armRunOrderLabel(Thoth::ArmRunOrder::AThenB)) != "A_THEN_B"
        || std::string(Thoth::armRunOrderLabel(Thoth::ArmRunOrder::BThenA)) != "B_THEN_A") {
        fail("arm order labels are wrong");
        return false;
    }
    return true;
}

bool testArmOrderBFirst(const std::string& binary) {
    const fs::path root = fs::temp_directory_path() / "thoth_lx_ab_order_b_first";
    fs::remove_all(root);
    Thoth::SyntheticAbRequest request;
    request.engine_binary = binary;
    request.work_root = root.string();
    request.retrieval_config_path = "docker/experiment/retrieval_config.json";
    request.acquisition = {makeCase("acq-order", "acquisition-order")};
    request.evaluation = {
        makeCase("eval-order-1", "evaluation-order-one"),
        makeCase("eval-order-2", "evaluation-order-two"),
    };
    request.engine_timeout_seconds = 45;
    request.arm_order_seed = 42;

    Thoth::SyntheticAbReport report;
    std::string error;
    if (!Thoth::runSyntheticAbPilot(request, report, error)) {
        fail("seeded pilot failed: " + error);
        return false;
    }
    if (!report.arm_order_seeded
        || report.arm_order_seed != 42
        || report.arm_order_procedure != "seeded_balanced_shuffle_v1"
        || report.evaluation_orders.size() != 2) {
        fail("seeded report metadata is incomplete");
        return false;
    }
    const auto expected = Thoth::buildArmOrderSchedule(2, std::optional<std::uint64_t>(42));
    bool saw_b_first = false;
    for (std::size_t i = 0; i < report.evaluation_orders.size(); ++i) {
        const auto& row = report.evaluation_orders[i];
        if (row.assigned_order != expected[i]) {
            fail("assigned order diverged from the seeded schedule");
            return false;
        }
        if (row.actual_starts.size() != 2) {
            fail("successful case did not record two goal starts");
            return false;
        }
        if (row.assigned_order == Thoth::ArmRunOrder::BThenA) {
            saw_b_first = true;
            if (row.actual_starts[0] != "B" || row.actual_starts[1] != "A") {
                fail("B_THEN_A actual starts are wrong");
                return false;
            }
        } else if (row.actual_starts[0] != "A" || row.actual_starts[1] != "B") {
            fail("A_THEN_B actual starts are wrong");
            return false;
        }
    }
    if (!saw_b_first) {
        fail("seeded two-case schedule did not include B_THEN_A");
        return false;
    }
    if (!report.frozen_unchanged
        || report.frozen_sha256_after_evaluation != report.acquisition_sha256
        || report.acquisition_counts.trajectories != 1
        || report.acquisition_counts.past_plans != 1) {
        fail("B-first run damaged the frozen snapshot or acquisition counts");
        return false;
    }
    fs::remove_all(root);
    return true;
}

bool fillCommonRequest(Thoth::SyntheticAbRequest& request, const std::string& binary, const fs::path& root) {
    fs::create_directories(root / "models");
    {
        std::ofstream chat(root / "models" / "chat.gguf", std::ios::binary);
        chat << "synthetic-chat-model";
        std::ofstream embed(root / "models" / "embed.gguf", std::ios::binary);
        embed << "synthetic-embed-model";
    }
    request.engine_binary = binary;
    request.work_root = root.string();
    request.retrieval_config_path = "docker/experiment/retrieval_config.json";
    request.model_files = {
        (root / "models" / "chat.gguf").string(),
        (root / "models" / "embed.gguf").string(),
    };
    request.engine_timeout_seconds = 45;
    return true;
}

bool loadManifestJson(const fs::path& path, nlohmann::json& body, std::string& error) {
    std::ifstream in(path);
    if (!in) {
        error = "manifest missing: " + path.string();
        return false;
    }
    try {
        in >> body;
    } catch (const std::exception& ex) {
        error = std::string("manifest JSON: ") + ex.what();
        return false;
    }
    return true;
}

bool writeManifestJson(const fs::path& path, const nlohmann::json& body, std::string& error) {
    std::ofstream out(path);
    if (!out) {
        error = "manifest write failed: " + path.string();
        return false;
    }
    out << body.dump(2) << "\n";
    return static_cast<bool>(out);
}

bool testAcquisitionOnly(const std::string& binary) {
    const fs::path root = fs::temp_directory_path() / "thoth_lx_ab_acq_only";
    fs::remove_all(root);
    Thoth::SyntheticAbRequest request;
    fillCommonRequest(request, binary, root);
    request.phase = Thoth::SyntheticAbPhase::AcquisitionOnly;
    request.acquisition = {makeCase("acq-1", "acquisition-alpha"), makeCase("acq-2", "acquisition-beta")};
    request.evaluation = {makeCase("eval-1", "evaluation-gamma"), makeCase("eval-2", "evaluation-delta")};

    Thoth::SyntheticAbReport report;
    std::string error;
    if (!Thoth::runSyntheticAbPilot(request, report, error)) {
        fail("acquisition-only failed: " + error);
        return false;
    }
    const bool ok = report.phase == "acquisition_only"
        && report.acquisition_sha256.size() == 64
        && report.empty_experience_sha256.size() == 64
        && report.acquisition_counts.trajectories == 2
        && report.acquisition_counts.past_plans == 2
        && report.evaluation_orders.empty()
        && !report.identities_match
        && !report.frozen_unchanged
        && report.frozen_sha256_after_evaluation.empty()
        && fs::is_regular_file(report.acquisition_manifest_path)
        && fs::is_regular_file(report.frozen_database_path)
        && fs::is_regular_file(report.empty_template_path)
        && fs::is_regular_file(report.report_path)
        && !fs::exists(root / "arm_a")
        && !fs::exists(root / "arm_b");
    std::cout << "ACQUISITION_MANIFEST " << report.acquisition_manifest_path << "\n";
    std::cout << "ACQUISITION_SHA256 " << report.acquisition_sha256 << "\n";
    if (!ok) {
        fail("acquisition-only report is incomplete or looks like a finished A/B evaluation");
        return false;
    }
    // Keep artifacts for chained tests that use the same helper names under other roots.
    fs::remove_all(root);
    return true;
}

bool testEvaluationFromFrozen(const std::string& binary) {
    const fs::path root = fs::temp_directory_path() / "thoth_lx_ab_eval_from_frozen";
    fs::remove_all(root);
    Thoth::SyntheticAbRequest acq;
    fillCommonRequest(acq, binary, root);
    acq.phase = Thoth::SyntheticAbPhase::AcquisitionOnly;
    acq.acquisition = {makeCase("acq-1", "acquisition-alpha")};
    acq.evaluation = {makeCase("eval-1", "evaluation-gamma"), makeCase("eval-2", "evaluation-delta")};
    Thoth::SyntheticAbReport acq_report;
    std::string error;
    if (!Thoth::runSyntheticAbPilot(acq, acq_report, error)) {
        fail("acquisition for eval-from-frozen failed: " + error);
        return false;
    }

    Thoth::SyntheticAbRequest eval;
    fillCommonRequest(eval, binary, root);
    eval.phase = Thoth::SyntheticAbPhase::EvaluationFromFrozen;
    eval.acquisition_manifest_path = acq_report.acquisition_manifest_path;
    eval.acquisition.clear();
    eval.evaluation = {makeCase("eval-1", "evaluation-gamma"), makeCase("eval-2", "evaluation-delta")};
    eval.arm_order_seed = 42;

    Thoth::SyntheticAbReport report;
    if (!Thoth::runSyntheticAbPilot(eval, report, error)) {
        fail("evaluation-from-frozen failed: " + error);
        return false;
    }
    const bool ok = report.phase == "evaluation_from_frozen"
        && report.frozen_unchanged
        && report.identities_match
        && report.acquisition_sha256 == acq_report.acquisition_sha256
        && report.frozen_sha256_after_evaluation == acq_report.acquisition_sha256
        && report.evaluation_orders.size() == 2
        && report.arm_order_seeded
        && report.arm_order_procedure == "seeded_balanced_shuffle_v1"
        && fs::is_regular_file(report.report_path);
    std::cout << "FROZEN_SHA256_AFTER " << report.frozen_sha256_after_evaluation << "\n";
    if (!ok) {
        fail("evaluation-from-frozen report is incomplete");
        return false;
    }
    fs::remove_all(root);
    return true;
}

bool testFrozenTamper(const std::string& binary) {
    const fs::path root = fs::temp_directory_path() / "thoth_lx_ab_frozen_tamper";
    fs::remove_all(root);
    Thoth::SyntheticAbRequest acq;
    fillCommonRequest(acq, binary, root);
    acq.phase = Thoth::SyntheticAbPhase::AcquisitionOnly;
    acq.acquisition = {makeCase("acq-1", "acquisition-alpha")};
    Thoth::SyntheticAbReport acq_report;
    std::string error;
    if (!Thoth::runSyntheticAbPilot(acq, acq_report, error)) {
        fail("acquisition for tamper test failed: " + error);
        return false;
    }
    {
        std::fstream db(acq_report.frozen_database_path, std::ios::in | std::ios::out | std::ios::binary);
        if (!db) {
            fail("could not open frozen database for tamper");
            return false;
        }
        db.seekp(0);
        char byte = 0;
        db.read(&byte, 1);
        db.seekp(0);
        byte = static_cast<char>(byte ^ 0x5A);
        db.write(&byte, 1);
    }

    Thoth::SyntheticAbRequest eval;
    fillCommonRequest(eval, binary, root);
    eval.phase = Thoth::SyntheticAbPhase::EvaluationFromFrozen;
    eval.acquisition_manifest_path = acq_report.acquisition_manifest_path;
    eval.evaluation = {makeCase("eval-1", "evaluation-gamma")};
    Thoth::SyntheticAbReport report;
    const bool ran = Thoth::runSyntheticAbPilot(eval, report, error);
    if (ran || error.find("frozen snapshot hash does not match acquisition manifest") == std::string::npos) {
        fail("tampered freeze should fail hash verification: " + error);
        return false;
    }
    fs::remove_all(root);
    return true;
}

bool testIdentityMismatchFromFrozen(const std::string& binary) {
    const fs::path root = fs::temp_directory_path() / "thoth_lx_ab_identity_mismatch";
    fs::remove_all(root);
    Thoth::SyntheticAbRequest acq;
    fillCommonRequest(acq, binary, root);
    acq.phase = Thoth::SyntheticAbPhase::AcquisitionOnly;
    acq.acquisition = {makeCase("acq-1", "acquisition-alpha")};
    Thoth::SyntheticAbReport acq_report;
    std::string error;
    if (!Thoth::runSyntheticAbPilot(acq, acq_report, error)) {
        fail("acquisition for identity mismatch failed: " + error);
        return false;
    }

    Thoth::SyntheticAbRequest eval;
    fillCommonRequest(eval, binary, root);
    eval.phase = Thoth::SyntheticAbPhase::EvaluationFromFrozen;
    eval.acquisition_manifest_path = acq_report.acquisition_manifest_path;
    eval.evaluation = {makeCase("eval-1", "evaluation-gamma")};
    // Corrupt one model file hash identity without touching the frozen DB.
    {
        std::ofstream chat(root / "models" / "chat.gguf", std::ios::binary | std::ios::trunc);
        chat << "synthetic-chat-model-CHANGED";
    }
    Thoth::SyntheticAbReport report;
    const bool ran = Thoth::runSyntheticAbPilot(eval, report, error);
    if (ran || error.find("model files do not match acquisition manifest") == std::string::npos) {
        fail("changed model bytes should fail identity verification: " + error);
        return false;
    }
    fs::remove_all(root);
    return true;
}

bool testEmptyTemplateTamper(const std::string& binary) {
    const fs::path root = fs::temp_directory_path() / "thoth_lx_ab_empty_template_tamper";
    fs::remove_all(root);
    Thoth::SyntheticAbRequest acq;
    fillCommonRequest(acq, binary, root);
    acq.phase = Thoth::SyntheticAbPhase::AcquisitionOnly;
    acq.acquisition = {makeCase("acq-1", "acquisition-alpha")};
    Thoth::SyntheticAbReport acq_report;
    std::string error;
    if (!Thoth::runSyntheticAbPilot(acq, acq_report, error)) {
        fail("acquisition for empty-template tamper failed: " + error);
        return false;
    }
    const fs::path frozen = acq_report.frozen_database_path;
    const std::string frozen_before = fileBytes(frozen);
    {
        std::fstream db(acq_report.empty_template_path, std::ios::in | std::ios::out | std::ios::binary);
        if (!db) {
            fail("could not open empty template for tamper");
            return false;
        }
        db.seekp(0);
        char byte = 0;
        db.read(&byte, 1);
        db.seekp(0);
        byte = static_cast<char>(byte ^ 0x5A);
        db.write(&byte, 1);
    }

    Thoth::SyntheticAbRequest eval;
    fillCommonRequest(eval, binary, root);
    eval.phase = Thoth::SyntheticAbPhase::EvaluationFromFrozen;
    eval.acquisition_manifest_path = acq_report.acquisition_manifest_path;
    eval.evaluation = {makeCase("eval-1", "evaluation-gamma")};
    Thoth::SyntheticAbReport report;
    const bool ran = Thoth::runSyntheticAbPilot(eval, report, error);
    if (ran
        || error.find("empty experience template hash does not match acquisition manifest")
            == std::string::npos) {
        fail("tampered empty template should fail hash verification: " + error);
        return false;
    }
    if (fileBytes(frozen) != frozen_before || !freezeHasNoJournal(frozen)) {
        fail("empty-template refusal mutated the frozen snapshot");
        return false;
    }
    fs::remove_all(root);
    return true;
}

bool testFrozenCountMismatch(const std::string& binary) {
    const fs::path root = fs::temp_directory_path() / "thoth_lx_ab_frozen_count_mismatch";
    fs::remove_all(root);
    Thoth::SyntheticAbRequest acq;
    fillCommonRequest(acq, binary, root);
    acq.phase = Thoth::SyntheticAbPhase::AcquisitionOnly;
    acq.acquisition = {makeCase("acq-1", "acquisition-alpha")};
    Thoth::SyntheticAbReport acq_report;
    std::string error;
    if (!Thoth::runSyntheticAbPilot(acq, acq_report, error)) {
        fail("acquisition for count mismatch failed: " + error);
        return false;
    }
    nlohmann::json manifest;
    if (!loadManifestJson(acq_report.acquisition_manifest_path, manifest, error)) {
        fail(error);
        return false;
    }
    manifest["acquisition_counts"]["trajectories"] = 99;
    manifest["acquisition_counts"]["past_plans"] = 99;
    if (!writeManifestJson(acq_report.acquisition_manifest_path, manifest, error)) {
        fail(error);
        return false;
    }
    const fs::path frozen = acq_report.frozen_database_path;
    const std::string bytes_before = fileBytes(frozen);

    Thoth::SyntheticAbRequest eval;
    fillCommonRequest(eval, binary, root);
    eval.phase = Thoth::SyntheticAbPhase::EvaluationFromFrozen;
    eval.acquisition_manifest_path = acq_report.acquisition_manifest_path;
    eval.evaluation = {makeCase("eval-1", "evaluation-gamma")};
    Thoth::SyntheticAbReport report;
    const bool ran = Thoth::runSyntheticAbPilot(eval, report, error);
    if (ran || error.find("frozen acquisition counts do not match acquisition manifest")
            == std::string::npos) {
        fail("manifest count mismatch should fail verification: " + error);
        return false;
    }
    if (fileBytes(frozen) != bytes_before || !freezeHasNoJournal(frozen)) {
        fail("count verification mutated the frozen snapshot");
        return false;
    }
    fs::remove_all(root);
    return true;
}

bool testImageDigestRequireUnavailable(const std::string& binary) {
    const fs::path root = fs::temp_directory_path() / "thoth_lx_ab_image_digest_require";
    fs::remove_all(root);
    Thoth::SyntheticAbRequest acq;
    fillCommonRequest(acq, binary, root);
    acq.phase = Thoth::SyntheticAbPhase::AcquisitionOnly;
    acq.acquisition = {makeCase("acq-1", "acquisition-alpha")};
    Thoth::SyntheticAbReport acq_report;
    std::string error;
    if (!Thoth::runSyntheticAbPilot(acq, acq_report, error)) {
        fail("acquisition for image-digest require failed: " + error);
        return false;
    }
    // Force unavailable prior digest so the test does not depend on local Docker state.
    // Default host-binary resume (require_engine_image=false) still succeeds elsewhere.
    nlohmann::json manifest;
    if (!loadManifestJson(acq_report.acquisition_manifest_path, manifest, error)) {
        fail(error);
        return false;
    }
    manifest["engine_image_digest"] = "unavailable";
    if (!writeManifestJson(acq_report.acquisition_manifest_path, manifest, error)) {
        fail(error);
        return false;
    }
    Thoth::SyntheticAbRequest eval;
    fillCommonRequest(eval, binary, root);
    eval.phase = Thoth::SyntheticAbPhase::EvaluationFromFrozen;
    eval.acquisition_manifest_path = acq_report.acquisition_manifest_path;
    eval.evaluation = {makeCase("eval-1", "evaluation-gamma")};
    eval.require_engine_image = true;
    Thoth::SyntheticAbReport report;
    const bool ran = Thoth::runSyntheticAbPilot(eval, report, error);
    if (ran || error.find("engine image digest unavailable") == std::string::npos) {
        fail("require_engine_image should fail when digest is unavailable: " + error);
        return false;
    }
    fs::remove_all(root);
    return true;
}

bool testFrozenCountInspectionReadonly(const std::string& binary) {
    const fs::path root = fs::temp_directory_path() / "thoth_lx_ab_count_inspect_ro";
    fs::remove_all(root);
    Thoth::SyntheticAbRequest acq;
    fillCommonRequest(acq, binary, root);
    acq.phase = Thoth::SyntheticAbPhase::AcquisitionOnly;
    acq.acquisition = {makeCase("acq-1", "acquisition-alpha")};
    Thoth::SyntheticAbReport acq_report;
    std::string error;
    if (!Thoth::runSyntheticAbPilot(acq, acq_report, error)) {
        fail("acquisition for readonly count inspection failed: " + error);
        return false;
    }
    const fs::path frozen = acq_report.frozen_database_path;
    if (!freezeHasNoJournal(frozen)) {
        fail("frozen snapshot already has journal artifacts");
        return false;
    }
    const std::string bytes_before = fileBytes(frozen);

    // Harness resume must inspect counts via copy-out, not by opening the freeze.
    Thoth::SyntheticAbRequest eval;
    fillCommonRequest(eval, binary, root);
    eval.phase = Thoth::SyntheticAbPhase::EvaluationFromFrozen;
    eval.acquisition_manifest_path = acq_report.acquisition_manifest_path;
    eval.evaluation = {makeCase("eval-1", "evaluation-gamma")};
    Thoth::SyntheticAbReport report;
    if (!Thoth::runSyntheticAbPilot(eval, report, error)) {
        fail("evaluation-from-frozen (copy-out count inspect) failed: " + error);
        return false;
    }
    if (fileBytes(frozen) != bytes_before || !freezeHasNoJournal(frozen)
        || report.acquisition_counts.trajectories != 1
        || report.acquisition_counts.past_plans != 1
        || report.frozen_sha256_after_evaluation != acq_report.acquisition_sha256
        || !report.frozen_unchanged) {
        fail("evaluation-from-frozen mutated the frozen snapshot or lost counts");
        return false;
    }
    if (!fs::is_regular_file(root / "verify_resume" / "memory.db")) {
        fail("expected resume verify copy under work_root/verify_resume");
        return false;
    }
    fs::remove_all(root);
    return true;
}

bool testArmOrderIncomplete(const std::string& binary) {
    const fs::path root = fs::temp_directory_path() / "thoth_lx_ab_order_incomplete";
    fs::remove_all(root);
    const fs::path wrapper = root / "fail_b_goal.sh";
    fs::create_directories(root);
    {
        std::ofstream out(wrapper);
        out << "#!/bin/bash\n"
            << "REAL=" << binary << "\n"
            << "args=\"$*\"\n"
            << "if [[ \"$args\" == *\"--fixed-evidence-goal\"* && \"$args\" == *\"/arm_b/\"* ]]; then\n"
            << "  echo \"FIXED_EVIDENCE_GOAL FAILED intentional\"\n"
            << "  exit 1\n"
            << "fi\n"
            << "exec \"$REAL\" \"$@\"\n";
    }
    fs::permissions(wrapper,
                    fs::perms::owner_read | fs::perms::owner_write | fs::perms::owner_exec,
                    fs::perm_options::replace);

    Thoth::SyntheticAbRequest request;
    request.engine_binary = wrapper.string();
    request.work_root = (root / "work").string();
    request.retrieval_config_path = "docker/experiment/retrieval_config.json";
    request.acquisition = {makeCase("acq-incomplete", "acquisition-incomplete")};
    request.evaluation = {
        makeCase("eval-incomplete-1", "evaluation-incomplete-one"),
        makeCase("eval-incomplete-2", "evaluation-incomplete-two"),
    };
    request.engine_timeout_seconds = 45;
    request.arm_order_seed = 42;

    Thoth::SyntheticAbReport report;
    std::string error;
    const bool ran = Thoth::runSyntheticAbPilot(request, report, error);
    if (ran) {
        fail("incomplete B-first launch should fail the pilot");
        return false;
    }
    if (report.evaluation_orders.size() != 1) {
        fail("failed pilot should record exactly the case that began evaluation goals");
        return false;
    }
    // seed 42 with n=2 is balanced. First case may be A_THEN_B or B_THEN_A.
    // A_THEN_B: A succeeds then B fails → actual_starts ["A","B"].
    // B_THEN_A: B fails immediately → actual_starts ["B"].
    const auto expected = Thoth::buildArmOrderSchedule(2, std::optional<std::uint64_t>(42));
    const auto& recorded = report.evaluation_orders.front();
    if (recorded.assigned_order != expected.front()) {
        fail("incomplete run assigned order does not match the seeded schedule");
        return false;
    }
    if (expected.front() == Thoth::ArmRunOrder::BThenA) {
        if (recorded.actual_starts.size() != 1 || recorded.actual_starts[0] != "B") {
            fail("B-first incomplete launch did not record only the B start");
            return false;
        }
    } else if (recorded.actual_starts.size() != 2
               || recorded.actual_starts[0] != "A"
               || recorded.actual_starts[1] != "B") {
        fail("A-first case that fails on B should record A then B starts");
        return false;
    }
    fs::remove_all(root);
    return true;
}

} // namespace

bool testEgarR3LoaderHashes() {
    EgarR3PilotBundle bundle;
    std::string error;
    if (!loadEgarR3PilotBundle(resolveEgarR3PilotRoot(""), bundle, error)) {
        fail("egar_r3_loader_hashes failed: " + error);
        return false;
    }
    if (bundle.acquisition.size() != 4 || bundle.evaluation.size() != 4) {
        fail("unexpected case counts after hash-verified load");
        return false;
    }
    // Fail-closed: a tampered expected hash must refuse.
    const fs::path tmp = fs::temp_directory_path() / "thoth_egar_r3_loader_hash_mismatch";
    fs::remove_all(tmp);
    fs::create_directories(tmp);
    fs::copy_file(bundle.pilot_root / "PILOT_MANIFEST.json", tmp / "PILOT_MANIFEST.json");
    // Point evidence paths at the real pilot files via absolute... simpler: rewrite one sha in copy
    // while still resolving relative to the real pilot by using a shim — use the real root and a
    // temp manifest overlay is awkward. Instead mutate a copied tree pointer:
    nlohmann::json body;
    {
        std::ifstream in(tmp / "PILOT_MANIFEST.json");
        in >> body;
    }
    body["cases"][0]["evidence_sha256"] = std::string(64, '0');
    {
        std::ofstream out(tmp / "PILOT_MANIFEST.json");
        out << body.dump(2) << "\n";
    }
    // Copy staging so relative paths resolve under tmp (or symlink). Use symlinks to real staging.
    fs::create_directories(tmp / "config");
    fs::create_directories(tmp / "staging");
    std::error_code ec;
    fs::create_directory_symlink(bundle.pilot_root / "staging" / "visible",
                                 tmp / "staging" / "visible", ec);
    if (ec) {
        fail("could not link visible staging for mismatch test: " + ec.message());
        return false;
    }
    fs::create_directory_symlink(bundle.pilot_root / "staging" / "goals",
                                 tmp / "staging" / "goals", ec);
    if (ec) {
        fail("could not link goals staging for mismatch test: " + ec.message());
        return false;
    }
    fs::copy_file(bundle.retrieval_config, tmp / "config" / "retrieval_config.json");
    EgarR3PilotBundle bad;
    if (loadEgarR3PilotBundle(tmp, bad, error)
        || error.find("evidence hash mismatch") == std::string::npos) {
        fail("tampered manifest hash should fail closed: " + error);
        return false;
    }
    fs::remove_all(tmp);
    return true;
}

bool testEgarR3LoaderAssignments() {
    EgarR3PilotBundle bundle;
    std::string error;
    if (!loadEgarR3PilotBundle(resolveEgarR3PilotRoot(""), bundle, error)) {
        fail("egar_r3_loader_assignments failed: " + error);
        return false;
    }
    const std::vector<std::string> expect_acq = {"LX-03", "LX-08", "LX-09", "LX-18"};
    const std::vector<std::string> expect_eval = {"LX-10", "LX-13", "LX-16", "LX-20"};
    if (bundle.acquisition_ids != expect_acq || bundle.evaluation_ids != expect_eval) {
        fail("assignment order does not match frozen PILOT_MANIFEST.json");
        return false;
    }
    for (std::size_t i = 0; i < expect_acq.size(); ++i) {
        if (bundle.acquisition[i].name != expect_acq[i]
            || bundle.acquisition[i].evidence_name != "evidence.md") {
            fail("acquisition SyntheticCase ordering/name mismatch");
            return false;
        }
    }
    for (std::size_t i = 0; i < expect_eval.size(); ++i) {
        if (bundle.evaluation[i].name != expect_eval[i]) {
            fail("evaluation SyntheticCase ordering/name mismatch");
            return false;
        }
    }
    return true;
}

bool testEgarR3LoaderMissingManifest() {
    const fs::path missing = fs::temp_directory_path() / "thoth_egar_r3_missing_manifest_root";
    fs::remove_all(missing);
    fs::create_directories(missing);
    EgarR3PilotBundle bundle;
    std::string error;
    if (loadEgarR3PilotBundle(missing, bundle, error)
        || error.find("PILOT_MANIFEST.json is missing") == std::string::npos) {
        fail("missing manifest should fail: " + error);
        return false;
    }
    fs::remove_all(missing);
    return true;
}

bool testEgarR3LoaderInvalidSeed() {
    std::uint64_t seed = 0;
    std::string error;
    if (parseArmOrderSeed("", seed, error) || error.find("required") == std::string::npos) {
        fail("empty arm_order_seed should be rejected");
        return false;
    }
    if (parseArmOrderSeed("not-a-number", seed, error)
        || error.find("invalid") == std::string::npos) {
        fail("non-numeric arm_order_seed should be rejected");
        return false;
    }
    if (parseArmOrderSeed("42xyz", seed, error) || error.find("invalid") == std::string::npos) {
        fail("partial numeric arm_order_seed should be rejected");
        return false;
    }
    if (!parseArmOrderSeed("42", seed, error) || seed != 42) {
        fail("valid arm_order_seed should parse");
        return false;
    }
    return true;
}

bool runEgarR3Acquisition(const std::string& binary, const std::string& pilot_root_arg) {
    std::string chat_body;
    std::string embed_body;
    std::string error;
    if (!httpGetOk("http://127.0.0.1:8082/v1/models", chat_body, error)
        || !httpGetOk("http://127.0.0.1:8081/v1/models", embed_body, error)) {
        fail("egar_r3_acquisition preflight: " + error);
        return false;
    }
    EgarR3PilotBundle bundle;
    if (!loadEgarR3PilotBundle(resolveEgarR3PilotRoot(pilot_root_arg), bundle, error)) {
        fail("egar_r3_acquisition load failed: " + error);
        return false;
    }
    const fs::path run_root = createUniqueEgarR3RunDir(bundle.pilot_root, error);
    if (run_root.empty()) {
        fail(error);
        return false;
    }
    Thoth::SyntheticAbRequest request;
    fillEgarR3LiveRequest(request, binary, bundle, run_root);
    request.phase = Thoth::SyntheticAbPhase::AcquisitionOnly;
    request.acquisition = bundle.acquisition;
    request.evaluation = bundle.evaluation;
    Thoth::SyntheticAbReport report;
    if (!Thoth::runSyntheticAbPilot(request, report, error)) {
        fail("egar_r3_acquisition failed: " + error);
        return false;
    }
    const bool ok = report.phase == "acquisition_only"
        && report.execution == "live"
        && report.acquisition_counts.trajectories
            == static_cast<long long>(bundle.acquisition.size())
        && report.acquisition_counts.past_plans
            == static_cast<long long>(bundle.acquisition.size())
        && !report.identities_match
        && !report.frozen_unchanged
        && report.frozen_sha256_after_evaluation.empty()
        && fs::is_regular_file(report.acquisition_manifest_path)
        && freezeHasNoJournal(report.frozen_database_path);
    std::cout << "PILOT_ROOT " << bundle.pilot_root.string() << "\n";
    std::cout << "WORK_ROOT " << run_root.string() << "\n";
    std::cout << "ACQUISITION_MANIFEST " << report.acquisition_manifest_path << "\n";
    std::cout << "FROZEN_DATABASE " << report.frozen_database_path << "\n";
    std::cout << "ACQUISITION_SHA256 " << report.acquisition_sha256 << "\n";
    std::cout << "EMPTY_EXPERIENCE_SHA256 " << report.empty_experience_sha256 << "\n";
    if (!ok) {
        fail("egar_r3_acquisition report is incomplete");
        return false;
    }
    return true;
}

bool runEgarR3EvaluationFromFrozen(const std::string& binary,
                                   const std::string& manifest_path,
                                   const std::string& seed_text,
                                   const std::string& pilot_root_arg) {
    if (manifest_path.empty()) {
        fail("evaluation requires an explicit acquisition manifest path");
        return false;
    }
    std::uint64_t seed = 0;
    std::string error;
    if (!parseArmOrderSeed(seed_text, seed, error)) {
        fail(error);
        return false;
    }
    if (!fs::is_regular_file(manifest_path)) {
        fail("acquisition manifest path is missing: " + manifest_path);
        return false;
    }
    std::string chat_body;
    std::string embed_body;
    if (!httpGetOk("http://127.0.0.1:8082/v1/models", chat_body, error)
        || !httpGetOk("http://127.0.0.1:8081/v1/models", embed_body, error)) {
        fail("egar_r3_evaluation_from_frozen preflight: " + error);
        return false;
    }
    EgarR3PilotBundle bundle;
    if (!loadEgarR3PilotBundle(resolveEgarR3PilotRoot(pilot_root_arg), bundle, error)) {
        fail("egar_r3_evaluation_from_frozen load failed: " + error);
        return false;
    }
    // Evaluation reuses the acquisition run root (parent of the explicit manifest).
    const fs::path manifest = fs::path(manifest_path);
    const fs::path work_root = manifest.parent_path();
    if (work_root.empty() || !fs::is_directory(work_root)) {
        fail("could not resolve work_root from acquisition manifest path");
        return false;
    }
    Thoth::SyntheticAbRequest request;
    fillEgarR3LiveRequest(request, binary, bundle, work_root);
    request.phase = Thoth::SyntheticAbPhase::EvaluationFromFrozen;
    request.acquisition_manifest_path = manifest.string();
    request.arm_order_seed = seed;
    request.evaluation = bundle.evaluation;
    Thoth::SyntheticAbReport report;
    if (!Thoth::runSyntheticAbPilot(request, report, error)) {
        fail("egar_r3_evaluation_from_frozen failed: " + error);
        return false;
    }
    const bool ok = report.phase == "evaluation_from_frozen"
        && report.execution == "live"
        && report.frozen_unchanged
        && report.identities_match
        && report.arm_order_seeded
        && report.arm_order_seed == seed
        && report.evaluation_orders.size() == bundle.evaluation.size()
        && freezeHasNoJournal(report.frozen_database_path);
    std::cout << "PILOT_ROOT " << bundle.pilot_root.string() << "\n";
    std::cout << "WORK_ROOT " << work_root.string() << "\n";
    std::cout << "ACQUISITION_MANIFEST " << manifest.string() << "\n";
    std::cout << "ARM_ORDER_SEED " << seed << "\n";
    std::cout << "ACQUISITION_SHA256 " << report.acquisition_sha256 << "\n";
    std::cout << "FROZEN_SHA256_AFTER " << report.frozen_sha256_after_evaluation << "\n";
    std::cout << "REPORT " << report.report_path << "\n";
    if (!ok) {
        fail("egar_r3_evaluation_from_frozen report is incomplete");
        return false;
    }
    return true;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        fail("usage: synthetic_ab_pilot_tests <case> [args...]");
        return 2;
    }
    const std::string case_name = argv[1];
    bool ok = false;
    if (case_name == "identity_match") {
        ok = testIdentityMatch();
    } else if (case_name == "identity_digest") {
        ok = testIdentityDigest();
    } else if (case_name == "identity_model") {
        ok = testIdentityModel();
    } else if (case_name == "identity_retrieval") {
        ok = testIdentityRetrieval();
    } else if (case_name == "identity_evidence") {
        ok = testIdentityEvidence();
    } else if (case_name == "identity_goal") {
        ok = testIdentityGoal();
    } else if (case_name == "exit_required") {
        ok = testExitRequired();
    } else if (case_name == "pilot") {
        if (argc < 3) {
            fail("pilot requires the thoth-engine path");
            return 2;
        }
        ok = testPilot(argv[2]);
    } else if (case_name == "live_identity") {
        ok = testLiveIdentity();
    } else if (case_name == "library_missing") {
        ok = testLibraryMissing();
    } else if (case_name == "library_resolve") {
        if (argc < 3) {
            fail("library_resolve requires the thoth-engine path");
            return 2;
        }
        ok = testLibraryResolve(argv[2]);
    } else if (case_name == "live_pair") {
        if (argc < 3) {
            fail("live_pair requires the thoth-engine path");
            return 2;
        }
        ok = testLivePair(argv[2]);
    } else if (case_name == "live_phase_split") {
        if (argc < 3) {
            fail("live_phase_split requires the thoth-engine path");
            return 2;
        }
        ok = testLivePhaseSplit(argv[2]);
    } else if (case_name == "arm_order_schedule") {
        ok = testArmOrderSchedule();
    } else if (case_name == "arm_order_b_first") {
        if (argc < 3) {
            fail("arm_order_b_first requires the thoth-engine path");
            return 2;
        }
        ok = testArmOrderBFirst(argv[2]);
    } else if (case_name == "arm_order_incomplete") {
        if (argc < 3) {
            fail("arm_order_incomplete requires the thoth-engine path");
            return 2;
        }
        ok = testArmOrderIncomplete(argv[2]);
    } else if (case_name == "acquisition_only") {
        if (argc < 3) {
            fail("acquisition_only requires the thoth-engine path");
            return 2;
        }
        ok = testAcquisitionOnly(argv[2]);
    } else if (case_name == "evaluation_from_frozen") {
        if (argc < 3) {
            fail("evaluation_from_frozen requires the thoth-engine path");
            return 2;
        }
        ok = testEvaluationFromFrozen(argv[2]);
    } else if (case_name == "frozen_tamper") {
        if (argc < 3) {
            fail("frozen_tamper requires the thoth-engine path");
            return 2;
        }
        ok = testFrozenTamper(argv[2]);
    } else if (case_name == "identity_mismatch_from_frozen") {
        if (argc < 3) {
            fail("identity_mismatch_from_frozen requires the thoth-engine path");
            return 2;
        }
        ok = testIdentityMismatchFromFrozen(argv[2]);
    } else if (case_name == "empty_template_tamper") {
        if (argc < 3) {
            fail("empty_template_tamper requires the thoth-engine path");
            return 2;
        }
        ok = testEmptyTemplateTamper(argv[2]);
    } else if (case_name == "frozen_count_mismatch") {
        if (argc < 3) {
            fail("frozen_count_mismatch requires the thoth-engine path");
            return 2;
        }
        ok = testFrozenCountMismatch(argv[2]);
    } else if (case_name == "image_digest_require_unavailable") {
        if (argc < 3) {
            fail("image_digest_require_unavailable requires the thoth-engine path");
            return 2;
        }
        ok = testImageDigestRequireUnavailable(argv[2]);
    } else if (case_name == "frozen_count_inspection_readonly") {
        if (argc < 3) {
            fail("frozen_count_inspection_readonly requires the thoth-engine path");
            return 2;
        }
        ok = testFrozenCountInspectionReadonly(argv[2]);
    } else if (case_name == "egar_r3_loader_hashes") {
        ok = testEgarR3LoaderHashes();
    } else if (case_name == "egar_r3_loader_assignments") {
        ok = testEgarR3LoaderAssignments();
    } else if (case_name == "egar_r3_loader_missing_manifest") {
        ok = testEgarR3LoaderMissingManifest();
    } else if (case_name == "egar_r3_loader_invalid_seed") {
        ok = testEgarR3LoaderInvalidSeed();
    } else if (case_name == "egar_r3_acquisition") {
        // Live operator entry: <engine> [pilot_root]
        if (argc < 3) {
            fail("usage: egar_r3_acquisition <thoth-engine> [pilot_root]");
            return 2;
        }
        ok = runEgarR3Acquisition(argv[2], argc >= 4 ? argv[3] : "");
    } else if (case_name == "egar_r3_evaluation_from_frozen") {
        // Live operator entry requires explicit manifest + seed; never auto-selects a run.
        // usage: egar_r3_evaluation_from_frozen <engine> <manifest> <arm_order_seed> [pilot_root]
        if (argc < 5) {
            fail("usage: egar_r3_evaluation_from_frozen <thoth-engine> <acquisition_manifest> "
                 "<arm_order_seed> [pilot_root]");
            return 2;
        }
        ok = runEgarR3EvaluationFromFrozen(
            argv[2], argv[3], argv[4], argc >= 6 ? argv[5] : "");
    } else {
        fail("unknown case: " + case_name);
        return 2;
    }
    return ok ? 0 : 1;
}
