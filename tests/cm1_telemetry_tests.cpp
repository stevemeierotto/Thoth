/*
 * Copyright (c) 2026 Steve Meierotto
 *
 * Thoth — CM-1 Phase 1 telemetry unit tests (disposable databases only)
 *
 * Licensed under the MIT License (see LICENSE in project root)
 */

#include "cm1_telemetry.h"
#include "alp_sha256.h"
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

bool testDeterministicSnapshotIds() {
    const auto a = Thoth::makeCm1SnapshotId("p", "s", "retrieval_observed", "after_retrieval");
    const auto b = Thoth::makeCm1SnapshotId("p", "s", "retrieval_observed", "after_retrieval");
    expect(a == b, "ids: deterministic");
    expect(a != Thoth::makeCm1SnapshotId("p", "s2", "retrieval_observed", "after_retrieval"),
           "ids: step distinguishes");
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
    testDeterministicSnapshotIds();
    testBreadthScoring();
    testBreadthReplayAndDisabledByDefault();

    std::cout << "cm1_telemetry_tests failures=" << g_failures << "\n";
    return g_failures == 0 ? 0 : 1;
}
