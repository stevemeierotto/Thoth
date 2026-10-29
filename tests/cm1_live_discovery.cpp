/*
 * Copyright (c) 2026 Steve Meierotto
 *
 * Thoth — EVAL-2 live claim-discovery retrieval check.
 * Uses IndexManager::retrieveChunks and runCm1ClaimDiscovery against the
 * embedding server at 127.0.0.1:8081. Not a CTest target.
 *
 * Licensed under the MIT License (see LICENSE in project root)
 */

#include "alp_sha256.h"
#include "cm1_telemetry.h"
#include "config.h"
#include "embedding_engine.h"
#include "index_manager.h"
#include "sqlite_memory_repository.h"

#include <json.hpp>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <string>
#include <unistd.h>
#include <unordered_map>
#include <unordered_set>
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
    const auto dir = fs::temp_directory_path() / ("thoth_cm1_live_discovery_" + std::to_string(::getpid()) + "_"
                                                  + std::to_string(std::chrono::steady_clock::now()
                                                                       .time_since_epoch()
                                                                       .count()));
    fs::create_directories(dir);
    return dir / "memory.db";
}

Thoth::MemoryRepository::ActivePlanRecord activePlan(const std::string& plan_id) {
    Thoth::MemoryRepository::ActivePlanRecord rec;
    rec.plan_id = plan_id;
    rec.session_id = "s-live";
    rec.goal = "goal";
    rec.steps_json = "{}";
    rec.current_index = 0;
    rec.controller_state = "IDLE";
    rec.created_at_ms = 1;
    rec.updated_at_ms = 1;
    return rec;
}

Thoth::Cm1EvidenceCompletion insufficientCompletion() {
    Thoth::Cm1EvidenceCompletion completion;
    completion.called = true;
    completion.ok = true;
    completion.text = "{\"label\":\"insufficient_evidence\",\"evidence_quote\":\"\"}";
    completion.finish_reason = "stop";
    completion.generation_id = "gen-live-discovery";
    return completion;
}

struct Passage {
    const char* role;
    const char* file;
    const char* text;
};

const char* kClaim = "The spare relay holds two red tokens.";
const char* kNotes = "Notes: the spare relay holds two red tokens.";
const char* kPainted = "The spare relay was painted blue last year.";
const char* kCited = "A cited handbook sentence.";
const char* kHeldOut = "Bay 4 inventory lists a spare relay beside the pump.";

const Passage kPassages[] = {
    {"synthesis", "synthesis.txt", kNotes},
    {"synthesis", "synthesis.txt", kPainted},
    {"cited", "handbook.txt", kCited},
    {"held_out", "ledger.txt", kHeldOut},
    {"unrelated", "unrelated-01.txt", "The quarterly wheat harvest in Lyon exceeded forecasts."},
    {"unrelated", "unrelated-02.txt", "A porcelain kiln in Kyoto fired overnight."},
    {"unrelated", "unrelated-03.txt", "The orchestra tuned to a flat oboe A."},
    {"unrelated", "unrelated-04.txt", "Glacier ice retreated from the fjord last spring."},
    {"unrelated", "unrelated-05.txt", "Copper prices fell after the Santiago strike."},
    {"unrelated", "unrelated-06.txt", "The library catalogued medieval maps of the Nile."},
    {"unrelated", "unrelated-07.txt", "A red fox crossed the frozen canal at dusk."},
    {"unrelated", "unrelated-08.txt", "Sailors repaired the mainsail before the monsoon."},
    {"unrelated", "unrelated-09.txt", "The chemist weighed dried lavender for the tincture."},
    {"unrelated", "unrelated-10.txt", "Voters approved a new footbridge over the creek."},
};

constexpr int kDiscoveryTopK = 8;

}  // namespace

