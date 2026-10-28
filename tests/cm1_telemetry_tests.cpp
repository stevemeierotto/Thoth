/*
 * Copyright (c) 2026 Steve Meierotto
 *
 * Thoth — CM-1 Phase 1 telemetry unit tests (disposable databases only)
 *
 * Licensed under the MIT License (see LICENSE in project root)
 */

#include "cm1_telemetry.h"
#include "alp_sha256.h"
#include "executive_controller.h"
#include "generation_call.h"
#include "grag_diagnostics.h"
#include "memory.h"
#include "sqlite_memory_repository.h"

#include <json.hpp>
#include <sqlite3.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <unistd.h>
#include <vector>

namespace fs = std::filesystem;

namespace {

int g_failures = 0;

void expect(bool cond, const std::string& name) {
    if (!cond) {
        std::cerr << "FAIL: " << name << "\n";
        ++g_failures;
    } else {
        std::cout << "PASS: " << name << "\n";
    }
}

fs::path makeTempDb() {
    const auto dir = fs::temp_directory_path() / ("thoth_cm1_" + std::to_string(::getpid()) + "_"
                                                  + std::to_string(std::chrono::steady_clock::now()
                                                                       .time_since_epoch()
                                                                       .count()));
    fs::create_directories(dir);
    return dir / "memory.db";
}

Thoth::MemoryRepository::ActivePlanRecord activePlan(const std::string& plan_id) {
    Thoth::MemoryRepository::ActivePlanRecord rec;
    rec.plan_id = plan_id;
    rec.session_id = "s1";
    rec.goal = "goal";
    rec.steps_json = "{}";
    rec.current_index = 0;
    rec.controller_state = "IDLE";
    rec.created_at_ms = 1;
    rec.updated_at_ms = 1;
    return rec;
}

Thoth::MemoryRepository::EpisodeStepRecord episode(const std::string& plan_id, int index) {
    Thoth::MemoryRepository::EpisodeStepRecord ep;
    ep.episode_id = "ep-" + plan_id;
    ep.goal_id = plan_id;
    ep.step_index = index;
    ep.state_summary = "summary";
    ep.action_taken = "action";
    ep.result_status = "SUCCESS";
    ep.timestamp_ms = 1;
    return ep;
}

GragDiagnostics makeDiagTwoFiles() {
    GragDiagnostics d;
    d.chunks_retrieved = 3;
    d.chunks_reranked = 3;
    d.alpha = 0.4f;
    d.routing_mode = "PLAN_AWARE";
    d.scoring_type = "grag_blended_hybrid";
    d.graph_activations = 1;
    d.graph_max_contribution = 0.2f;
    d.final_scores = {0.9f, 0.8f, 0.7f};

    ScoreBreakdown a;
    a.file_name = "/corpus/doc_a.md";
    a.code_text = "alpha chunk one";
    a.final_score = 0.9f;
    a.graph_score = 0.2f;
    a.query_sim = 0.5f;

    ScoreBreakdown a2;
    a2.file_name = "/corpus/doc_a.md";
    a2.code_text = "alpha chunk one"; // duplicate full content
    a2.final_score = 0.8f;
    a2.query_sim = 0.25f;

    ScoreBreakdown b;
    b.file_name = "/corpus/doc_b.md";
    b.code_text = "beta chunk";
    b.final_score = 0.7f;
    b.query_sim = 0.75f;

    d.breakdowns = {a, a2, b};
    return d;
}

bool testSourceGrouping() {
    const GragDiagnostics d = makeDiagTwoFiles();
    Thoth::Cm1SnapshotBuildInput input;
    input.plan_id = "p1";
    input.step_id = "r1";
    input.snapshot_type = "retrieval_observed";
    input.lifecycle_stage = "after_retrieval";
    input.observed_at_ms = 10;
    input.diagnostics = &d;
    input.indexed_counts_available = true;
    input.indexed_counts_by_file["/corpus/doc_a.md"] = 10;
    input.indexed_counts_by_file["/corpus/doc_b.md"] = 4;
    input.selected_chunks = {{"/corpus/doc_a.md", "alpha chunk one"},
                             {"/corpus/doc_b.md", "beta chunk"}};

    const auto snap = Thoth::buildCm1Snapshot(input);
    expect(snap.measurement_status == "complete", "grouping: status complete");
    expect(snap.estimator_status == Thoth::kCm1EstimatorStatus, "grouping: estimator none_active");
    const auto features = nlohmann::json::parse(snap.features_json);
    expect(features["agreement_status"] == "not_evaluated", "grouping: agreement not_evaluated");
    expect(features["source_groups"].is_array() && features["source_groups"].size() == 2,
           "grouping: two source groups");
    bool sawA = false;
    bool sawB = false;
    for (const auto& g : features["source_groups"]) {
        if (g["file_name"] == "/corpus/doc_a.md") {
            sawA = true;
            expect(g["pool_chunk_count"] == 2, "grouping: doc_a pool 2");
            expect(g["selected_chunk_count"] == 1, "grouping: doc_a selected 1");
            expect(g["indexed_chunk_count"] == 10, "grouping: doc_a indexed 10");
            expect(g["duplicate_full_content_hash_count"] == 1, "grouping: doc_a duplicate 1");
            expect(g["origin_id"] == "unknown", "grouping: origin unknown");
            expect(g.contains("graph_score_max"), "grouping: graph meta present");
            expect(g["graph_independent_evidence"] == false, "grouping: graph not independent");
            expect(!g.contains("coverage_ratio"), "grouping: no coverage_ratio");
            expect(g["highest_query_sim_status"] == "available", "grouping: query_sim available");
            expect(std::fabs(g["highest_query_sim"].get<float>() - 0.5f) < 1e-5f,
                   "grouping: doc_a keeps max query_sim, not a chunk sum");
        }
        if (g["file_name"] == "/corpus/doc_b.md") {
            sawB = true;
            expect(g["pool_chunk_count"] == 1, "grouping: doc_b pool 1");
        }
    }
    expect(sawA && sawB, "grouping: both files present");
    return g_failures == 0;
}

bool testMissingBreakdownsAndDenominator() {
    Thoth::Cm1SnapshotBuildInput input;
    input.plan_id = "p2";
    input.step_id = "r1";
    input.snapshot_type = "retrieval_observed";
    input.lifecycle_stage = "after_retrieval";
    input.observed_at_ms = 11;
    GragDiagnostics empty;
    empty.chunks_retrieved = 0;
    empty.chunks_reranked = 0;
    input.diagnostics = &empty;
    input.indexed_counts_available = false;
    const auto emptySnap = Thoth::buildCm1Snapshot(input);
    expect(emptySnap.measurement_status == "empty_retrieval", "empty: status");
    const auto emptyFeat = nlohmann::json::parse(emptySnap.features_json);
    expect(emptyFeat["evidence_state"] == "no_evidence", "empty: no_evidence");

    Thoth::Cm1SnapshotBuildInput missing;
    missing.plan_id = "p2";
    missing.step_id = "r2";
    missing.snapshot_type = "retrieval_observed";
    missing.lifecycle_stage = "after_retrieval";
    missing.observed_at_ms = 12;
    missing.diagnostics = nullptr;
    missing.selected_chunks = {{"/x.md", "text"}};
    const auto miss = Thoth::buildCm1Snapshot(missing);
    expect(miss.measurement_status == "incomplete", "missing diagnostics: incomplete");
    return true;
}

bool testSynthesisInclusionAndTruncation() {
    const GragDiagnostics d = makeDiagTwoFiles();
    Thoth::Cm1SnapshotBuildInput input;
    input.plan_id = "p3";
    input.step_id = "s1";
    input.snapshot_type = "synthesis_observed";
    input.lifecycle_stage = "after_synthesis";
    input.observed_at_ms = 13;
    input.diagnostics = &d;
    input.selected_chunks = {{"/corpus/doc_a.md", "alpha chunk one"},
                             {"/corpus/doc_b.md", "beta chunk"}};
    input.included_in_synthesis_available = true;
    input.included_content_sha256 = {Thoth::sha256Hex("alpha chunk one")};
    input.synthesis_context_truncated_available = true;
    input.synthesis_context_truncated = true;
    const auto snap = Thoth::buildCm1Snapshot(input);
    const auto feat = nlohmann::json::parse(snap.features_json);
    expect(feat["synthesis_context_truncated"] == true, "synthesis: truncated flag");
    expect(feat["included_in_synthesis_status"] == "available", "synthesis: inclusion available");
    expect(feat["included_in_synthesis_total"] == 1, "synthesis: one included");

    Thoth::Cm1SnapshotBuildInput unavailable = input;
    unavailable.included_in_synthesis_available = false;
    unavailable.included_content_sha256.clear();
    const auto snap2 = Thoth::buildCm1Snapshot(unavailable);
    const auto feat2 = nlohmann::json::parse(snap2.features_json);
    expect(feat2["included_in_synthesis_status"] == "unavailable",
           "synthesis: inclusion unavailable when unknown");
    return true;
}

bool testAnswerVersionAndIdempotency() {
    const auto dbPath = makeTempDb();
    auto repo = std::make_shared<Thoth::SQLiteMemoryRepository>(dbPath.string());
    const auto answer = Thoth::buildCm1AnswerVersion("p4", "s1", "gen-1", "hello answer", 100);
    expect(!answer.answer_id.empty(), "answer: id present");
    expect(answer.content_sha256 == Thoth::sha256Hex("hello answer"), "answer: hash");
    expect(Thoth::buildCm1AnswerVersion("p4", "s1", "", "hello", 100).answer_id.empty(),
           "answer: no fabricate without generation_id");

    Thoth::Cm1SnapshotBuildInput input;
    input.plan_id = "p4";
    input.step_id = "r1";
    input.snapshot_type = "retrieval_observed";
    input.lifecycle_stage = "after_retrieval";
    input.observed_at_ms = 100;
    const GragDiagnostics d = makeDiagTwoFiles();
    input.diagnostics = &d;
    auto snap = Thoth::buildCm1Snapshot(input);

    expect(repo->commitStepCheckpoint(activePlan("p4"), std::nullopt, episode("p4", 0), snap, answer),
           "persist: first commit");
    expect(repo->commitStepCheckpoint(activePlan("p4"), std::nullopt, episode("p4", 0), snap, answer),
           "persist: replay idempotent");

    const auto loaded = repo->loadCm1Snapshot(snap.snapshot_id);
    expect(loaded.has_value(), "persist: snapshot loaded");
    expect(loaded->features_json == snap.features_json, "persist: immutable features");
    const auto loadedAns = repo->loadCm1AnswerVersion(answer.answer_id);
    expect(loadedAns.has_value(), "persist: answer loaded");
    expect(loadedAns->content_sha256 == answer.content_sha256, "persist: answer hash stable");

    const auto listed = repo->listCm1SnapshotsForPlan("p4");
    expect(listed.size() == 1, "persist: one snapshot after replay");
    expect(repo->listCm1SnapshotsForPlan("legacy-none").empty(), "persist: legacy not_recorded");
    return true;
}

bool testTelemetryWriteFailurePreservesCheckpoint() {
    const auto dbPath = makeTempDb();
    auto repo = std::make_shared<Thoth::SQLiteMemoryRepository>(dbPath.string());
    Thoth::Cm1SnapshotBuildInput input;
    input.plan_id = "p5";
    input.step_id = "r1";
    input.snapshot_type = "retrieval_observed";
    input.lifecycle_stage = "after_retrieval";
    input.observed_at_ms = 50;
    const GragDiagnostics d = makeDiagTwoFiles();
    input.diagnostics = &d;
    auto snap = Thoth::buildCm1Snapshot(input);

    ::setenv("THOTH_INJECT_CM1_WRITE_FAIL", "1", 1);
    expect(repo->commitStepCheckpoint(activePlan("p5"), std::nullopt, episode("p5", 0), snap, std::nullopt),
           "fail: step commit still true");
    ::unsetenv("THOTH_INJECT_CM1_WRITE_FAIL");

    const auto plan = repo->getActivePlan("s1");
    expect(plan.has_value() && plan->plan_id == "p5", "fail: active plan preserved");
    expect(!repo->loadCm1Snapshot(snap.snapshot_id).has_value(), "fail: no fabricated snapshot");
    return true;
}

bool nearFloat(float a, float b) {
    return std::fabs(a - b) < 1e-5f;
}

float saturated(float mass) {
    return mass / (mass + 1.0f);
}

Thoth::Cm1SnapshotRecord retrievalFromDiag(const std::string& plan_id,
                                          const std::string& step_id,
                                          const GragDiagnostics& diag,
                                          std::int64_t observed_at_ms) {
    Thoth::Cm1SnapshotBuildInput input;
    input.plan_id = plan_id;
    input.step_id = step_id;
    input.snapshot_type = "retrieval_observed";
    input.lifecycle_stage = "after_retrieval";
    input.observed_at_ms = observed_at_ms;
    input.diagnostics = &diag;
    return Thoth::buildCm1Snapshot(input);
}

GragDiagnostics diagForFiles(const std::vector<std::pair<std::string, float>>& files,
                             int copies_of_first = 1) {
    GragDiagnostics d;
    d.chunks_retrieved = 0;
    d.scoring_type = "grag_blended_hybrid";
    d.routing_mode = "PLAN_AWARE";
    for (std::size_t i = 0; i < files.size(); ++i) {
        const int copies = (i == 0) ? copies_of_first : 1;
        for (int c = 0; c < copies; ++c) {
            ScoreBreakdown b;
            b.file_name = files[i].first;
            b.code_text = files[i].first + " chunk " + std::to_string(c);
            b.query_sim = files[i].second;
            b.final_score = files[i].second + 0.4f;
            b.graph_score = (c == 0) ? 0.3f : 0.0f;
            d.breakdowns.push_back(b);
            d.final_scores.push_back(b.final_score);
            ++d.chunks_retrieved;
        }
    }
    d.chunks_reranked = d.chunks_retrieved;
    return d;
}

bool testBreadthScoring() {
    const auto many = retrievalFromDiag(
        "breadth", "r-many", diagForFiles({{"/corpus/only.md", 0.5f}}, 20), 1);
    const auto one = retrievalFromDiag(
        "breadth", "r-one", diagForFiles({{"/corpus/only.md", 0.5f}}, 1), 2);
    const auto manyEst = Thoth::buildCm1BreadthEstimate(many, {});
    const auto oneEst = Thoth::buildCm1BreadthEstimate(one, {});
    const auto manyPayload = nlohmann::json::parse(manyEst.payload_json);
    const auto onePayload = nlohmann::json::parse(oneEst.payload_json);
    expect(many.estimator_status == Thoth::kCm1EstimatorStatus, "breadth: raw estimator stays none_active");
    expect(manyPayload["evidence_breadth_retrieved_status"] == "available", "breadth: many available");
    expect(nearFloat(manyPayload["retrieved_mass"].get<float>(), 0.5f),
           "breadth: twenty chunks from one path contribute once");
    expect(manyPayload["retrieved_path_count"] == 1, "breadth: one path");
    expect(nearFloat(manyPayload["evidence_breadth_retrieved"].get<float>(), saturated(0.5f)),
           "breadth: saturated mass");
    expect(nearFloat(manyPayload["evidence_breadth_retrieved"].get<float>(),
                     onePayload["evidence_breadth_retrieved"].get<float>()),
           "breadth: extra chunks and graph_score do not change the index");
    expect(manyPayload["evidence_breadth_retrieved"].get<float>() < 1.0f, "breadth: strictly below 1");
    expect(!manyPayload.contains("goal_confidence"), "breadth: no goal_confidence");
    expect(!manyPayload.contains("predicted_correctness"), "breadth: no predicted_correctness");
    expect(manyPayload["agreement_status"] == "not_evaluated", "breadth: agreement not_evaluated");
    expect(manyPayload["label"] == Thoth::kCm1BreadthLabel, "breadth: uncalibrated label");
    expect(manyPayload["goal_evidence_breadth_used_status"] == "not_recorded",
           "breadth: used goal breadth not_recorded before synthesis");

    std::vector<std::pair<std::string, float>> six;
    for (int i = 0; i < 6; ++i) {
        six.push_back({"/corpus/src_" + std::to_string(i) + ".md", 0.5f});
    }
    const auto sixSnap = retrievalFromDiag("breadth", "r-six", diagForFiles(six), 3);
    const auto sixEst = Thoth::buildCm1BreadthEstimate(sixSnap, {});
    const auto sixPayload = nlohmann::json::parse(sixEst.payload_json);
    expect(sixPayload["retrieved_path_count"] == 6, "breadth: six paths");
    expect(nearFloat(sixPayload["retrieved_mass"].get<float>(), 3.0f), "breadth: six strengths sum");
    expect(sixPayload["evidence_breadth_retrieved"].get<float>()
               > onePayload["evidence_breadth_retrieved"].get<float>(),
           "breadth: six paths outweigh one path");

    Thoth::Cm1SnapshotBuildInput usedInput;
    usedInput.plan_id = "breadth";
    usedInput.step_id = "s-used";
    usedInput.snapshot_type = "synthesis_observed";
    usedInput.lifecycle_stage = "after_synthesis";
    usedInput.observed_at_ms = 4;
    const GragDiagnostics sixDiag = diagForFiles(six);
    usedInput.diagnostics = &sixDiag;
    usedInput.included_in_synthesis_available = true;
    usedInput.synthesis_context_truncated_available = true;
    usedInput.synthesis_context_truncated = false;
    for (const auto& file : six) {
        usedInput.selected_chunks.push_back({file.first, file.first + " chunk 0"});
    }
    usedInput.included_content_sha256 = {Thoth::sha256Hex(six[0].first + " chunk 0")};
    const auto usedFew = Thoth::buildCm1Snapshot(usedInput);
    usedInput.synthesis_context_truncated = true;
    std::vector<std::string> allHashes;
    for (const auto& file : six) {
        allHashes.push_back(Thoth::sha256Hex(file.first + " chunk 0"));
    }
    usedInput.included_content_sha256 = allHashes;
    const auto usedAllTruncated = Thoth::buildCm1Snapshot(usedInput);
    usedInput.synthesis_context_truncated = false;
    const auto usedAll = Thoth::buildCm1Snapshot(usedInput);

    const auto fewEst = Thoth::buildCm1BreadthEstimate(usedFew, {sixSnap});
    const auto allEst = Thoth::buildCm1BreadthEstimate(usedAll, {sixSnap});
    const auto truncEst = Thoth::buildCm1BreadthEstimate(usedAllTruncated, {sixSnap});
    const auto fewPayload = nlohmann::json::parse(fewEst.payload_json);
    const auto allPayload = nlohmann::json::parse(allEst.payload_json);
    const auto truncPayload = nlohmann::json::parse(truncEst.payload_json);
    expect(fewPayload["evidence_breadth_used_status"] == "available", "breadth: used available");
    expect(nearFloat(fewPayload["used_mass"].get<float>(), 0.5f), "breadth: one included path");
    expect(nearFloat(allPayload["used_mass"].get<float>(), 3.0f), "breadth: six included paths");
    expect(allPayload["evidence_breadth_used"].get<float>() > fewPayload["evidence_breadth_used"].get<float>(),
           "breadth: used index falls when inclusion drops");
    expect(nearFloat(truncPayload["evidence_breadth_used"].get<float>(),
                     allPayload["evidence_breadth_used"].get<float>()),
           "breadth: truncation does not change the index");
    expect(truncPayload["synthesis_context_truncated"] == true, "breadth: truncation kept as telemetry");
    expect(allPayload["step_evidence_breadth_status"] == "available", "breadth: step used available");
    expect(nearFloat(allPayload["step_evidence_breadth"].get<float>(),
                     allPayload["evidence_breadth_used"].get<float>()),
           "breadth: synthesis step uses the used index");

    const auto repeat = retrievalFromDiag(
        "breadth", "r-repeat", diagForFiles({{"/corpus/only.md", 0.25f}}), 5);
    const auto stronger = retrievalFromDiag(
        "breadth", "r-stronger", diagForFiles({{"/corpus/only.md", 0.5f}}), 6);
    const auto merged = Thoth::buildCm1BreadthEstimate(stronger, {repeat});
    const auto mergedPayload = nlohmann::json::parse(merged.payload_json);
    expect(mergedPayload["goal_retrieved_path_count"] == 1, "breadth: same path is not counted twice");
    expect(nearFloat(mergedPayload["goal_retrieved_mass"].get<float>(), 0.5f),
           "breadth: goal mass keeps the stronger query_sim");

    GragDiagnostics empty;
    empty.chunks_retrieved = 0;
    const auto emptySnap = retrievalFromDiag("breadth", "r-empty", empty, 7);
    const auto emptyEst = Thoth::buildCm1BreadthEstimate(emptySnap, {});
    const auto emptyPayload = nlohmann::json::parse(emptyEst.payload_json);
    expect(emptySnap.measurement_status == "empty_retrieval", "breadth: empty status");
    expect(emptyPayload["evidence_breadth_retrieved_status"] == "available", "breadth: empty is zero, available");
    expect(nearFloat(emptyPayload["evidence_breadth_retrieved"].get<float>(), 0.0f), "breadth: empty index is 0");
    expect(emptyPayload["retrieved_path_count"] == 0, "breadth: empty path count 0");

    auto missing = sixSnap;
    auto missingFeatures = nlohmann::json::parse(missing.features_json);
    for (auto& group : missingFeatures["source_groups"]) {
        group.erase("highest_query_sim");
        group["highest_query_sim_status"] = "unavailable";
    }
    missing.features_json = missingFeatures.dump();
    const auto missingEst = Thoth::buildCm1BreadthEstimate(missing, {});
    const auto missingPayload = nlohmann::json::parse(missingEst.payload_json);
    expect(missingPayload["evidence_breadth_retrieved_status"] == "unavailable",
           "breadth: missing query_sim is unavailable");
    expect(!missingPayload.contains("evidence_breadth_retrieved"),
           "breadth: unavailable does not store a zero index");
    expect(missingPayload["goal_evidence_breadth_retrieved_status"] == "unavailable",
           "breadth: incomplete goal query is unavailable");

    Thoth::Cm1SnapshotBuildInput noDiag;
    noDiag.plan_id = "breadth";
    noDiag.step_id = "r-nodiag";
    noDiag.snapshot_type = "retrieval_observed";
    noDiag.lifecycle_stage = "after_retrieval";
    noDiag.observed_at_ms = 8;
    noDiag.selected_chunks = {{"/corpus/x.md", "text"}};
    const auto noDiagSnap = Thoth::buildCm1Snapshot(noDiag);
    const auto noDiagPayload = nlohmann::json::parse(
        Thoth::buildCm1BreadthEstimate(noDiagSnap, {}).payload_json);
    expect(noDiagSnap.measurement_status == "incomplete", "breadth: missing diagnostics incomplete");
    expect(noDiagPayload["evidence_breadth_retrieved_status"] == "unavailable",
           "breadth: missing diagnostics are not zero");

    ScoreBreakdown high;
    high.file_name = "/corpus/hot.md";
    high.code_text = "hot";
    high.query_sim = 1.4f;
    high.final_score = 3.0f;
    GragDiagnostics hot;
    hot.chunks_retrieved = 1;
    hot.chunks_reranked = 1;
    hot.breakdowns = {high};
    const auto hotSnap = retrievalFromDiag("breadth", "r-hot", hot, 9);
    const auto hotPayload = nlohmann::json::parse(
        Thoth::buildCm1BreadthEstimate(hotSnap, {}).payload_json);
    expect(nearFloat(hotPayload["retrieved_mass"].get<float>(), 1.0f), "breadth: query_sim clamps to 1");
    expect(hotPayload["evidence_breadth_retrieved"].get<float>() < 1.0f, "breadth: clamp still saturates below 1");
    return true;
}

bool testBreadthReplayAndDisabledByDefault() {
    ::unsetenv("THOTH_CM1_BREADTH");
    expect(!Thoth::cm1BreadthEnabled(), "breadth: default off");

    const auto dbPath = makeTempDb();
    auto repo = std::make_shared<Thoth::SQLiteMemoryRepository>(dbPath.string());
    const auto snap = retrievalFromDiag("p6", "r1", diagForFiles({{"/corpus/only.md", 0.5f}}), 10);
    const std::string rawFeatures = snap.features_json;
    expect(repo->commitStepCheckpoint(activePlan("p6"), std::nullopt, episode("p6", 0), snap, std::nullopt),
           "breadth off: commit");
    expect(!repo->loadCm1BreadthEstimate("cm1b2:" + snap.snapshot_id).has_value(),
           "breadth off: no estimate row");

    ::setenv("THOTH_CM1_BREADTH", "1", 1);
    expect(Thoth::cm1BreadthEnabled(), "breadth: enabled by flag");
    const auto estimate = Thoth::buildCm1BreadthEstimate(snap, {});
    expect(repo->commitStepCheckpoint(
               activePlan("p6"), std::nullopt, episode("p6", 0), snap, std::nullopt, estimate),
           "breadth on: commit");
    expect(repo->commitStepCheckpoint(
               activePlan("p6"), std::nullopt, episode("p6", 0), snap, std::nullopt, estimate),
           "breadth on: replay");
    const auto loaded = repo->loadCm1BreadthEstimate(estimate.estimate_id);
    expect(loaded.has_value(), "breadth: row loaded");
    expect(loaded->payload_json == estimate.payload_json, "breadth: replay keeps the first payload");
    expect(loaded->estimator_version == Thoth::kCm1BreadthVersion, "breadth: version cm1-breadth-2");
    const auto raw = repo->loadCm1Snapshot(snap.snapshot_id);
    expect(raw.has_value() && raw->features_json == rawFeatures, "breadth: raw snapshot unchanged");
    expect(raw->estimator_status == Thoth::kCm1EstimatorStatus, "breadth: raw status remains none_active");
    expect(repo->listCm1SnapshotsForPlan("p6").size() == 1, "breadth: replay does not duplicate snapshots");

    const auto failDb = makeTempDb();
    auto failRepo = std::make_shared<Thoth::SQLiteMemoryRepository>(failDb.string());
    const auto failSnap = retrievalFromDiag("p7", "r1", diagForFiles({{"/corpus/only.md", 0.5f}}), 11);
    const auto failEstimate = Thoth::buildCm1BreadthEstimate(failSnap, {});
    ::setenv("THOTH_INJECT_CM1_WRITE_FAIL", "1", 1);
    expect(failRepo->commitStepCheckpoint(
               activePlan("p7"), std::nullopt, episode("p7", 0), failSnap, std::nullopt, failEstimate),
           "breadth fail: step commit still true");
    ::unsetenv("THOTH_INJECT_CM1_WRITE_FAIL");
    ::unsetenv("THOTH_CM1_BREADTH");
    expect(failRepo->getActivePlan("s1").has_value(), "breadth fail: plan preserved");
    expect(!failRepo->loadCm1Snapshot(failSnap.snapshot_id).has_value(), "breadth fail: no snapshot");
    expect(!failRepo->loadCm1BreadthEstimate(failEstimate.estimate_id).has_value(),
           "breadth fail: no estimate");
    return true;
}

bool testGenerationIdsSurviveRestartShape() {
    const auto a = Thoth::makeGenerationId();
    const auto b = Thoth::makeGenerationId();
    expect(a != b, "generation: two calls differ");
    expect(a.rfind("gen-", 0) == 0 && b.rfind("gen-", 0) == 0, "generation: gen- prefix");
    expect(a.size() > 10 && b.size() > 10, "generation: longer than a restarting counter");
    return true;
}

bool testAnswerFeedbackAndVerifiedLabels() {
    const auto dbPath = makeTempDb();
    auto repo = std::make_shared<Thoth::SQLiteMemoryRepository>(dbPath.string());
    const std::string text = "hello answer";
    const auto answer = Thoth::buildCm1AnswerVersion("p8", "s1", "gen-stable", text, 200);
    expect(repo->insertCm1AnswerVersion(answer), "feedback: answer stored");
    const auto loaded = repo->loadCm1AnswerVersion(answer.answer_id);
    expect(loaded.has_value(), "feedback: answer loaded");

    Thoth::Cm1FeedbackSubmission blank;
    blank.answer_id = answer.answer_id;
    blank.answer_text = text;
    const auto none = Thoth::prepareCm1UserFeedback(&*loaded, nullptr, blank, 201);
    expect(none.disposition == Thoth::Cm1RecordDisposition::NoRecord, "feedback: blank writes nothing");
    expect(repo->listCm1UserFeedback(answer.answer_id).empty(), "feedback: no row before submit");

    Thoth::Cm1FeedbackSubmission missing = blank;
    missing.answer_id = "missing-answer";
    const auto missingDraft = Thoth::prepareCm1UserFeedback(nullptr, nullptr, missing, 202);
    expect(missingDraft.disposition == Thoth::Cm1RecordDisposition::Rejected,
           "feedback: missing answer rejected");

    Thoth::Cm1FeedbackSubmission thumbs = blank;
    thumbs.thumbs_status = "up";
    const auto thumbsDraft = Thoth::prepareCm1UserFeedback(&*loaded, nullptr, thumbs, 203);
    expect(thumbsDraft.disposition == Thoth::Cm1RecordDisposition::Insert, "feedback: thumbs-only accepted");
    expect(thumbsDraft.record.quality_rating_status == "absent", "feedback: thumbs-only has no rating");
    expect(repo->insertCm1UserFeedback(thumbsDraft.record), "feedback: thumbs stored");

    Thoth::Cm1FeedbackSubmission rating = blank;
    rating.include_quality_rating = true;
    rating.quality_rating = 11;
    const auto badRating = Thoth::prepareCm1UserFeedback(&*loaded, nullptr, rating, 204);
    expect(badRating.disposition == Thoth::Cm1RecordDisposition::Rejected,
           "feedback: rating outside 1-10 rejected");
    rating.quality_rating = 8;
    const auto latestThumbs = repo->latestCm1UserFeedback(answer.answer_id);
    const auto ratingDraft = Thoth::prepareCm1UserFeedback(
        &*loaded, latestThumbs ? &*latestThumbs : nullptr, rating, 205);
    expect(ratingDraft.record.thumbs_status == "absent", "feedback: rating-only leaves thumbs unset");
    expect(ratingDraft.record.quality_rating == 8, "feedback: rating stored");
    expect(ratingDraft.record.revision_index == 2, "feedback: revision increments");
    expect(ratingDraft.record.supersedes_feedback_id == thumbsDraft.record.feedback_id,
           "feedback: revision points at the previous row");
    expect(repo->insertCm1UserFeedback(ratingDraft.record), "feedback: rating revision stored");
    expect(repo->insertCm1UserFeedback(ratingDraft.record), "feedback: replay insert is ignored");

    Thoth::Cm1FeedbackSubmission note = blank;
    note.explanation = "  clearer than the first draft  ";
    const auto latestRating = repo->latestCm1UserFeedback(answer.answer_id);
    const auto noteDraft = Thoth::prepareCm1UserFeedback(
        &*loaded, latestRating ? &*latestRating : nullptr, note, 206);
    expect(noteDraft.record.explanation == "clearer than the first draft", "feedback: explanation trimmed");
    expect(noteDraft.record.thumbs_status == "absent", "feedback: explanation-only does not invent thumbs");
    expect(noteDraft.record.quality_rating_status == "absent",
           "feedback: explanation-only does not keep an unsent rating");
    expect(repo->insertCm1UserFeedback(noteDraft.record), "feedback: explanation stored");

    Thoth::Cm1FeedbackSubmission keep = blank;
    keep.thumbs_status = "down";
    keep.include_quality_rating = true;
    keep.quality_rating = 4;
    const auto both = Thoth::prepareCm1UserFeedback(&*loaded, &noteDraft.record, keep, 207);
    expect(repo->insertCm1UserFeedback(both.record), "feedback: both fields stored");
    Thoth::Cm1FeedbackSubmission changeRating = keep;
    changeRating.quality_rating = 6;
    const auto revised = Thoth::prepareCm1UserFeedback(&*loaded, &both.record, changeRating, 208);
    expect(revised.record.thumbs_status == "down", "feedback: revision keeps the unchanged thumb");
    expect(revised.record.quality_rating == 6, "feedback: revision changes the rating");
    expect(repo->insertCm1UserFeedback(revised.record), "feedback: revision stored");

    const auto cleared = Thoth::prepareCm1UserFeedback(&*loaded, &revised.record, blank, 209);
    expect(cleared.disposition == Thoth::Cm1RecordDisposition::Insert, "feedback: explicit clear is a row");
    expect(cleared.record.thumbs_status == "absent"
               && cleared.record.quality_rating_status == "absent"
               && cleared.record.explanation.empty(),
           "feedback: clear drops both fields");
    expect(repo->insertCm1UserFeedback(cleared.record), "feedback: clear stored");

    Thoth::Cm1FeedbackSubmission mismatch = thumbs;
    mismatch.answer_text = "different answer";
    const auto refused = Thoth::prepareCm1UserFeedback(&*loaded, &cleared.record, mismatch, 210);
    expect(refused.disposition == Thoth::Cm1RecordDisposition::Rejected, "feedback: hash mismatch refused");

    const auto history = repo->listCm1UserFeedback(answer.answer_id);
    expect(history.size() == 6, "feedback: history is append-only");
    expect(history.front().feedback_id == thumbsDraft.record.feedback_id, "feedback: first row remains");
    const auto current = repo->latestCm1UserFeedback(answer.answer_id);
    expect(current && current->feedback_id == cleared.record.feedback_id, "feedback: latest is the clear");

    Thoth::Cm1VerifiedLabelSubmission label;
    label.answer_id = answer.answer_id;
    label.answer_text = text;
    label.protocol_id = "desk-check";
    label.reviewer_id = "reviewer-a";
    label.factual_bin = 7;
    const auto labelDraft = Thoth::prepareCm1VerifiedLabel(&*loaded, nullptr, label, 211);
    expect(labelDraft.disposition == Thoth::Cm1RecordDisposition::Insert, "verified: accepted");
    expect(repo->insertCm1VerifiedLabel(labelDraft.record), "verified: stored");
    label.factual_bin = 3;
    const auto relabel = Thoth::prepareCm1VerifiedLabel(&*loaded, &labelDraft.record, label, 212);
    expect(relabel.record.revision_index == 2, "verified: revision appends");
    expect(relabel.record.supersedes_label_id == labelDraft.record.label_id, "verified: points at previous");
    expect(repo->insertCm1VerifiedLabel(relabel.record), "verified: revision stored");
    label.answer_text = "different answer";
    const auto badLabel = Thoth::prepareCm1VerifiedLabel(&*loaded, &relabel.record, label, 213);
    expect(badLabel.disposition == Thoth::Cm1RecordDisposition::Rejected, "verified: hash mismatch refused");

    const auto still = repo->latestCm1UserFeedback(answer.answer_id);
    expect(still && still->feedback_id == cleared.record.feedback_id
               && still->content_sha256 == answer.content_sha256,
           "verified: does not change the feedback row");
    const auto answerAfter = repo->loadCm1AnswerVersion(answer.answer_id);
    expect(answerAfter && answerAfter->content_sha256 == answer.content_sha256,
           "verified: answer provenance unchanged");
    expect(repo->listCm1VerifiedLabels(answer.answer_id).size() == 2, "verified: both labels remain");
    const auto latestLabel = repo->latestCm1VerifiedLabel(answer.answer_id, "desk-check");
    expect(latestLabel && latestLabel->factual_bin == 3, "verified: latest bin is the revision");
    expect(!repo->latestCm1VerifiedLabel(answer.answer_id, "other-protocol").has_value(),
           "verified: a second protocol is not filled in");
    return true;
}

Thoth::Cm1EvidenceCompletion completionWith(const std::string& text,
                                             const std::string& finish_reason,
                                             bool ok = true) {
    Thoth::Cm1EvidenceCompletion completion;
    completion.called = true;
    completion.ok = ok;
    completion.text = text;
    completion.finish_reason = finish_reason;
    completion.generation_id = "gen-judge";
    if (!ok) completion.error = "down";
    return completion;
}

Thoth::Cm1AnswerVersionRecord evidenceAnswer() {
    return Thoth::buildCm1AnswerVersion("p7", "synthesize", "gen-answer", "answer body", 50);
}

Thoth::Cm1ClaimVerificationSubmission claimSubmission(
    const Thoth::Cm1AnswerVersionRecord& answer,
    const Thoth::Cm1StoredClaim& claim,
    const std::string& answer_text) {
    Thoth::Cm1ClaimVerificationSubmission submission;
    submission.claim_id = claim.claim_id;
    submission.claim_text = claim.claim_text;
    submission.answer_id = answer.answer_id;
    submission.answer_text = answer_text;
    submission.protocol_id = "claim-check";
    submission.protocol_version = "v1";
    submission.reviewer_id = "reviewer-a";
    submission.verifier_kind = "human";
    submission.verifier_version = "";
    submission.method_id = "desk-read";
    submission.outcome = "true";
    submission.source_kind = "external_manual";
    submission.source_ref = "relay-handbook-4";
    submission.calibration_eligibility = "provisional";
    submission.egar_eligibility = "excluded";
    submission.revision_reason = "initial";
    return submission;
}

bool testClaimVerification() {
    const std::string answer_text = "The relay cache stores one blue token.";
    const auto answer = Thoth::buildCm1AnswerVersion(
        "p9", "synthesize", "gen-claim", answer_text, 300);
    Thoth::Cm1EvidenceChunk passage;
    passage.content = "The relay cache stores one blue token.";
    passage.content_sha256 = Thoth::sha256Hex(passage.content);
    const auto judged = Thoth::runCm1EvidenceJudgment(
        answer, answer_text, {passage}, true, true, false,
        [&](const std::string&, int) {
            return completionWith(
                "{\"label\":\"supported\",\"evidence_quote\":\"The relay cache stores one blue token.\"}",
                "stop");
        },
        300);
    expect(judged.has_value(), "claim verify: judgment exists");
    if (!judged) return false;
    const std::string judgment_payload = judged->payload_json;

    const auto dbPath = makeTempDb();
    auto repo = std::make_shared<Thoth::SQLiteMemoryRepository>(dbPath.string());
    expect(repo->insertCm1AnswerVersion(answer), "claim verify: answer stored");
    expect(repo->insertCm1EvidenceJudgment(*judged), "claim verify: judgment stored");
    const auto loadedAnswer = repo->loadCm1AnswerVersion(answer.answer_id);
    const auto loadedJudgment = repo->loadCm1EvidenceJudgment(judged->judgment_id);
    expect(loadedAnswer.has_value() && loadedJudgment.has_value(), "claim verify: records reloaded");
    if (!loadedAnswer || !loadedJudgment) return false;

    const auto claim_id = nlohmann::json::parse(loadedJudgment->payload_json)["claims"][0]
                              .value("claim_id", "");
    const auto stored = Thoth::findCm1StoredClaim(*loadedJudgment, claim_id);
    expect(stored.has_value(), "claim verify: stored claim is read from the judgment");
    expect(!Thoth::findCm1StoredClaim(*loadedJudgment, claim_id + ":missing").has_value(),
           "claim verify: unknown claim is absent");
    if (!stored) return false;

    Thoth::Cm1VerifiedLabelSubmission label;
    label.answer_id = answer.answer_id;
    label.answer_text = answer_text;
    label.protocol_id = "desk-check";
    label.reviewer_id = "reviewer-a";
    label.factual_bin = 8;
    const auto labelDraft = Thoth::prepareCm1VerifiedLabel(&*loadedAnswer, nullptr, label, 301);
    expect(repo->insertCm1VerifiedLabel(labelDraft.record), "claim verify: factual label stored");

    auto submission = claimSubmission(*loadedAnswer, *stored, answer_text);
    const auto missing = Thoth::prepareCm1ClaimVerification(
        &*loadedAnswer, nullptr, nullptr, submission, 302);
    expect(missing.disposition == Thoth::Cm1RecordDisposition::Rejected,
           "claim verify: missing claim rejected");

    submission.answer_text = "different answer";
    const auto badAnswer = Thoth::prepareCm1ClaimVerification(
        &*loadedAnswer, &*stored, nullptr, submission, 303);
    expect(badAnswer.disposition == Thoth::Cm1RecordDisposition::Rejected,
           "claim verify: answer hash rejected");
    submission.answer_text = answer_text;
    submission.claim_text = "A different claim.";
    const auto badClaim = Thoth::prepareCm1ClaimVerification(
        &*loadedAnswer, &*stored, nullptr, submission, 304);
    expect(badClaim.disposition == Thoth::Cm1RecordDisposition::Rejected,
           "claim verify: claim hash rejected");
    submission.claim_text = stored->claim_text;

    for (const char* kind : {"cm1_judgment", "evidence_passage", "evidence_quote", "grag"}) {
        submission.source_kind = kind;
        const auto internal = Thoth::prepareCm1ClaimVerification(
            &*loadedAnswer, &*stored, nullptr, submission, 305);
        expect(internal.disposition == Thoth::Cm1RecordDisposition::Rejected,
               std::string("claim verify: source kind rejected: ") + kind);
    }
    submission.source_kind = "external_manual";
    submission.source_ref = stored->evidence_quote;
    const auto quoteRef = Thoth::prepareCm1ClaimVerification(
        &*loadedAnswer, &*stored, nullptr, submission, 306);
    expect(quoteRef.disposition == Thoth::Cm1RecordDisposition::Rejected,
           "claim verify: evidence quote reference rejected");
    submission.source_ref = stored->evidence_chunk_sha256;
    const auto hashRef = Thoth::prepareCm1ClaimVerification(
        &*loadedAnswer, &*stored, nullptr, submission, 307);
    expect(hashRef.disposition == Thoth::Cm1RecordDisposition::Rejected,
           "claim verify: evidence chunk reference rejected");
    submission.source_ref = "relay-handbook-4";

    submission.verifier_kind = "human";
    submission.verifier_version = "1";
    const auto humanVersion = Thoth::prepareCm1ClaimVerification(
        &*loadedAnswer, &*stored, nullptr, submission, 308);
    expect(humanVersion.disposition == Thoth::Cm1RecordDisposition::Rejected,
           "claim verify: human version rejected");
    submission.verifier_version = "";
    submission.verifier_kind = "automated_agent";
    const auto agentVersion = Thoth::prepareCm1ClaimVerification(
        &*loadedAnswer, &*stored, nullptr, submission, 309);
    expect(agentVersion.disposition == Thoth::Cm1RecordDisposition::Rejected,
           "claim verify: agent without version rejected");
    submission.verifier_kind = "external_service";
    const auto serviceVersion = Thoth::prepareCm1ClaimVerification(
        &*loadedAnswer, &*stored, nullptr, submission, 310);
    expect(serviceVersion.disposition == Thoth::Cm1RecordDisposition::Rejected,
           "claim verify: service without version rejected");
    submission.verifier_kind = "human";
    submission.method_id = " ";
    const auto emptyMethod = Thoth::prepareCm1ClaimVerification(
        &*loadedAnswer, &*stored, nullptr, submission, 311);
    expect(emptyMethod.disposition == Thoth::Cm1RecordDisposition::Rejected,
           "claim verify: empty method rejected");
    submission.method_id = "desk-read";

    submission.outcome = "maybe";
    const auto badOutcome = Thoth::prepareCm1ClaimVerification(
        &*loadedAnswer, &*stored, nullptr, submission, 312);
    expect(badOutcome.disposition == Thoth::Cm1RecordDisposition::Rejected,
           "claim verify: unknown outcome rejected");
    submission.outcome = "true";
    submission.revision_reason = "correction";
    const auto earlyCorrection = Thoth::prepareCm1ClaimVerification(
        &*loadedAnswer, &*stored, nullptr, submission, 313);
    expect(earlyCorrection.disposition == Thoth::Cm1RecordDisposition::Rejected,
           "claim verify: correction without a chain rejected");
    submission.revision_reason = "authorization";
    const auto authorize = Thoth::prepareCm1ClaimVerification(
        &*loadedAnswer, &*stored, nullptr, submission, 314);
    expect(authorize.disposition == Thoth::Cm1RecordDisposition::Rejected,
           "claim verify: authorization reason rejected");
    submission.revision_reason = "initial";
    submission.calibration_eligibility = "authorized";
    const auto calAuth = Thoth::prepareCm1ClaimVerification(
        &*loadedAnswer, &*stored, nullptr, submission, 315);
    expect(calAuth.disposition == Thoth::Cm1RecordDisposition::Rejected,
           "claim verify: authorized calibration rejected");
    submission.calibration_eligibility = "provisional";
    submission.egar_eligibility = "authorized";
    const auto egarAuth = Thoth::prepareCm1ClaimVerification(
        &*loadedAnswer, &*stored, nullptr, submission, 316);
    expect(egarAuth.disposition == Thoth::Cm1RecordDisposition::Rejected,
           "claim verify: authorized EGAR eligibility rejected");
    submission.egar_eligibility = "excluded";

    submission.evidence_label = "supported";
    const auto labeledTruth = Thoth::prepareCm1ClaimVerification(
        &*loadedAnswer, &*stored, nullptr, submission, 316);
    expect(labeledTruth.disposition == Thoth::Cm1RecordDisposition::Rejected,
           "claim verify: evidence label with true outcome rejected");
    submission.outcome = "unverifiable";
    submission.evidence_label = "maybe";
    const auto badLabel = Thoth::prepareCm1ClaimVerification(
        &*loadedAnswer, &*stored, nullptr, submission, 316);
    expect(badLabel.disposition == Thoth::Cm1RecordDisposition::Rejected,
           "claim verify: unknown evidence label rejected");
    submission.evidence_label.clear();
    submission.outcome = "true";

    const auto initial = Thoth::prepareCm1ClaimVerification(
        &*loadedAnswer, &*stored, nullptr, submission, 317);
    expect(initial.disposition == Thoth::Cm1RecordDisposition::Insert, "claim verify: true accepted");
    expect(initial.record.outcome == "true", "claim verify: outcome true");
    expect(initial.record.evidence_label.empty(), "claim verify: factual outcome has no evidence label");
    expect(initial.record.revision_reason == "initial", "claim verify: reason initial");
    expect(initial.record.revision_index == 1, "claim verify: first revision");
    expect(initial.record.verifier_version.empty(), "claim verify: human version is empty");
    expect(initial.record.calibration_eligibility == "provisional"
               && initial.record.egar_eligibility == "excluded",
           "claim verify: eligibility stays provisional and excluded");
    expect(repo->insertCm1ClaimVerification(initial.record), "claim verify: initial stored");
    expect(repo->insertCm1ClaimVerification(initial.record), "claim verify: replay insert is ignored");
    expect(repo->listCm1ClaimVerifications(answer.answer_id).size() == 1,
           "claim verify: replay does not add a row");

    submission.revision_reason = "initial";
    const auto secondInitial = Thoth::prepareCm1ClaimVerification(
        &*loadedAnswer, &*stored, &initial.record, submission, 318);
    expect(secondInitial.disposition == Thoth::Cm1RecordDisposition::Rejected,
           "claim verify: second initial rejected");

    submission.revision_reason = "correction";
    submission.outcome = "false";
    submission.source_ref = "relay-handbook-5";
    const auto correction = Thoth::prepareCm1ClaimVerification(
        &*loadedAnswer, &*stored, &initial.record, submission, 319);
    expect(correction.disposition == Thoth::Cm1RecordDisposition::Insert, "claim verify: correction accepted");
    expect(correction.record.outcome == "false", "claim verify: outcome false");
    expect(correction.record.evidence_label.empty(),
           "claim verify: factual correction has no evidence label");
    expect(correction.record.revision_reason == "correction", "claim verify: reason correction");
    expect(correction.record.revision_index == 2, "claim verify: correction index");
    expect(correction.record.supersedes_verification_id == initial.record.verification_id,
           "claim verify: correction points at the previous row");
    expect(repo->insertCm1ClaimVerification(correction.record), "claim verify: correction stored");
    expect(repo->insertCm1ClaimVerification(correction.record), "claim verify: correction replay ignored");

    submission.outcome = "unverifiable";
    submission.revision_reason = "initial";
    submission.verifier_kind = "automated_agent";
    submission.verifier_version = "agent-1";
    submission.method_id = "tool-check";
    submission.reviewer_id = "agent-alpha";
    submission.source_kind = "external_tool";
    submission.source_ref = "tool-run-19";
    const auto agent = Thoth::prepareCm1ClaimVerification(
        &*loadedAnswer, &*stored, &correction.record, submission, 320);
    expect(agent.disposition == Thoth::Cm1RecordDisposition::Rejected,
           "claim verify: another chain is not a correction of the human row");
    const auto agentInitial = Thoth::prepareCm1ClaimVerification(
        &*loadedAnswer, &*stored, nullptr, submission, 321);
    expect(agentInitial.disposition == Thoth::Cm1RecordDisposition::Insert,
           "claim verify: automated agent accepted");
    expect(agentInitial.record.outcome == "unverifiable", "claim verify: outcome unverifiable");
    expect(repo->insertCm1ClaimVerification(agentInitial.record), "claim verify: agent stored");

    submission.method_id = "other-tool";
    const auto otherMethod = Thoth::prepareCm1ClaimVerification(
        &*loadedAnswer, &*stored, &agentInitial.record, submission, 322);
    expect(otherMethod.disposition == Thoth::Cm1RecordDisposition::Rejected,
           "claim verify: a different method does not supersede");
    const auto otherMethodInitial = Thoth::prepareCm1ClaimVerification(
        &*loadedAnswer, &*stored, nullptr, submission, 323);
    expect(otherMethodInitial.disposition == Thoth::Cm1RecordDisposition::Insert,
           "claim verify: other method starts its own chain");
    expect(repo->insertCm1ClaimVerification(otherMethodInitial.record), "claim verify: other method stored");

    submission.method_id = "tool-check";
    submission.verifier_version = "agent-2";
    const auto nextVersion = Thoth::prepareCm1ClaimVerification(
        &*loadedAnswer, &*stored, &agentInitial.record, submission, 324);
    expect(nextVersion.disposition == Thoth::Cm1RecordDisposition::Rejected,
           "claim verify: a new verifier version does not supersede");
    const auto nextVersionInitial = Thoth::prepareCm1ClaimVerification(
        &*loadedAnswer, &*stored, nullptr, submission, 325);
    expect(repo->insertCm1ClaimVerification(nextVersionInitial.record), "claim verify: agent version stored");

    submission.verifier_kind = "external_service";
    submission.verifier_version = "svc-3";
    submission.method_id = "api-lookup";
    submission.reviewer_id = "gold-service";
    submission.outcome = "true";
    submission.source_kind = "external_service_record";
    submission.source_ref = "case-44";
    const auto service = Thoth::prepareCm1ClaimVerification(
        &*loadedAnswer, &*stored, nullptr, submission, 326);
    expect(service.disposition == Thoth::Cm1RecordDisposition::Insert,
           "claim verify: external service accepted");
    expect(repo->insertCm1ClaimVerification(service.record), "claim verify: service stored");

    expect(!stored->evidence_quote.empty() && !stored->evidence_chunk_sha256.empty(),
           "claim verify: fixture keeps the internal evidence identifiers");
    auto freshChain = [&](const std::string& method) {
        auto row = initial.record;
        row.method_id = method;
        row.verification_id = initial.record.verification_id + "~" + method;
        row.revision_reason = "initial";
        row.revision_index = 1;
        row.supersedes_verification_id.clear();
        row.created_at_ms = 400;
        return row;
    };
    auto refuse = [&](const Thoth::Cm1ClaimVerification& row, const std::string& name) {
        const auto before = repo->listCm1ClaimVerifications(answer.answer_id).size();
        expect(!repo->insertCm1ClaimVerification(row), name);
        expect(repo->listCm1ClaimVerifications(answer.answer_id).size() == before,
               name + " is not stored");
    };

    auto conflict = correction.record;
    conflict.outcome = "true";
    refuse(conflict, "claim verify: conflicting replay rejected");

    auto calibration = freshChain("bypass-calibration");
    calibration.calibration_eligibility = "authorized";
    refuse(calibration, "claim verify: direct authorized calibration rejected");
    auto egar = freshChain("bypass-egar");
    egar.egar_eligibility = "authorized";
    refuse(egar, "claim verify: direct authorized EGAR eligibility rejected");
    auto authorization = freshChain("bypass-authorization");
    authorization.revision_reason = "authorization";
    refuse(authorization, "claim verify: direct authorization reason rejected");

    auto directHumanVersion = freshChain("bypass-human-version");
    directHumanVersion.verifier_version = "1";
    refuse(directHumanVersion, "claim verify: direct human version rejected");
    auto agentGap = freshChain("bypass-agent");
    agentGap.verifier_kind = "automated_agent";
    agentGap.verifier_version.clear();
    refuse(agentGap, "claim verify: direct agent without version rejected");
    auto directOutcome = freshChain("bypass-outcome");
    directOutcome.outcome = "maybe";
    refuse(directOutcome, "claim verify: direct unknown outcome rejected");

    auto answerHash = freshChain("bypass-answer-hash");
    answerHash.content_sha256 = "not-the-answer-hash";
    refuse(answerHash, "claim verify: direct answer hash rejected");
    auto fabricated = freshChain("bypass-claim");
    fabricated.claim_id = stored->claim_id + ":fabricated";
    refuse(fabricated, "claim verify: fabricated claim id rejected");
    auto claimHash = freshChain("bypass-claim-hash");
    claimHash.claim_sha256 = "0123456789abcdef";
    refuse(claimHash, "claim verify: direct claim hash rejected");

    auto internalKind = freshChain("bypass-grag");
    internalKind.source_kind = "grag";
    refuse(internalKind, "claim verify: direct internal source kind rejected");
    auto directQuote = freshChain("bypass-quote");
    directQuote.source_ref = stored->evidence_quote;
    refuse(directQuote, "claim verify: direct evidence quote rejected");
    auto chunkRef = freshChain("bypass-chunk");
    chunkRef.source_ref = stored->evidence_chunk_sha256;
    refuse(chunkRef, "claim verify: direct evidence chunk rejected");

    auto labeledTruthRow = freshChain("bypass-label-outcome");
    labeledTruthRow.evidence_label = "supported";
    refuse(labeledTruthRow, "claim verify: direct evidence label with true outcome rejected");
    auto unknownLabel = freshChain("bypass-label");
    unknownLabel.outcome = "unverifiable";
    unknownLabel.evidence_label = "maybe";
    refuse(unknownLabel, "claim verify: direct unknown evidence label rejected");
    auto warmKind = freshChain("bypass-warm");
    warmKind.source_kind = "warm_memory";
    refuse(warmKind, "claim verify: direct warm-memory source rejected");
    auto heldOutMissing = freshChain("bypass-held-out");
    heldOutMissing.verifier_kind = "automated_agent";
    heldOutMissing.verifier_version = "cm1-corroboration-1";
    heldOutMissing.outcome = "unverifiable";
    heldOutMissing.evidence_label = "supported";
    heldOutMissing.source_kind = "held_out_passage";
    heldOutMissing.source_ref = std::string(64, 'a');
    refuse(heldOutMissing, "claim verify: held-out passage without an inclusion list rejected");

    auto skipped = correction.record;
    skipped.verification_id += "~skip";
    skipped.revision_index = 4;
    skipped.created_at_ms = 401;
    refuse(skipped, "claim verify: skipped revision rejected");
    auto wrongLink = correction.record;
    wrongLink.verification_id += "~link";
    wrongLink.revision_index = 3;
    wrongLink.supersedes_verification_id = initial.record.verification_id;
    wrongLink.created_at_ms = 402;
    refuse(wrongLink, "claim verify: supersedes must name the current revision");
    auto orphan = freshChain("bypass-orphan");
    orphan.revision_reason = "correction";
    orphan.revision_index = 2;
    orphan.supersedes_verification_id = correction.record.verification_id;
    refuse(orphan, "claim verify: correction without that chain rejected");

    expect(repo->insertCm1ClaimVerification(initial.record),
           "claim verify: identical initial replay still succeeds");
    expect(repo->insertCm1ClaimVerification(correction.record),
           "claim verify: identical correction replay still succeeds");

    const auto history = repo->listCm1ClaimVerifications(answer.answer_id);
    expect(history.size() == 6, "claim verify: five chains and the human correction remain");
    expect(history.front().verification_id == initial.record.verification_id
               && history.front().outcome == "true",
           "claim verify: the first revision remains");
    const auto humanLatest = repo->latestCm1ClaimVerification(
        stored->claim_id, "claim-check", "v1", "reviewer-a", "human", "", "desk-read");
    expect(humanLatest && humanLatest->verification_id == correction.record.verification_id
               && humanLatest->outcome == "false",
           "claim verify: human latest is the correction");
    const auto agentLatest = repo->latestCm1ClaimVerification(
        stored->claim_id, "claim-check", "v1", "agent-alpha", "automated_agent", "agent-1", "tool-check");
    expect(agentLatest && agentLatest->outcome == "unverifiable"
               && agentLatest->revision_index == 1,
           "claim verify: agent chain is unchanged");

    const auto judgmentAfter = repo->loadCm1EvidenceJudgment(judged->judgment_id);
    expect(judgmentAfter && judgmentAfter->payload_json == judgment_payload,
           "claim verify: judgment payload unchanged");
    const auto labelAfter = repo->latestCm1VerifiedLabel(answer.answer_id, "desk-check");
    expect(labelAfter && labelAfter->factual_bin == 8, "claim verify: factual label unchanged");
    const auto answerAfter = repo->loadCm1AnswerVersion(answer.answer_id);
    expect(answerAfter && answerAfter->content_sha256 == answer.content_sha256,
           "claim verify: answer version unchanged");
    return true;
}

std::string batchRef(const std::vector<std::string>& hashes) {
    auto ordered = hashes;
    std::sort(ordered.begin(), ordered.end());
    ordered.erase(std::unique(ordered.begin(), ordered.end()), ordered.end());
    std::string joined;
    for (const auto& hash : ordered) {
        if (!joined.empty()) joined.push_back('\n');
        joined += hash;
    }
    return joined;
}

bool testClaimCorroboration() {
    ::unsetenv("THOTH_CM1_CORROBORATION");
    expect(!Thoth::cm1CorroborationEnabled(), "corroboration: disabled by default");

    const auto migratedPath = makeTempDb();
    {
        sqlite3* raw = nullptr;
        expect(sqlite3_open(migratedPath.c_str(), &raw) == SQLITE_OK, "corroboration: migration db opens");
        const char* old_schema =
            "CREATE TABLE cm1_claim_verifications ("
            "  verification_id TEXT PRIMARY KEY,"
            "  claim_id TEXT NOT NULL,"
            "  claim_sha256 TEXT NOT NULL,"
            "  answer_id TEXT NOT NULL,"
            "  content_sha256 TEXT NOT NULL,"
            "  protocol_id TEXT NOT NULL,"
            "  protocol_version TEXT NOT NULL,"
            "  reviewer_id TEXT NOT NULL,"
            "  verifier_kind TEXT NOT NULL,"
            "  verifier_version TEXT NOT NULL,"
            "  method_id TEXT NOT NULL,"
            "  outcome TEXT NOT NULL,"
            "  source_kind TEXT NOT NULL,"
            "  source_ref TEXT NOT NULL,"
            "  calibration_eligibility TEXT NOT NULL,"
            "  egar_eligibility TEXT NOT NULL,"
            "  revision_reason TEXT NOT NULL,"
            "  supersedes_verification_id TEXT NOT NULL,"
            "  revision_index INTEGER NOT NULL,"
            "  created_at_ms INTEGER NOT NULL"
            ");";
        expect(sqlite3_exec(raw, old_schema, nullptr, nullptr, nullptr) == SQLITE_OK,
               "corroboration: legacy verification table created");
        sqlite3_close(raw);
    }
    {
        auto migrated = std::make_shared<Thoth::SQLiteMemoryRepository>(migratedPath.string());
        sqlite3* raw = nullptr;
        sqlite3_open(migratedPath.c_str(), &raw);
        sqlite3_stmt* info = nullptr;
        sqlite3_prepare_v2(raw, "PRAGMA table_info(cm1_claim_verifications);", -1, &info, nullptr);
        bool found = false;
        while (sqlite3_step(info) == SQLITE_ROW) {
            const auto* name = reinterpret_cast<const char*>(sqlite3_column_text(info, 1));
            if (name && std::string(name) == "evidence_label") found = true;
        }
        sqlite3_finalize(info);
        sqlite3_close(raw);
        expect(found, "corroboration: evidence_label column is added");
        (void)migrated;
    }

    const std::string answer_text = "The spare relay holds two red tokens.";
    const std::string included_body = "The spare relay holds two red tokens.";
    const std::string included_hash = Thoth::sha256Hex(included_body);
    const std::string decorated = "--- notes.txt ---\n" + included_body + "\n\n";
    expect(Thoth::sha256Hex(decorated) != included_hash,
           "corroboration: inclusion hash is the raw chunk content");
    const std::string near_body = included_body + "x";
    expect(Thoth::sha256Hex(near_body) != included_hash,
           "corroboration: a one-character change is a different hash");
    const std::string newline_body = "Alpha";
    expect(Thoth::sha256Hex(newline_body + "\n") != Thoth::sha256Hex(newline_body),
           "corroboration: a trailing newline is a different hash");

    const auto answer = Thoth::buildCm1AnswerVersion(
        "p-cor", "synthesize", "gen-cor", answer_text, 500);
    Thoth::Cm1EvidenceChunk cited;
    cited.content = "A cited handbook sentence.";
    cited.content_sha256 = Thoth::sha256Hex(cited.content);
    const auto judged = Thoth::runCm1EvidenceJudgment(
        answer, answer_text, {cited}, true, true, false,
        [&](const std::string&, int) {
            return completionWith(
                "{\"label\":\"supported\",\"evidence_quote\":\"A cited handbook sentence.\"}",
                "stop");
        },
        500);
    expect(judged.has_value(), "corroboration: judgment exists");
    if (!judged) return false;
    const std::string judgment_payload = judged->payload_json;

    const auto dbPath = makeTempDb();
    auto repo = std::make_shared<Thoth::SQLiteMemoryRepository>(dbPath.string());
    expect(repo->insertCm1AnswerVersion(answer), "corroboration: answer stored");
    expect(repo->insertCm1EvidenceJudgment(*judged), "corroboration: judgment stored");
    const auto loadedJudgment = repo->loadCm1EvidenceJudgment(judged->judgment_id);
    if (!loadedJudgment) return false;
    const auto claim_id = nlohmann::json::parse(loadedJudgment->payload_json)["claims"][0]
                              .value("claim_id", "");
    const auto stored = Thoth::findCm1StoredClaim(*loadedJudgment, claim_id);
    if (!stored) return false;

    nlohmann::json plan = {
        {"plan_id", "p-cor"},
        {"steps", nlohmann::json::array({{
            {"step_id", "synthesize"},
            {"result", {
                {"cm1_included_in_synthesis_available", true},
                {"cm1_included_content_sha256", nlohmann::json::array({included_hash})}
            }}
        }})}
    };
    auto active = activePlan("p-cor");
    active.steps_json = plan.dump();
    expect(repo->storeActivePlan(active), "corroboration: plan stored");
    const auto inclusion = repo->loadCm1SynthesisInclusion("p-cor", "synthesize");
    expect(inclusion && inclusion->content_sha256.size() == 1
               && inclusion->content_sha256.front() == included_hash,
           "corroboration: inclusion hashes load from the plan");

    bool called = false;
    auto disabled = Thoth::runCm1ClaimCorroboration(
        *repo, answer.answer_id, claim_id, answer_text,
        {{near_body, "handbook.md"}},
        [&](const std::string&, int) {
            called = true;
            return completionWith("{}", "stop");
        },
        510);
    expect(!called && !disabled.wrote && disabled.reason == "disabled",
           "corroboration: a disabled writer does not call the model");
    expect(repo->listCm1ClaimVerifications(answer.answer_id).empty(),
           "corroboration: a disabled writer stores nothing");

    ::setenv("THOTH_CM1_CORROBORATION", "1", 1);
    called = false;
    auto exact = Thoth::runCm1ClaimCorroboration(
        *repo, answer.answer_id, claim_id, answer_text,
        {{included_body, "notes.txt"}},
        [&](const std::string&, int) {
            called = true;
            return completionWith("{}", "stop");
        },
        511);
    expect(!called && exact.reason == "no_eligible_passage",
           "corroboration: exact included content is excluded");

    called = false;
    auto quoted = Thoth::runCm1ClaimCorroboration(
        *repo, answer.answer_id, claim_id, answer_text,
        {{cited.content, "other.txt"}},
        [&](const std::string&, int) {
            called = true;
            return completionWith("{}", "stop");
        },
        512);
    expect(!called && quoted.reason == "no_eligible_passage",
           "corroboration: the judgment quote is excluded");

    called = false;
    auto warm = Thoth::runCm1ClaimCorroboration(
        *repo, answer.answer_id, claim_id, answer_text,
        {{near_body, "warm_memory:session-1"}},
        [&](const std::string&, int) {
            called = true;
            return completionWith("{}", "stop");
        },
        513);
    expect(!called && warm.reason == "no_eligible_passage",
           "corroboration: warm-memory content is excluded");

    Thoth::Cm1ClaimVerificationSubmission blocked;
    blocked.claim_id = stored->claim_id;
    blocked.claim_text = stored->claim_text;
    blocked.answer_id = answer.answer_id;
    blocked.answer_text = answer_text;
    blocked.protocol_id = "direct-held-out";
    blocked.protocol_version = "v1";
    blocked.reviewer_id = "bypass";
    blocked.verifier_kind = "automated_agent";
    blocked.verifier_version = "cm1-corroboration-1";
    blocked.method_id = "direct";
    blocked.outcome = "unverifiable";
    blocked.evidence_label = "supported";
    blocked.source_kind = "held_out_passage";
    blocked.source_ref = included_hash;
    blocked.revision_reason = "initial";
    const auto loadedAnswer = repo->loadCm1AnswerVersion(answer.answer_id);
    const auto blockedDraft = Thoth::prepareCm1ClaimVerification(
        &*loadedAnswer, &*stored, nullptr, blocked, 515);
    expect(blockedDraft.disposition == Thoth::Cm1RecordDisposition::Insert,
           "corroboration: prepare accepts a hash the repository must still reject");
    expect(!repo->insertCm1ClaimVerification(blockedDraft.record),
           "corroboration: inclusion hash rejected at insert");
    blocked.source_ref = "ledger.txt";
    const auto pathRef = Thoth::prepareCm1ClaimVerification(
        &*loadedAnswer, &*stored, nullptr, blocked, 516);
    expect(pathRef.disposition == Thoth::Cm1RecordDisposition::Insert
               && !repo->insertCm1ClaimVerification(pathRef.record),
           "corroboration: a non-hash passage reference is rejected");

    const std::string hash_low = Thoth::sha256Hex("batch-a");
    const std::string hash_high = Thoth::sha256Hex("batch-b");
    const std::string first_hash = hash_low < hash_high ? hash_low : hash_high;
    const std::string second_hash = hash_low < hash_high ? hash_high : hash_low;
    expect(first_hash < second_hash, "corroboration: fixture hashes are ordered");
    std::string uppercase = first_hash;
    for (char& ch : uppercase) {
        if (ch >= 'a' && ch <= 'f') {
            ch = static_cast<char>(ch - 'a' + 'A');
            break;
        }
    }
    expect(uppercase != first_hash, "corroboration: fixture has a hex letter to raise");

    blocked.source_kind = "held_out_batch";
    blocked.evidence_label = "insufficient_evidence";
    const std::vector<std::pair<std::string, std::string>> malformed = {
        {batchRef({included_hash, second_hash}), "an included passage in a batch is rejected"},
        {batchRef({cited.content_sha256, second_hash}), "a cited judgment chunk in a batch is rejected"},
        {second_hash + "\n" + first_hash, "an unsorted batch is rejected"},
        {first_hash + "\n" + first_hash, "a duplicate batch hash is rejected"},
        {uppercase + "\n" + second_hash, "an uppercase batch hash is rejected"},
        {first_hash, "a one-hash batch is rejected"},
        {"trusted-source", "free-text batch provenance is rejected"},
    };
    for (const auto& [ref, name] : malformed) {
        blocked.source_ref = ref;
        auto draft = Thoth::prepareCm1ClaimVerification(
            &*loadedAnswer, &*stored, nullptr, blocked, 517);
        if (draft.disposition == Thoth::Cm1RecordDisposition::Insert) {
            expect(!repo->insertCm1ClaimVerification(draft.record), "corroboration: " + name);
        } else {
            expect(false, "corroboration: prepare should accept the shape the repository rejects: " + name);
        }
    }
    expect(repo->listCm1ClaimVerifications(answer.answer_id).empty(),
           "corroboration: malformed held-out references are not stored");

    const std::string held_body = "An outside ledger says the spare relay holds two red tokens.";
    const std::string held_hash = Thoth::sha256Hex(held_body);
    expect(held_hash != included_hash && held_hash != cited.content_sha256,
           "corroboration: the held-out passage is outside the inclusion set");
    auto stored_row = Thoth::runCm1ClaimCorroboration(
        *repo, answer.answer_id, claim_id, answer_text,
        {{held_body, "ledger.txt"}},
        [&](const std::string&, int) {
            return completionWith(
                "{\"label\":\"supported\",\"evidence_quote\":\"the spare relay holds two red tokens.\"}",
                "stop");
        },
        520);
    expect(stored_row.wrote && stored_row.reason == "stored" && stored_row.record,
           "corroboration: held-out assessment stored");
    if (!stored_row.record) {
        ::unsetenv("THOTH_CM1_CORROBORATION");
        return false;
    }
    const auto& row = *stored_row.record;
    expect(row.verifier_kind == "automated_agent"
               && row.verifier_version == Thoth::kCm1CorroborationVerifierVersion
               && row.method_id == Thoth::kCm1CorroborationMethodId
               && row.outcome == "unverifiable"
               && row.evidence_label == "supported"
               && row.calibration_eligibility == "provisional"
               && row.egar_eligibility == "excluded"
               && row.source_kind == "held_out_passage"
               && row.source_ref == held_hash,
           "corroboration: the row is a provisional passage assessment");
    expect(row.source_ref != "ledger.txt",
           "corroboration: the file name is not stored on the verification row");

    auto replay = Thoth::runCm1ClaimCorroboration(
        *repo, answer.answer_id, claim_id, answer_text,
        {{held_body, "ledger.txt"}},
        [&](const std::string&, int) {
            return completionWith(
                "{\"label\":\"supported\",\"evidence_quote\":\"the spare relay holds two red tokens.\"}",
                "stop");
        },
        521);
    expect(!replay.wrote && replay.reason == "unchanged",
           "corroboration: the same assessment does not append a row");
    expect(repo->insertCm1ClaimVerification(row), "corroboration: identical replay succeeds");
    auto conflict = row;
    conflict.evidence_label = "contradicted";
    expect(!repo->insertCm1ClaimVerification(conflict),
           "corroboration: conflicting replay rejected");
    expect(repo->listCm1ClaimVerifications(answer.answer_id).size() == 1,
           "corroboration: replay leaves one row");

    blocked.source_kind = "held_out_batch";
    blocked.evidence_label = "insufficient_evidence";
    blocked.revision_reason = "initial";
    blocked.source_ref = batchRef({first_hash, second_hash});
    const auto validBatch = Thoth::prepareCm1ClaimVerification(
        &*loadedAnswer, &*stored, nullptr, blocked, 518);
    expect(validBatch.disposition == Thoth::Cm1RecordDisposition::Insert
               && repo->insertCm1ClaimVerification(validBatch.record)
               && repo->insertCm1ClaimVerification(validBatch.record),
           "corroboration: a sorted held-out batch is stored and replays");
    expect(validBatch.record.source_ref == first_hash + "\n" + second_hash,
           "corroboration: batch membership is the hash list");
    blocked.revision_reason = "correction";
    blocked.source_ref = batchRef({first_hash, Thoth::sha256Hex("batch-c")});
    const auto batchCorrection = Thoth::prepareCm1ClaimVerification(
        &*loadedAnswer, &*stored, &validBatch.record, blocked, 519);
    expect(batchCorrection.disposition == Thoth::Cm1RecordDisposition::Insert
               && batchCorrection.record.revision_index == 2
               && batchCorrection.record.outcome == "unverifiable"
               && repo->insertCm1ClaimVerification(batchCorrection.record),
           "corroboration: a batch correction stays unverifiable");

    auto corrected = Thoth::runCm1ClaimCorroboration(
        *repo, answer.answer_id, claim_id, answer_text,
        {{held_body, "ledger.txt"}},
        [&](const std::string&, int) {
            return completionWith(
                "{\"label\":\"contradicted\",\"evidence_quote\":\"the spare relay holds two red tokens.\"}",
                "stop");
        },
        522);
    expect(corrected.wrote && corrected.record
               && corrected.record->revision_index == 2
               && corrected.record->revision_reason == "correction"
               && corrected.record->evidence_label == "contradicted"
               && corrected.record->outcome == "unverifiable"
               && corrected.record->supersedes_verification_id == row.verification_id,
           "corroboration: a changed label appends an unverifiable correction");

    auto timed_out = Thoth::runCm1ClaimCorroboration(
        *repo, answer.answer_id, claim_id, answer_text,
        {{"A second outside note about red tokens.", "note.txt"}},
        [&](const std::string&, int) {
            Thoth::Cm1EvidenceCompletion completion;
            completion.called = true;
            completion.ok = false;
            completion.error = "timeout";
            return completion;
        },
        523);
    expect(!timed_out.wrote && timed_out.reason == "timeout", "corroboration: timeout writes nothing");

    auto invalid = Thoth::runCm1ClaimCorroboration(
        *repo, answer.answer_id, claim_id, answer_text,
        {{"A second outside note about red tokens.", "note.txt"}},
        [&](const std::string&, int) { return completionWith("not json", "stop"); },
        524);
    expect(!invalid.wrote && invalid.reason == "invalid_output",
           "corroboration: invalid output writes nothing");

    const std::string shared = "shared red token sentence";
    auto ambiguous = Thoth::runCm1ClaimCorroboration(
        *repo, answer.answer_id, claim_id, answer_text,
        {{"left " + shared, "left.txt"}, {"right " + shared, "right.txt"}},
        [&](const std::string&, int) {
            return completionWith(
                "{\"label\":\"supported\",\"evidence_quote\":\"" + shared + "\"}", "stop");
        },
        525);
    expect(!ambiguous.wrote && ambiguous.reason == "ambiguous",
           "corroboration: a quote in two passages writes nothing");

    const std::string first = "Ledger A does not mention the relay.";
    const std::string second = "Ledger B does not mention the relay.";
    auto insufficient = Thoth::runCm1ClaimCorroboration(
        *repo, answer.answer_id, claim_id, answer_text,
        {{first, "a.txt"}, {second, "b.txt"}},
        [&](const std::string&, int) {
            return completionWith("{\"label\":\"insufficient_evidence\",\"evidence_quote\":\"\"}", "stop");
        },
        526);
    const auto expected_batch = batchRef({Thoth::sha256Hex(first), Thoth::sha256Hex(second)});
    called = false;
    auto duplicate = Thoth::runCm1ClaimCorroboration(
        *repo, answer.answer_id, claim_id, answer_text,
        {{near_body, "a.txt"}, {near_body, "b.txt"}},
        [&](const std::string&, int) {
            called = true;
            return completionWith(
                "{\"label\":\"supported\",\"evidence_quote\":\"two red tokens.x\"}", "stop");
        },
        527);
    expect(called && duplicate.wrote && duplicate.record
               && duplicate.record->source_kind == "held_out_passage"
               && duplicate.record->source_ref == Thoth::sha256Hex(near_body),
           "corroboration: identical passage text is assessed once");

    Thoth::Cm1ClaimVerificationSubmission manual;
    manual.claim_id = stored->claim_id;
    manual.claim_text = stored->claim_text;
    manual.answer_id = answer.answer_id;
    manual.answer_text = answer_text;
    manual.protocol_id = "claim-check";
    manual.protocol_version = "v1";
    manual.reviewer_id = "reviewer-a";
    manual.verifier_kind = "human";
    manual.method_id = "desk-read";
    manual.outcome = "true";
    manual.source_kind = "external_manual";
    manual.source_ref = "relay-handbook-4";
    manual.revision_reason = "initial";
    const auto manualDraft = Thoth::prepareCm1ClaimVerification(
        &*loadedAnswer, &*stored, nullptr, manual, 530);
    expect(manualDraft.disposition == Thoth::Cm1RecordDisposition::Insert
               && manualDraft.record.evidence_label.empty()
               && repo->insertCm1ClaimVerification(manualDraft.record),
           "corroboration: an external manual reference stays valid");

    expect(insufficient.wrote && insufficient.record
               && insufficient.record->evidence_label == "insufficient_evidence"
               && insufficient.record->outcome == "unverifiable"
               && insufficient.record->source_kind == "held_out_batch"
               && insufficient.record->source_ref == expected_batch
               && expected_batch.find(Thoth::sha256Hex(first)) != std::string::npos
               && expected_batch.find(Thoth::sha256Hex(second)) != std::string::npos
               && expected_batch.size() == Thoth::sha256Hex(first).size()
                      + 1 + Thoth::sha256Hex(second).size(),
           "corroboration: the batch lists each passage hash");

    const auto judgmentAfter = repo->loadCm1EvidenceJudgment(judged->judgment_id);
    expect(judgmentAfter && judgmentAfter->payload_json == judgment_payload,
           "corroboration: judgment payload unchanged");
    ::unsetenv("THOTH_CM1_CORROBORATION");
    expect(!Thoth::cm1CorroborationEnabled(), "corroboration: flag restored to off");
    return true;
}

bool testEvidenceSegmentation() {
    const auto budget = Thoth::segmentCm1EvidenceClaims("The budget is 900 seconds.");
    expect(budget.size() == 1 && budget[0] == "The budget is 900 seconds.",
           "evidence segment: short factual sentence");
    const auto decimal = Thoth::segmentCm1EvidenceClaims("w_t = -0.05.");
    expect(decimal.size() == 1 && decimal[0] == "w_t = -0.05.",
           "evidence segment: decimal stays in the claim");
    const auto abbrev = Thoth::segmentCm1EvidenceClaims("See e.g. the timeout.");
    expect(abbrev.size() == 1 && abbrev[0] == "See e.g. the timeout.",
           "evidence segment: abbreviation stays in the claim");
    const auto listed = Thoth::segmentCm1EvidenceClaims("1. Alpha is on. Beta is off.");
    expect(listed.size() == 2 && listed[0] == "Alpha is on." && listed[1] == "Beta is off.",
           "evidence segment: numbered item splits sentences");
    const auto code = Thoth::segmentCm1EvidenceClaims("Use `a.b` here. Done.");
    expect(code.size() == 2 && code[0] == "Use `a.b` here." && code[1] == "Done.",
           "evidence segment: backtick span stays intact");
    return true;
}

bool testEvidenceJudgmentGate() {
    ::unsetenv("THOTH_CM1_EVIDENCE");
    expect(!Thoth::cm1EvidenceEnabled(), "evidence: default off");

    const auto answer = evidenceAnswer();
    expect(!Thoth::runCm1EvidenceJudgment(
               Thoth::Cm1AnswerVersionRecord{}, "The budget is 900 seconds.", {}, true, true, false,
               [](const std::string&, int) { return Thoth::Cm1EvidenceCompletion{}; }, 1)
                .has_value(),
           "evidence: missing answer is not_recorded");

    Thoth::Cm1EvidenceChunk passage;
    passage.content = "The budget is 900 seconds.";
    passage.content_sha256 = Thoth::sha256Hex(passage.content);
    const std::vector<Thoth::Cm1EvidenceChunk> included = {passage};
    const std::string answer_text = "The budget is 900 seconds.";

    auto judge = [&](const Thoth::Cm1EvidenceCompletion& completion) {
        return Thoth::runCm1EvidenceJudgment(
            answer, answer_text, included, true, true, false,
            [&](const std::string&, int) { return completion; }, 60);
    };

    const auto supported = judge(completionWith(
        "{\"label\":\"supported\",\"evidence_quote\":\"The budget is 900 seconds.\"}", "stop"));
    const auto supportedPayload = nlohmann::json::parse(supported->payload_json);
    expect(supportedPayload["claims_total"] == 1, "evidence: one claim is the total");
    expect(supportedPayload["claims_evaluated"] == 1, "evidence: one claim is evaluated");
    expect(supportedPayload["claims_not_judged"] == 0, "evidence: a judged claim is not unjudged");
    expect(supportedPayload["claims"][0]["evaluation_status"] == "available",
           "evidence: supported is available");
    expect(supportedPayload["claims"][0]["label"] == "supported", "evidence: supported label");
    expect(supportedPayload["claims"][0]["evidence_chunk_sha256"] == passage.content_sha256,
           "evidence: quote binds one included chunk");
    expect(!supportedPayload.contains("predicted_correctness"), "evidence: no correctness field");
    expect(!supportedPayload.contains("goal_confidence"), "evidence: no confidence field");
    expect(!supportedPayload.contains("success_score"), "evidence: no success field");

    const auto contradicted = judge(completionWith(
        "{\"label\":\"contradicted\",\"evidence_quote\":\"The budget is 900 seconds.\"}", "stop"));
    expect(nlohmann::json::parse(contradicted->payload_json)["claims"][0]["label"] == "contradicted",
           "evidence: contradicted label");

    const auto insufficient = judge(completionWith(
        "{\"label\":\"insufficient_evidence\",\"evidence_quote\":\"\"}", "stop"));
    const auto insufficientClaim = nlohmann::json::parse(insufficient->payload_json)["claims"][0];
    expect(insufficientClaim["label"] == "insufficient_evidence", "evidence: insufficient label");
    expect(insufficientClaim["evidence_quote"] == "", "evidence: insufficient quote is empty");
    expect(!insufficientClaim.contains("evidence_chunk_sha256"),
           "evidence: insufficient has no chunk");

    const auto outside = judge(completionWith(
        "{\"label\":\"supported\",\"evidence_quote\":\"selected but not included\"}", "stop"));
    expect(nlohmann::json::parse(outside->payload_json)["claims"][0]["unavailable_reason"]
               == "quote_not_in_evidence",
           "evidence: excluded quote is unavailable");

    Thoth::Cm1EvidenceChunk second = passage;
    second.content_sha256 = "other-hash";
    const auto ambiguous = Thoth::runCm1EvidenceJudgment(
        answer, answer_text, {passage, second}, true, true, false,
        [&](const std::string&, int) {
            return completionWith(
                "{\"label\":\"supported\",\"evidence_quote\":\"The budget is 900 seconds.\"}",
                "stop");
        },
        60);
    expect(nlohmann::json::parse(ambiguous->payload_json)["claims"][0]["unavailable_reason"]
               == "quote_ambiguous",
           "evidence: quote in two chunks is unavailable");

    const auto quotedInsufficient = judge(completionWith(
        "{\"label\":\"insufficient_evidence\",\"evidence_quote\":\"The budget is 900 seconds.\"}",
        "stop"));
    expect(nlohmann::json::parse(quotedInsufficient->payload_json)["claims"][0]["unavailable_reason"]
               == "quote_present",
           "evidence: insufficient with a quote is unavailable");

    const auto missingQuote = judge(completionWith(
        "{\"label\":\"supported\",\"evidence_quote\":\"\"}", "stop"));
    expect(nlohmann::json::parse(missingQuote->payload_json)["claims"][0]["unavailable_reason"]
               == "quote_required",
           "evidence: support without a quote is unavailable");

    const auto truncated = judge(completionWith("{\"label\":\"supported\"", "length"));
    expect(nlohmann::json::parse(truncated->payload_json)["claims"][0]["unavailable_reason"] == "length",
           "evidence: length finish is unavailable");

    const auto broken = judge(completionWith("not json", "stop"));
    expect(nlohmann::json::parse(broken->payload_json)["claims"][0]["unavailable_reason"] == "parse",
           "evidence: broken JSON is unavailable");

    const auto twoObjects = judge(completionWith(
        "{\"label\":\"supported\",\"evidence_quote\":\"The budget is 900 seconds.\"}"
        "{\"label\":\"contradicted\",\"evidence_quote\":\"The budget is 900 seconds.\"}",
        "stop"));
    expect(nlohmann::json::parse(twoObjects->payload_json)["claims"][0]["unavailable_reason"] == "parse",
           "evidence: two objects are unavailable");

    const auto provider = judge(completionWith("", "", false));
    expect(nlohmann::json::parse(provider->payload_json)["claims"][0]["unavailable_reason"]
               == "provider_error",
           "evidence: provider error is unavailable");

    int calls = 0;
    const auto mocked = Thoth::runCm1EvidenceJudgment(
        answer, answer_text, included, true, true, true,
        [&](const std::string&, int) {
            ++calls;
            return completionWith("{}", "stop");
        },
        60);
    expect(calls == 0, "evidence: mock synthesis does not call the model");
    expect(nlohmann::json::parse(mocked->payload_json)["claims"][0]["unavailable_reason"]
               == "mock_synthesis",
           "evidence: mock synthesis is unavailable");

    std::string many;
    for (int i = 0; i < 9; ++i) {
        if (i) many += " ";
        many += "Claim" + std::to_string(i) + " stands.";
    }
    int judged_calls = 0;
    const auto capped = Thoth::runCm1EvidenceJudgment(
        answer, many, included, true, true, false,
        [&](const std::string&, int) {
            ++judged_calls;
            return completionWith(
                "{\"label\":\"insufficient_evidence\",\"evidence_quote\":\"\"}", "stop");
        },
        60);
    const auto cappedPayload = nlohmann::json::parse(capped->payload_json);
    expect(judged_calls == Thoth::kCm1EvidenceClaimCap, "evidence: cap limits model calls");
    expect(cappedPayload["claims"].size() == 8, "evidence: eight claims are judged");
    expect(cappedPayload["claims_total"] == 9, "evidence: claims_total counts every segment");
    expect(cappedPayload["claims_evaluated"] == 8, "evidence: claims_evaluated counts judged rows");
    expect(cappedPayload["claims_not_judged"] == 1, "evidence: the ninth claim is not judged");
    expect(cappedPayload["claims_total"] ==
               cappedPayload["claims_evaluated"].get<int>() + cappedPayload["claims_not_judged"].get<int>(),
           "evidence: claim counts add up");

    const auto dbPath = makeTempDb();
    auto repo = std::make_shared<Thoth::SQLiteMemoryRepository>(dbPath.string());
    Thoth::Cm1SnapshotBuildInput input;
    input.plan_id = "p7";
    input.step_id = "synthesize";
    input.snapshot_type = "synthesis_observed";
    input.lifecycle_stage = "after_synthesis";
    input.observed_at_ms = 60;
    auto snap = Thoth::buildCm1Snapshot(input);
    const std::string rawFeatures = snap.features_json;
    expect(repo->commitStepCheckpoint(activePlan("p7"), std::nullopt, episode("p7", 0), snap, answer),
           "evidence: checkpoint");
    expect(repo->insertCm1EvidenceJudgment(*supported), "evidence: insert judgment");
    Thoth::Cm1EvidenceJudgmentRecord replacement = *supported;
    replacement.payload_json = "{\"replaced\":true}";
    replacement.created_at_ms = 99;
    expect(repo->insertCm1EvidenceJudgment(replacement), "evidence: replay insert");
    const auto loaded = repo->loadCm1EvidenceJudgment(supported->judgment_id);
    expect(loaded && loaded->payload_json == supported->payload_json, "evidence: replay keeps the first row");
    expect(loaded->estimator_version == Thoth::kCm1EvidenceVersion, "evidence: version cm1-evidence-1");
    const auto raw = repo->loadCm1Snapshot(snap.snapshot_id);
    expect(raw && raw->features_json == rawFeatures, "evidence: snapshot bytes unchanged");
    expect(raw->estimator_status == Thoth::kCm1EstimatorStatus, "evidence: estimator stays none_active");
    expect(repo->listCm1UserFeedback(answer.answer_id).empty(), "evidence: feedback untouched");
    expect(repo->listCm1VerifiedLabels(answer.answer_id).empty(), "evidence: verified labels untouched");
    const auto storedAnswer = repo->loadCm1AnswerVersion(answer.answer_id);
    expect(storedAnswer && storedAnswer->content_sha256 == answer.content_sha256,
           "evidence: answer hash unchanged");
    return true;
}

bool testEvidenceJsonWrapper() {
    const auto answer = evidenceAnswer();
    Thoth::Cm1EvidenceChunk passage;
    passage.content = "The retrieval weight w_t is negative.";
    passage.content_sha256 = Thoth::sha256Hex(passage.content);
    const std::string claim = "Trajectory weight is below zero.";
    const std::string object =
        "{\"label\":\"supported\",\"evidence_quote\":\"The retrieval weight w_t is negative.\"}";
    const std::string captured =
        "To determine if the claim \"Trajectory weight is below zero\" is supported by the evidence, "
        "we need to analyze the provided passage.\n\n"
        "The passage states: \"The retrieval weight w_t is negative.\"\n\n"
        "1. The claim uses the term \"trajectory weight,\" while the passage mentions \"retrieval weight.\"\n"
        "2. The claim states that the weight is \"below zero,\" which is equivalent to being negative.\n\n"
        "Since the passage explicitly states that the retrieval weight \\( w_t \\) is negative, and the "
        "claim is essentially saying the same thing (that the weight is below zero), the claim is "
        "supported by the evidence.\n\n"
        + object;

    auto judge = [&](const std::string& text) {
        return Thoth::runCm1EvidenceJudgment(
            answer, claim, {passage}, true, true, false,
            [&](const std::string&, int) { return completionWith(text, "stop"); }, 60);
    };

    const auto leading = nlohmann::json::parse(judge(captured)->payload_json)["claims"][0];
    expect(leading["evaluation_status"] == "available",
           "evidence wrapper: captured leading explanation is available");
    expect(leading["label"] == "supported", "evidence wrapper: captured response is supported");
    expect(leading["evidence_quote"] == passage.content,
           "evidence wrapper: captured quote is the passage");
    expect(leading["evidence_chunk_sha256"] == passage.content_sha256,
           "evidence wrapper: captured quote binds the included chunk");

    const auto trailing = nlohmann::json::parse(
        judge(object + "\nThe passages establish the claim.")->payload_json)["claims"][0];
    expect(trailing["label"] == "supported", "evidence wrapper: text after the object is accepted");
    expect(trailing["evidence_quote"] == passage.content,
           "evidence wrapper: trailing text does not change the quote");

    const auto two = nlohmann::json::parse(
        judge("First reading " + object + " " +
              "{\"label\":\"contradicted\",\"evidence_quote\":\"The retrieval weight w_t is negative.\"}")
            ->payload_json)["claims"][0];
    expect(two["unavailable_reason"] == "parse", "evidence wrapper: two objects are rejected");

    const auto unclosed = nlohmann::json::parse(
        judge("The answer is {\"label\":\"supported\",\"evidence_quote\":\"The retrieval weight w_t is negative.\"")
            ->payload_json)["claims"][0];
    expect(unclosed["unavailable_reason"] == "parse", "evidence wrapper: an unclosed object is rejected");

    const auto malformed = nlohmann::json::parse(
        judge("Note {\"label\":supported,\"evidence_quote\":\"The retrieval weight w_t is negative.\"} done")
            ->payload_json)["claims"][0];
    expect(malformed["unavailable_reason"] == "parse", "evidence wrapper: malformed JSON is rejected");

    const auto badLabel = nlohmann::json::parse(
        judge("Result {\"label\":\"maybe\",\"evidence_quote\":\"The retrieval weight w_t is negative.\"}")
            ->payload_json)["claims"][0];
    expect(badLabel["unavailable_reason"] == "schema", "evidence wrapper: an invalid label is rejected");

    const auto missingQuote = nlohmann::json::parse(
        judge("Result {\"label\":\"supported\",\"evidence_quote\":\"not in the passage\"}")
            ->payload_json)["claims"][0];
    expect(missingQuote["unavailable_reason"] == "quote_not_in_evidence",
           "evidence wrapper: a quote outside the passage is rejected");

    Thoth::Cm1EvidenceChunk duplicate = passage;
    duplicate.content_sha256 = "other-hash";
    const auto ambiguous = Thoth::runCm1EvidenceJudgment(
        answer, claim, {passage, duplicate}, true, true, false,
        [&](const std::string&, int) {
            return completionWith("See the passage. " + object, "stop");
        },
        60);
    expect(nlohmann::json::parse(ambiguous->payload_json)["claims"][0]["unavailable_reason"]
               == "quote_ambiguous",
           "evidence wrapper: a quote in two chunks is rejected");
    return true;
}

bool testEvidenceDeadlineAndState() {
    const auto answer = evidenceAnswer();
    Thoth::Cm1EvidenceChunk passage;
    passage.content = "The budget is 900 seconds.";
    passage.content_sha256 = Thoth::sha256Hex(passage.content);
    const std::vector<Thoth::Cm1EvidenceChunk> included = {passage};

    std::int64_t now = 0;
    int calls = 0;
    int seen_timeout = 0;
    const auto exhausted = Thoth::runCm1EvidenceJudgment(
        answer, "One stands. Two stands. Three stands.", included, true, true, false,
        [&](const std::string&, int timeout_seconds) {
            ++calls;
            seen_timeout = timeout_seconds;
            now = 2500;
            return completionWith(
                "{\"label\":\"insufficient_evidence\",\"evidence_quote\":\"\"}", "stop");
        },
        60, 2, [&]() { return now; });
    const auto exhaustedPayload = nlohmann::json::parse(exhausted->payload_json);
    expect(calls == 1, "evidence deadline: later claims are not called");
    expect(seen_timeout == 2, "evidence deadline: the call uses the remaining seconds");
    expect(exhaustedPayload["claims"].size() == 1, "evidence deadline: unissued claims have no row");
    expect(exhaustedPayload["claims"][0]["label"] == "insufficient_evidence",
           "evidence deadline: an issued call keeps its label");
    expect(exhaustedPayload["claims_total"] == 3, "evidence deadline: total counts every claim");
    expect(exhaustedPayload["claims_evaluated"] == 1, "evidence deadline: only the issued call is evaluated");
    expect(exhaustedPayload["claims_not_judged"] == 2, "evidence deadline: unissued claims are not judged");

    calls = 0;
    const auto timedOut = Thoth::runCm1EvidenceJudgment(
        answer, "One stands. Two stands.", included, true, true, false,
        [&](const std::string&, int) {
            ++calls;
            auto completion = completionWith("", "", false);
            completion.error = "timeout";
            return completion;
        },
        60, 10, []() { return static_cast<std::int64_t>(0); });
    const auto timedPayload = nlohmann::json::parse(timedOut->payload_json);
    expect(calls == 1, "evidence timeout: a timed-out call stops the rest");
    expect(timedPayload["claims"].size() == 1, "evidence timeout: the unissued claim has no row");
    expect(timedPayload["claims"][0]["evaluation_status"] == "unavailable",
           "evidence timeout: the issued call is unavailable");
    expect(timedPayload["claims"][0]["unavailable_reason"] == "timeout",
           "evidence timeout: the reason is timeout");
    expect(!timedPayload["claims"][0].contains("label"),
           "evidence timeout: a timeout is not insufficient evidence");
    expect(timedPayload["claims_total"] == 2, "evidence timeout: total stays the segment count");
    expect(timedPayload["claims_evaluated"] == 1, "evidence timeout: the timed-out call was evaluated");
    expect(timedPayload["claims_not_judged"] == 1, "evidence timeout: the remaining claim is not judged");
    expect(timedPayload["claims_total"] ==
               timedPayload["claims_evaluated"].get<int>() + timedPayload["claims_not_judged"].get<int>(),
           "evidence timeout: claim counts add up");

    calls = 0;
    const auto unissued = Thoth::runCm1EvidenceJudgment(
        answer, "One stands. Two stands.", included, true, true, false,
        [&](const std::string&, int) {
            ++calls;
            return completionWith(
                "{\"label\":\"insufficient_evidence\",\"evidence_quote\":\"\"}", "stop");
        },
        60, 0, []() { return static_cast<std::int64_t>(0); });
    const auto unissuedPayload = nlohmann::json::parse(unissued->payload_json);
    expect(calls == 0, "evidence deadline: no call is issued when the budget is already spent");
    expect(unissuedPayload["claims"].empty(), "evidence deadline: spent budget writes no claim rows");
    expect(unissuedPayload["claims_total"] == 2 && unissuedPayload["claims_evaluated"] == 0 &&
               unissuedPayload["claims_not_judged"] == 2,
           "evidence deadline: an unissued claim is not judged");

    const int observing = static_cast<int>(Thoth::ControllerState::OBSERVING_RESULT);
    const int aborted = static_cast<int>(Thoth::ControllerState::ABORTED);
    expect(Thoth::cm1EvidenceStateUnchanged(observing, observing),
           "evidence state: an unchanged observation may return to idle");
    expect(!Thoth::cm1EvidenceStateUnchanged(observing, aborted),
           "evidence state: a newer abort is left in place");
    return true;
}

bool testDeterministicSnapshotIds() {
    const auto a = Thoth::makeCm1SnapshotId("p", "s", "retrieval_observed", "after_retrieval");
    const auto b = Thoth::makeCm1SnapshotId("p", "s", "retrieval_observed", "after_retrieval");
    expect(a == b, "ids: deterministic");
    expect(a != Thoth::makeCm1SnapshotId("p", "s2", "retrieval_observed", "after_retrieval"),
           "ids: step distinguishes");
    return true;
}

bool testEvidencePromptStructure() {
    const std::string claim = "Each claim is judged by its own model call.";
    const std::string stop =
        "Emit exactly one JSON object, stop immediately afterward, and copy any quotation only from the evidence passage, not from the claim.\n";
    Thoth::Cm1EvidenceChunk passage;
    passage.content = "The evidence judge sends one model call for each claim.";
    passage.content_sha256 = Thoth::sha256Hex(passage.content);

    const std::string expected =
        "Judge whether the claim is established by the evidence passages.\n"
        "Use only those passages.\n"
        "Reply with one JSON object and no other text:\n"
        "{\"label\":\"supported\",\"evidence_quote\":\"exact span from one passage\"}\n"
        "{\"label\":\"contradicted\",\"evidence_quote\":\"exact span from one passage\"}\n"
        "{\"label\":\"insufficient_evidence\",\"evidence_quote\":\"\"}\n"
        "supported means the passages establish the claim.\n"
        "contradicted means the passages conflict with the claim.\n"
        "insufficient_evidence means the passages establish neither conclusion.\n"
        "Copy the quote exactly from one passage. Leave it empty for insufficient_evidence.\n"
        "\n"
        "EVIDENCE PASSAGE START\n"
        "[passage " + passage.content_sha256 + "]\n" + passage.content + "\n"
        "\n"
        "EVIDENCE PASSAGE END\n"
        "\n"
        "CLAIM START\n" + claim + "\n"
        "CLAIM END\n"
        "\n"
        + stop;
    const std::string prompt = Thoth::buildCm1EvidencePrompt(claim, {passage});
    expect(prompt == expected, "evidence prompt: one passage matches the verified structure");

    const auto evidence_at = prompt.find("EVIDENCE PASSAGE START\n");
    const auto evidence_end = prompt.find("EVIDENCE PASSAGE END\n");
    const auto claim_at = prompt.find("CLAIM START\n");
    const auto claim_end = prompt.find("CLAIM END\n");
    const auto stop_at = prompt.find(stop);
    expect(evidence_at != std::string::npos && evidence_end > evidence_at && claim_at > evidence_end
               && claim_end > claim_at && stop_at > claim_end && prompt.find(stop, stop_at + 1) == std::string::npos,
           "evidence prompt: evidence, claim, and one stop sentence are in order");
    const std::string evidence_body = prompt.substr(evidence_at, evidence_end - evidence_at);
    const std::string claim_body = prompt.substr(claim_at, claim_end - claim_at);
    expect(evidence_body.find(passage.content) != std::string::npos
               && evidence_body.find(claim) == std::string::npos,
           "evidence prompt: the passage is inside the evidence block");
    expect(claim_body.find(claim) != std::string::npos
               && claim_body.find(passage.content) == std::string::npos,
           "evidence prompt: the claim is inside the claim block");
    expect(prompt.find("[Evidence]\n") == std::string::npos && prompt.find("[Claim]\n") == std::string::npos,
           "evidence prompt: evidence and claim blocks are closed");
    const std::string supported_example =
        "{\"label\":\"supported\",\"evidence_quote\":\"exact span from one passage\"}\n";
    const std::string contradicted_example =
        "{\"label\":\"contradicted\",\"evidence_quote\":\"exact span from one passage\"}\n";
    const std::string insufficient_example =
        "{\"label\":\"insufficient_evidence\",\"evidence_quote\":\"\"}\n";
    expect(prompt.find(supported_example) < evidence_at
               && prompt.find(contradicted_example) < evidence_at
               && prompt.find(insufficient_example) < evidence_at
               && prompt.find(supported_example) == prompt.rfind(supported_example)
               && prompt.find(contradicted_example) == prompt.rfind(contradicted_example)
               && prompt.find(insufficient_example) == prompt.rfind(insufficient_example),
           "evidence prompt: the three JSON examples stay in the instruction section");

    Thoth::Cm1EvidenceChunk second;
    second.content = "Plan revision runs after a failed step.";
    second.content_sha256 = Thoth::sha256Hex(second.content);
    const std::string multi = Thoth::buildCm1EvidencePrompt(claim, {passage, second});
    const auto multi_start = multi.find("EVIDENCE PASSAGE START\n");
    const auto multi_end = multi.find("EVIDENCE PASSAGE END\n");
    const std::string both = multi.substr(multi_start, multi_end - multi_start);
    expect(multi.find("EVIDENCE PASSAGE START\n") == multi.rfind("EVIDENCE PASSAGE START\n")
               && multi.find("EVIDENCE PASSAGE END\n") == multi.rfind("EVIDENCE PASSAGE END\n")
               && both.find(passage.content) < both.find(second.content)
               && both.find(claim) == std::string::npos,
           "evidence prompt: several passages share one closed evidence block");
    expect(multi.find("CLAIM START\n" + claim + "\nCLAIM END\n") != std::string::npos
               && multi.rfind(stop) > multi.find("CLAIM END\n"),
           "evidence prompt: several passages still close the claim before the stop sentence");

    const std::string empty = Thoth::buildCm1EvidencePrompt(claim, {});
    const auto empty_start = empty.find("EVIDENCE PASSAGE START\n");
    const auto empty_end = empty.find("EVIDENCE PASSAGE END\n");
    const std::string empty_body = empty.substr(empty_start, empty_end - empty_start);
    expect(empty_body.find("No evidence passages were included.") != std::string::npos
               && empty_body.find(claim) == std::string::npos
               && empty.find("CLAIM START\n" + claim + "\nCLAIM END\n") != std::string::npos
               && empty.rfind(stop) > empty.find("CLAIM END\n"),
           "evidence prompt: an empty passage list stays inside the closed evidence block");
    return true;
}

} // namespace

int main() {
    testSourceGrouping();
    testMissingBreakdownsAndDenominator();
    testSynthesisInclusionAndTruncation();
    testAnswerVersionAndIdempotency();
    testTelemetryWriteFailurePreservesCheckpoint();
    testGenerationIdsSurviveRestartShape();
    testAnswerFeedbackAndVerifiedLabels();
    testClaimVerification();
    testClaimCorroboration();
    testEvidenceSegmentation();
    testEvidenceJudgmentGate();
    testEvidenceJsonWrapper();
    testEvidencePromptStructure();
    testEvidenceDeadlineAndState();
    testDeterministicSnapshotIds();
    testBreadthScoring();
    testBreadthReplayAndDisabledByDefault();

    std::cout << "cm1_telemetry_tests failures=" << g_failures << "\n";
    return g_failures == 0 ? 0 : 1;
}
