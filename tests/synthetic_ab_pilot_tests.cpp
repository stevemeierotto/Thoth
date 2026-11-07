/*
 * Copyright (c) 2026 Steve Meierotto
 *
 * Thoth — synthetic EGAR-LX A/B orchestration tests
 *
 * Each CTest invocation runs one case in a fresh process.
 *
 * Licensed under the MIT License (see LICENSE in project root)
 */
#include "../external/basic_agent/include/synthetic_ab_pilot.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
#include <string>
#include <vector>

namespace {

namespace fs = std::filesystem;

void fail(const std::string& message) {
    std::cerr << message << "\n";
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
    const bool ok = report.execution == "mock" && report.frozen_unchanged && report.identities_match
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

int main(int argc, char** argv) {
    if (argc < 2) {
        fail("usage: synthetic_ab_pilot_tests <case> [thoth-engine]");
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
    } else {
        fail("unknown case: " + case_name);
        return 2;
    }
    return ok ? 0 : 1;
}
