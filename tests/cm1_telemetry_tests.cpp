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