int main() {
    ::setenv("THOTH_EMBED_STRICT", "1", 1);
    ::setenv("THOTH_EMBED_BASE_URL", "http://127.0.0.1:8081", 1);
    ::setenv("THOTH_CM1_CORROBORATION", "1", 1);

    Config config;
    config.embedding_model = "/models/nomic-embed-text.gguf";
    config.embed_base_url = "http://127.0.0.1:8081";
    EmbeddingEngine engine(EmbeddingEngine::Method::External, &config);
    IndexManager index(&engine);

    std::cout << "live-discovery model=" << engine.getModelName()
              << " endpoint=http://127.0.0.1:8081 top_k=" << kDiscoveryTopK << "\n";

    std::unordered_map<std::string, const Passage*> by_text;
    for (const auto& passage : kPassages) {
        CodeChunk chunk;
        chunk.fileName = passage.file;
        chunk.code = passage.text;
        chunk.embedding = engine.embed(passage.text);
        std::cout << "embedded role=" << passage.role
                  << " file=" << passage.file
                  << " dim=" << chunk.embedding.size() << "\n";
        expect(chunk.embedding.size() == 768, std::string("embedding dim 768 for ") + passage.file);
        if (chunk.embedding.size() != 768) {
            std::cout << "live-discovery failures=" << g_failures << "\n";
            return 1;
        }
        by_text.emplace(passage.text, &passage);
        index.addChunkToIndex(std::move(chunk));
    }
    expect(index.getChunks().size() == std::size(kPassages), "every passage was stored");

    const std::string answer_text = kClaim;
    const std::string notes_hash = Thoth::sha256Hex(kNotes);
    const auto answer = Thoth::buildCm1AnswerVersion(
        "p-live", "synthesize", "gen-live", answer_text, 800);
    Thoth::Cm1EvidenceChunk cited;
    cited.content = kCited;
    cited.content_sha256 = Thoth::sha256Hex(cited.content);
    const auto judged = Thoth::runCm1EvidenceJudgment(
        answer, answer_text, {cited}, true, true, false,
        [](const std::string&, int) {
            Thoth::Cm1EvidenceCompletion completion;
            completion.called = true;
            completion.ok = true;
            completion.text = "{\"label\":\"supported\",\"evidence_quote\":\"A cited handbook sentence.\"}";
            completion.finish_reason = "stop";
            completion.generation_id = "gen-judge";
            return completion;
        },
        800);
    expect(judged.has_value(), "judgment exists");
    if (!judged) return 1;

    nlohmann::json chunks = nlohmann::json::array({
        {{"file", "synthesis.txt"}, {"content", kNotes}},
        {{"file", "synthesis.txt"}, {"content", kPainted}},
        {{"file", "handbook.txt"}, {"content", kCited}},
    });
    nlohmann::json plan = {
        {"plan_id", "p-live"},
        {"steps", nlohmann::json::array({
            {{"step_id", "retrieve"},
             {"result", {{"data", {{"chunks", chunks}}}}}},
            {{"step_id", "synthesize"},
             {"result", {
                 {"cm1_included_in_synthesis_available", true},
                 {"cm1_included_content_sha256", nlohmann::json::array({notes_hash})}
             }}}
        })}
    };

    auto repo = std::make_shared<Thoth::SQLiteMemoryRepository>(makeTempDb().string());
    expect(repo->insertCm1AnswerVersion(answer), "answer stored");
    expect(repo->insertCm1EvidenceJudgment(*judged), "judgment stored");
    auto active = activePlan("p-live");
    active.steps_json = plan.dump();
    expect(repo->storeActivePlan(active), "plan stored");
    const auto loaded = repo->loadCm1EvidenceJudgment(judged->judgment_id);
    if (!loaded) return 1;
    const std::string claim_id = nlohmann::json::parse(loaded->payload_json)["claims"][0]
                                     .value("claim_id", "");
    const auto stored = Thoth::findCm1StoredClaim(*loaded, claim_id);
    if (!stored) return 1;
    std::cout << "query_claim=" << stored->claim_text << "\n";

    std::unordered_set<std::string> synthesis_files;
    for (const auto& passage : kPassages) {
        const std::string hash = Thoth::sha256Hex(passage.text);
        if (hash == notes_hash || hash == cited.content_sha256) {
            synthesis_files.insert(passage.file);
        }
    }

    const auto ranked = index.retrieveChunks(stored->claim_text, static_cast<int>(std::size(kPassages)));
    std::cout << "ranked_count=" << ranked.size() << "\n";
    int held_rank = -1;
    float held_score = 0.0f;
    int outside_unrelated = 0;
    float outside_unrelated_score = 0.0f;
    std::string outside_unrelated_text;
    std::vector<std::string> eligible_texts;
    std::vector<std::string> dropped_texts;
    for (std::size_t i = 0; i < ranked.size(); ++i) {
        const auto found = by_text.find(ranked[i].first);
        const char* role = found == by_text.end() ? "unknown" : found->second->role;
        const char* file = found == by_text.end() ? "" : found->second->file;
        const bool outside = static_cast<int>(i) >= kDiscoveryTopK;
        const bool excluded = !outside && synthesis_files.count(file) > 0;
        const char* disposition = outside ? "outside_top8" : (excluded ? "excluded_source" : "eligible");
        std::cout << "rank=" << (i + 1)
                  << " score=" << std::fixed << std::setprecision(6) << ranked[i].second
                  << " role=" << role
                  << " file=" << file
                  << " disposition=" << disposition
                  << " text=" << ranked[i].first << "\n";
        if (ranked[i].first == kHeldOut) {
            held_rank = static_cast<int>(i);
            held_score = ranked[i].second;
        }
        if (outside && std::string(role) == "unrelated") {
            ++outside_unrelated;
            outside_unrelated_score = ranked[i].second;
            outside_unrelated_text = ranked[i].first;
        }
        if (!outside && !excluded) eligible_texts.push_back(ranked[i].first);
        if (outside || excluded) dropped_texts.push_back(ranked[i].first);
    }

    expect(ranked.size() > static_cast<std::size_t>(kDiscoveryTopK), "more passages than topK were indexed");
    expect(held_rank >= 0 && held_rank < kDiscoveryTopK, "held-out passage is inside the top eight");
    expect(outside_unrelated > 0, "at least one unrelated passage falls outside the top eight");
    expect(!outside_unrelated_text.empty() && outside_unrelated_score < held_score,
           "the excluded unrelated passage scores below the held-out passage");

    bool called = false;
    std::string prompt;
    const auto found = Thoth::runCm1ClaimDiscovery(
        index, *repo, answer.answer_id, claim_id, answer_text,
        [&](const std::string& sent, int) {
            called = true;
            prompt = sent;
            return insufficientCompletion();
        },
        810);
    const std::string held_hash = Thoth::sha256Hex(kHeldOut);
    expect(called, "the corroboration writer called the model");
    expect(prompt.find(kHeldOut) != std::string::npos, "the writer received the held-out passage");
    expect(prompt.find("[passage " + held_hash + "]") != std::string::npos,
           "the writer received the held-out content hash");
    expect(prompt.find(outside_unrelated_text) == std::string::npos,
           "a passage outside the top eight did not reach the writer");
    expect(prompt.find(kNotes) == std::string::npos, "included synthesis text was excluded");
    expect(prompt.find(kPainted) == std::string::npos, "another span of the synthesis file was excluded");
    expect(prompt.find(kCited) == std::string::npos, "cited evidence was excluded");
    for (const auto& text : dropped_texts) {
        expect(prompt.find(text) == std::string::npos,
               "a dropped passage did not reach the writer");
    }
    for (const auto& text : eligible_texts) {
        expect(prompt.find(text) != std::string::npos,
               "an eligible top-eight passage reached the writer");
    }

    const auto rows = repo->listCm1ClaimVerifications(answer.answer_id);
    expect(found.wrote && found.reason == "stored" && rows.size() == 1,
           "the writer stored one corroboration row");
    if (rows.size() == 1) {
        const auto& row = rows.front();
        std::cout << "writer reason=" << found.reason
                  << " outcome=" << row.outcome
                  << " evidence_label=" << row.evidence_label
                  << " calibration=" << row.calibration_eligibility
                  << " egar=" << row.egar_eligibility
                  << " source_kind=" << row.source_kind
                  << " source_ref=" << row.source_ref << "\n";
        expect(row.outcome == "unverifiable"
                   && row.evidence_label == "insufficient_evidence"
                   && row.calibration_eligibility == "provisional"
                   && row.egar_eligibility == "excluded"
                   && row.source_ref.find(held_hash) != std::string::npos,
               "the stored judgment stays unverifiable, provisional, and EGAR-excluded");
    }

    ::unsetenv("THOTH_CM1_CORROBORATION");
    std::cout << "live-discovery failures=" << g_failures << "\n";
    return g_failures == 0 ? 0 : 1;
}
