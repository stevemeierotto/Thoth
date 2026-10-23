/*
 * Copyright (c) 2026 Steve Meierotto
 *
 * Thoth — CM-1 live evidence-judgment benchmark.
 * Sixteen independent claims, two runs, production generateCall.
 * Not a CTest target. Does not open the controller or a development database.
 *
 * Licensed under the MIT License (see LICENSE in project root)
 */

#include "alp_sha256.h"
#include "cm1_telemetry.h"
#include "generation_call.h"
#include "inference_client.h"
#include "inference_endpoint.h"
#include "llm_interface.h"
#include "llm_timeout_policy.h"

#include <json.hpp>

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <unistd.h>
#include <vector>

namespace {

std::string g_trace_directory;
bool g_prompt_experiment = false;
bool g_separation_experiment = false;
bool g_revised_benchmark = false;
bool g_placement_experiment = false;
bool g_example_ablation = false;
bool g_core_benchmark = false;

// Benchmark-only. Matches the sentence emitted by buildCm1EvidencePrompt.
// Experiment modes do not append a second copy when the builder already included it.
const char* kPromptExperimentInstruction =
    "Emit exactly one JSON object, stop immediately afterward, and copy any quotation only from the evidence passage, not from the claim.\n";

struct SegmentCheck {
    const char* input;
    std::vector<std::string> claims;
};

struct Fixture {
    const char* id;
    const char* kind;
    const char* claim;
    const char* evidence;
    const char* expected_label;
    const char* gold_quote;
};

struct Observed {
    std::string id;
    std::string kind;
    std::string expected_label;
    std::string gold_quote;
    std::string evidence;
    std::string status;
    std::string label;
    std::string quote;
    std::string reason;
    std::int64_t elapsed_ms = 0;
    int timeout_seconds = 0;
    std::string generation_id;
    std::string finish_reason;
};

struct RunScore {
    bool quote_provenance = true;
    bool absent_class = true;
    int paraphrase_hits = 0;
    int contradicted_hits = 0;
    int label_hits = 0;
    int quote_hits = 0;
    int unavailable = 0;
    bool pass = false;
};

const std::vector<SegmentCheck>& segmentChecks() {
    static const std::vector<SegmentCheck> checks = {
        {"The budget is 900 seconds.", {"The budget is 900 seconds."}},
        {"w_t = -0.05.", {"w_t = -0.05."}},
        {"See e.g. the timeout.", {"See e.g. the timeout."}},
        {"1. Alpha is on. Beta is off.", {"Alpha is on.", "Beta is off."}},
        {"Use `a.b` here. Done.", {"Use `a.b` here.", "Done."}},
    };
    return checks;
}

const std::vector<Fixture>& fixtures() {
    static const std::vector<Fixture> items = {
        {"paraphrase-1", "paraphrase",
         "Trajectory weight is below zero.",
         "The retrieval weight w_t is negative.",
         "supported", "The retrieval weight w_t is negative."},
        {"paraphrase-2", "paraphrase",
         "A successful step changes graph edge weights.",
         "Graph reinforcement updates edge weights after a successful step.",
         "supported",
         "Graph reinforcement updates edge weights after a successful step."},
        {"paraphrase-3", "paraphrase",
         "Claims are judged with separate model calls.",
         "The evidence judge sends one model call for each claim.",
         "supported", "The evidence judge sends one model call for each claim."},
        {"paraphrase-4", "paraphrase",
         "The judge is limited to quoting included passages.",
         "Included passages are the only text the judge may quote.",
         "supported", "Included passages are the only text the judge may quote."},

        {"negation-1", "negation",
         "The evidence feature is enabled by default.",
         "The evidence feature is disabled unless its flag is set.",
         "contradicted", "The evidence feature is disabled unless its flag is set."},
        {"negation-2", "negation",
         "Plan revision does not run after a failed step.",
         "Plan revision runs after a failed step.",
         "contradicted", "Plan revision runs after a failed step."},
        {"negation-3", "negation",
         "The controller does not return to idle after a successful step.",
         "The controller returns to idle after a successful step.",
         "contradicted", "The controller returns to idle after a successful step."},
        {"negation-4", "negation",
         "Mock synthesis still calls the evidence model.",
         "Mock synthesis skips the evidence model.",
         "contradicted", "Mock synthesis skips the evidence model."},

        {"number-1", "number",
         "The shared generation timeout is 90 seconds.",
         "The shared generation timeout is 900 seconds.",
         "contradicted", "The shared generation timeout is 900 seconds."},
        {"number-2", "number",
         "The judge reads at most 512 completion tokens.",
         "The judge reads at most 256 completion tokens.",
         "contradicted", "The judge reads at most 256 completion tokens."},
        {"number-3", "number",
         "The live suite contains 8 claims.",
         "The live suite contains 16 claims.",
         "contradicted", "The live suite contains 16 claims."},
        {"number-4", "number",
         "The default temperature for ordinary generation is 0.9.",
         "The default temperature for ordinary generation is 0.7.",
         "contradicted", "The default temperature for ordinary generation is 0.7."},

        {"absent-1", "absent",
         "The embedding model emits 768 dimensions.",
         "The shared generation timeout is 900 seconds.",
         "insufficient_evidence", ""},
        {"absent-2", "absent",
         "The operator rated this answer 9 out of 10.",
         "Included passages are the only text the judge may quote.",
         "insufficient_evidence", ""},
        {"absent-3", "absent",
         "The GUI displays a confidence percentage.",
         "The controller returns to idle after a successful step.",
         "insufficient_evidence", ""},
        {"absent-4", "absent",
         "The plan was resumed from a crash.",
         "Graph reinforcement updates edge weights after a successful step.",
         "insufficient_evidence", ""},
    };
    return items;
}

// Revised benchmark cm1-revised-1. Same sixteen fixtures, with paraphrase-1's
// claim replaced. The original fixture list above is left unchanged.
const std::vector<Fixture>& revisedFixtures() {
    static const std::vector<Fixture> items = [] {
        std::vector<Fixture> copy = fixtures();
        for (auto& item : copy) {
            if (std::string(item.id) == "paraphrase-1") {
                item.claim = "w_t is less than zero.";
            }
        }
        return copy;
    }();
    return items;
}

bool revisedSetReady(std::string& why) {
    const Fixture* original = nullptr;
    for (const auto& fixture : fixtures()) {
        if (std::string(fixture.id) == "paraphrase-1") original = &fixture;
    }
    if (original == nullptr || std::string(original->claim) != "Trajectory weight is below zero.") {
        why = "original paraphrase-1 fixture is no longer the baseline claim";
        return false;
    }
    if (revisedFixtures().size() != fixtures().size()) {
        why = "revised benchmark does not contain 16 fixtures";
        return false;
    }
    for (const auto& fixture : revisedFixtures()) {
        const auto got = Thoth::segmentCm1EvidenceClaims(fixture.claim);
        if (got.size() != 1 || got[0] != fixture.claim) {
            why = std::string("revised fixture claim does not segment to itself: ") + fixture.id;
            return false;
        }
        if (std::string(fixture.id) == "paraphrase-1"
            && std::string(fixture.claim) != "w_t is less than zero.") {
            why = "revised paraphrase-1 claim is not the replacement";
            return false;
        }
    }
    return true;
}

// Core reliability set cm1-core-1. Historical fixtures() stay unchanged.
const std::vector<Fixture>& coreFixtures() {
    static const std::vector<Fixture> items = [] {
        std::vector<Fixture> copy = fixtures();
        for (auto& item : copy) {
            const std::string id(item.id);
            if (id == "paraphrase-1") {
                item.claim = "w_t is less than zero.";
            } else if (id == "paraphrase-3") {
                item.claim = "Each claim is judged by its own model call.";
            } else if (id == "paraphrase-4") {
                item.claim = "Pausing keeps the controller in its current state.";
                item.evidence = "A pause sets the pause flag and leaves the controller state unchanged.";
                item.gold_quote = "A pause sets the pause flag and leaves the controller state unchanged.";
            }
        }
        return copy;
    }();
    return items;
}

bool coreSetReady(std::string& why) {
    const Fixture* paraphrase_1 = nullptr;
    const Fixture* paraphrase_3 = nullptr;
    const Fixture* paraphrase_4 = nullptr;
    for (const auto& fixture : fixtures()) {
        const std::string id(fixture.id);
        if (id == "paraphrase-1") paraphrase_1 = &fixture;
        if (id == "paraphrase-3") paraphrase_3 = &fixture;
        if (id == "paraphrase-4") paraphrase_4 = &fixture;
    }
    if (paraphrase_1 == nullptr || std::string(paraphrase_1->claim) != "Trajectory weight is below zero."
        || paraphrase_3 == nullptr
        || std::string(paraphrase_3->claim) != "Claims are judged with separate model calls."
        || paraphrase_4 == nullptr
        || std::string(paraphrase_4->claim) != "The judge is limited to quoting included passages."
        || std::string(paraphrase_4->evidence) != "Included passages are the only text the judge may quote.") {
        why = "historical paraphrase fixtures were changed";
        return false;
    }
    if (coreFixtures().size() != 16) {
        why = "cm1-core-1 does not contain 16 fixtures";
        return false;
    }
    for (const auto& fixture : coreFixtures()) {
        const auto got = Thoth::segmentCm1EvidenceClaims(fixture.claim);
        if (got.size() != 1 || got[0] != fixture.claim) {
            why = std::string("core fixture claim does not segment to itself: ") + fixture.id;
            return false;
        }
        const std::string id(fixture.id);
        if (id == "paraphrase-1" && std::string(fixture.claim) != "w_t is less than zero.") {
            why = "core paraphrase-1 claim is not the approved text";
            return false;
        }
        if (id == "paraphrase-3" && (std::string(fixture.claim) != "Each claim is judged by its own model call."
            || std::string(fixture.evidence) != "The evidence judge sends one model call for each claim."
            || std::string(fixture.gold_quote) != fixture.evidence)) {
            why = "core paraphrase-3 is not the approved fixture";
            return false;
        }
        if (id == "paraphrase-4"
            && (std::string(fixture.claim) != "Pausing keeps the controller in its current state."
                || std::string(fixture.evidence) != "A pause sets the pause flag and leaves the controller state unchanged."
                || std::string(fixture.gold_quote) != fixture.evidence)) {
            why = "core paraphrase-4 is not the approved fixture";
            return false;
        }
    }
    return true;
}

bool segmentationReady(std::string& why) {
    for (const auto& check : segmentChecks()) {
        const auto got = Thoth::segmentCm1EvidenceClaims(check.input);
        if (got != check.claims) {
            why = std::string("segmentation fixture mismatch: ") + check.input;
            return false;
        }
    }
    for (const auto& fixture : fixtures()) {
        const auto got = Thoth::segmentCm1EvidenceClaims(fixture.claim);
        if (got.size() != 1 || got[0] != fixture.claim) {
            why = std::string("fixture claim does not segment to itself: ") + fixture.id;
            return false;
        }
    }
    return true;
}

bool timeoutIsApolloDefault(std::string& why) {
    const char* raw = std::getenv("THOTH_LLM_TIMEOUT_SECONDS");
    if (raw == nullptr || raw[0] == '\0') return true;
    if (std::string(raw) == "900") return true;
    why = "THOTH_LLM_TIMEOUT_SECONDS is set and is not 900";
    return false;
}

bool inferencePathIsLive(std::string& why) {
    if (const char* dev = std::getenv("THOTH_TEST_SUITE_DEV")) {
        const std::string value(dev);
        if (value == "1" || value == "true" || value == "TRUE" || value == "yes" || value == "YES") {
            why = "THOTH_TEST_SUITE_DEV would replace generateCall with the test-suite mock";
            return false;
        }
    }
    if (const char* mock = std::getenv("THOTH_MOCK_LLM_UNAVAILABLE")) {
        if (mock[0] != '\0') {
            why = "THOTH_MOCK_LLM_UNAVAILABLE would replace generateCall";
            return false;
        }
    }
    return true;
}

// One structural change on top of the prompt experiment: close the evidence
// passage and the claim in separate blocks. Production prompt text is unchanged.
std::string fenceClaimAndEvidence(const std::string& prompt) {
    const std::string evidence_mark = "[Evidence]\n";
    const std::string claim_mark = "[Claim]\n";
    const auto evidence_at = prompt.find(evidence_mark);
    const auto claim_at = prompt.find(claim_mark);
    if (evidence_at == std::string::npos || claim_at == std::string::npos || claim_at < evidence_at) {
        return prompt;
    }
    const std::string evidence_body = prompt.substr(
        evidence_at + evidence_mark.size(), claim_at - (evidence_at + evidence_mark.size()));
    const std::string claim_body = prompt.substr(claim_at + claim_mark.size());
    return prompt.substr(0, evidence_at)
        + "EVIDENCE PASSAGE START\n" + evidence_body + "EVIDENCE PASSAGE END\n\n"
        + "CLAIM START\n" + claim_body + "CLAIM END\n\n";
}

std::string removeExampleObjects(const std::string& prompt) {
    const std::string examples =
        "{\"label\":\"supported\",\"evidence_quote\":\"exact span from one passage\"}\n"
        "{\"label\":\"contradicted\",\"evidence_quote\":\"exact span from one passage\"}\n"
        "{\"label\":\"insufficient_evidence\",\"evidence_quote\":\"\"}\n";
    const auto at = prompt.find(examples);
    if (at == std::string::npos || prompt.find(examples, at + examples.size()) != std::string::npos) {
        return {};
    }
    std::string out = prompt;
    out.erase(at, examples.size());
    return out;
}

bool isolateTraceDirectory(std::string& directory) {
    std::string pattern = g_core_benchmark
        ? std::string("/tmp/thoth-cm1-core-XXXXXX")
        : g_example_ablation
        ? std::string("/tmp/thoth-cm1-examples-XXXXXX")
        : g_placement_experiment
        ? std::string("/tmp/thoth-cm1-placement-XXXXXX")
        : g_revised_benchmark
            ? std::string("/tmp/thoth-cm1-revised-XXXXXX")
            : g_separation_experiment
                ? std::string("/tmp/thoth-cm1-separate-XXXXXX")
                : g_prompt_experiment
                    ? std::string("/tmp/thoth-cm1-prompt-XXXXXX")
                    : std::string("/tmp/thoth-cm1-live-XXXXXX");
    std::vector<char> buffer(pattern.begin(), pattern.end());
    buffer.push_back('\0');
    if (mkdtemp(buffer.data()) == nullptr) return false;
    directory = buffer.data();
    g_trace_directory = directory;
    if (setenv("THOTH_WORKSPACE_PATH", directory.c_str(), 1) != 0) return false;
    if (setenv("THOTH_LOGS_PATH", directory.c_str(), 1) != 0) return false;
    return true;
}

Thoth::Cm1EvidenceCompletion callProductionModel(LLMInterface& llm,
                                                 const std::string& prompt,
                                                 int timeout_seconds,
                                                 const std::string& fixture_id,
                                                 int run_index,
                                                 std::int64_t& elapsed_ms,
                                                 int& used_timeout,
                                                 std::string& generation_id,
                                                 std::string& finish_reason) {
    Thoth::GenerationCallContext call;
    call.task_id = fixture_id;
    call.plan_id = "cm1-live-evidence";
    call.session_id = "cm1-live-evidence";
    call.call_type = "evidence_judgment";
    used_timeout = timeout_seconds;
    std::string sent = prompt;
    if (g_example_ablation) {
        sent = removeExampleObjects(sent);
        if (sent.empty()) {
            std::cerr << "cm1-live-evidence STOP: example JSON objects were not found once\n";
            std::exit(2);
        }
    }
    if (g_separation_experiment) sent = fenceClaimAndEvidence(sent);
    if (g_prompt_experiment && sent.find(kPromptExperimentInstruction) == std::string::npos) {
        if (g_placement_experiment) {
            const std::string evidence_mark = "EVIDENCE PASSAGE START\n";
            const auto evidence_at = sent.find(evidence_mark);
            if (evidence_at == std::string::npos) {
                sent += kPromptExperimentInstruction;
            } else {
                sent.insert(evidence_at, std::string(kPromptExperimentInstruction) + "\n");
            }
        } else {
            sent += kPromptExperimentInstruction;
        }
    }
    const Thoth::GenerationOutcome generated = llm.generateCall(
        sent, Thoth::kCm1EvidenceNumPredict, {}, call, 0.0,
        static_cast<long>(std::max(1, timeout_seconds)));
    elapsed_ms = generated.elapsed_ms;
    generation_id = generated.generation_id;
    finish_reason = generated.finish_reason;
    Thoth::Cm1EvidenceCompletion completion;
    completion.called = true;
    completion.ok = generated.ok;
    if (!generated.ok && generated.error.find("Timeout was reached") != std::string::npos) {
        completion.error = "timeout";
    } else {
        completion.error = generated.error;
    }
    completion.text = generated.text;
    completion.finish_reason = generated.finish_reason;
    completion.generation_id = generated.generation_id;
    if (!g_trace_directory.empty()) {
        const std::string stem = g_trace_directory + "/" + fixture_id
            + (g_prompt_experiment ? "-run" + std::to_string(run_index) : "")
            + "-completion";
        std::ofstream raw(stem + ".txt");
        raw << generated.text;
        nlohmann::json meta = {
            {"fixture_id", fixture_id},
            {"ok", generated.ok},
            {"error", generated.error},
            {"finish_reason", generated.finish_reason},
            {"generation_id", generated.generation_id},
            {"elapsed_ms", generated.elapsed_ms},
            {"requested_max_tokens", generated.requested_max_tokens},
            {"text", generated.text},
        };
        if (g_prompt_experiment) {
            meta["run"] = run_index;
            meta["prompt_experiment"] = true;
            meta["appended_instruction"] = kPromptExperimentInstruction;
            meta["prompt"] = sent;
        }
        if (g_separation_experiment) meta["separation"] = "evidence-and-claim-fences";
        if (g_revised_benchmark) meta["benchmark"] = "cm1-revised-1";
        if (g_placement_experiment) meta["instruction_placement"] = "before-evidence";
        if (g_example_ablation) meta["example_objects"] = "removed";
        if (g_core_benchmark) meta["benchmark"] = "cm1-core-1";
        std::ofstream record(stem + ".json");
        record << meta.dump(2) << "\n";
        std::cout << "diagnostic=" << stem << ".txt\n";
    }
    return completion;
}

Observed judgeFixture(LLMInterface& llm, const Fixture& fixture, int run_index) {
    Observed observed;
    observed.id = fixture.id;
    observed.kind = fixture.kind;
    observed.expected_label = fixture.expected_label;
    observed.gold_quote = fixture.gold_quote;
    observed.evidence = fixture.evidence;

    Thoth::Cm1EvidenceChunk chunk;
    chunk.content = fixture.evidence;
    chunk.content_sha256 = Thoth::sha256Hex(chunk.content);
    const auto answer = Thoth::buildCm1AnswerVersion(
        "cm1-live-evidence", fixture.id, "run-" + std::to_string(run_index), fixture.claim, 0);

    std::int64_t elapsed_ms = 0;
    int used_timeout = 0;
    std::string generation_id;
    std::string finish_reason;
    const auto judgment = Thoth::runCm1EvidenceJudgment(
        answer, fixture.claim, {chunk}, true, true, false,
        [&](const std::string& prompt, int timeout_seconds) {
            return callProductionModel(
                llm, prompt, timeout_seconds, fixture.id, run_index,
                elapsed_ms, used_timeout, generation_id, finish_reason);
        },
        0, static_cast<int>(Thoth::LlmTimeoutPolicy::timeoutSeconds()));

    observed.elapsed_ms = elapsed_ms;
    observed.timeout_seconds = used_timeout;
    observed.generation_id = generation_id;
    observed.finish_reason = finish_reason;
    if (!judgment) {
        observed.status = "unavailable";
        observed.reason = "not_recorded";
        return observed;
    }
    const auto payload = nlohmann::json::parse(judgment->payload_json);
    const auto& claims = payload["claims"];
    if (!claims.is_array() || claims.empty()) {
        observed.status = "unavailable";
        observed.reason = "not_judged";
        return observed;
    }
    const auto& claim = claims[0];
    observed.status = claim.value("evaluation_status", "");
    if (observed.status == "available") {
        observed.label = claim.value("label", "");
        observed.quote = claim.value("evidence_quote", "");
    } else {
        observed.reason = claim.value("unavailable_reason", "");
    }
    return observed;
}

std::string labelKey(const Observed& item) {
    if (item.status == "available") return item.label;
    return "unavailable";
}

bool quoteInOnlyIncludedChunk(const std::string& quote, const std::string& evidence) {
    return !quote.empty() && evidence.find(quote) != std::string::npos;
}

RunScore scoreRun(const std::vector<Observed>& items) {
    RunScore score;
    for (const auto& item : items) {
        if (item.status != "available") ++score.unavailable;
        const bool label_match = item.status == "available" && item.label == item.expected_label;
        if (label_match) ++score.label_hits;
        if (item.kind == "paraphrase" && label_match) ++score.paraphrase_hits;
        if ((item.kind == "negation" || item.kind == "number") && label_match) {
            ++score.contradicted_hits;
        }
        if (item.status == "available" && item.quote == item.gold_quote) ++score.quote_hits;
        if (item.status == "available"
            && (item.label == "supported" || item.label == "contradicted")
            && !quoteInOnlyIncludedChunk(item.quote, item.evidence)) {
            score.quote_provenance = false;
        }
        if (item.kind == "absent") {
            const bool insufficient = item.status == "available"
                && item.label == "insufficient_evidence" && item.quote.empty();
            if (!insufficient || item.label == "supported") score.absent_class = false;
        }
    }
    score.pass = score.quote_provenance && score.absent_class && score.paraphrase_hits >= 3
        && score.contradicted_hits >= 7 && score.label_hits >= 14 && score.quote_hits >= 12
        && score.unavailable <= 2;
    return score;
}

int repeatedLabels(const std::vector<Observed>& first, const std::vector<Observed>& second) {
    int matched = 0;
    const std::size_t count = std::min(first.size(), second.size());
    for (std::size_t i = 0; i < count; ++i) {
        if (labelKey(first[i]) == labelKey(second[i])) ++matched;
    }
    return matched;
}

void printItem(int run, const Observed& item) {
    std::cout << "run=" << run
              << " id=" << item.id
              << " kind=" << item.kind
              << " expected=" << item.expected_label
              << " actual=" << (item.label.empty() ? std::string("-") : item.label)
              << " status=" << item.status
              << " reason=" << (item.reason.empty() ? std::string("-") : item.reason)
              << " elapsed_ms=" << item.elapsed_ms
              << " timeout_s=" << item.timeout_seconds
              << "\n  gold=" << (item.gold_quote.empty() ? std::string("<empty>") : item.gold_quote)
              << "\n  quote=" << (item.quote.empty() ? std::string("<empty>") : item.quote)
              << "\n";
    if (g_prompt_experiment) {
        std::cout << "  finish=" << (item.finish_reason.empty() ? std::string("-") : item.finish_reason)
                  << "\n";
    }
}

bool itemAccepted(const Observed& item) {
    if (item.status != "available" || item.label != item.expected_label || item.quote != item.gold_quote) {
        return false;
    }
    if (item.label == "insufficient_evidence") return item.quote.empty();
    return quoteInOnlyIncludedChunk(item.quote, item.evidence);
}

void printScore(int run, const RunScore& score) {
    std::cout << "run=" << run
              << " quote_provenance=" << (score.quote_provenance ? "pass" : "fail")
              << " absent_class=" << (score.absent_class ? "pass" : "fail")
              << " paraphrase=" << score.paraphrase_hits << "/4"
              << " contradicted=" << score.contradicted_hits << "/8"
              << " labels=" << score.label_hits << "/16"
              << " quotes=" << score.quote_hits << "/16"
              << " unavailable=" << score.unavailable << "/16"
              << " run_pass=" << (score.pass ? "pass" : "fail")
              << "\n";
}

} // namespace

const Fixture* findFixture(const std::string& id) {
    for (const auto& fixture : fixtures()) {
        if (fixture.id == id) return &fixture;
    }
    return nullptr;
}

int main(int argc, char** argv) {
    const char* only_id = nullptr;
    if (argc == 2 && std::string(argv[1]) == "--prompt-experiment") {
        g_prompt_experiment = true;
    } else if (argc == 2 && std::string(argv[1]) == "--separation-experiment") {
        g_prompt_experiment = true;
        g_separation_experiment = true;
    } else if (argc == 2 && std::string(argv[1]) == "--revised-benchmark") {
        g_revised_benchmark = true;
        g_prompt_experiment = true;
        g_separation_experiment = true;
    } else if (argc == 2 && std::string(argv[1]) == "--placement-experiment") {
        g_placement_experiment = true;
        g_prompt_experiment = true;
        g_separation_experiment = true;
    } else if (argc == 2 && std::string(argv[1]) == "--example-ablation") {
        g_example_ablation = true;
        g_prompt_experiment = true;
        g_separation_experiment = true;
    } else if (argc == 2 && std::string(argv[1]) == "--core-benchmark") {
        g_core_benchmark = true;
        g_prompt_experiment = true;
        g_separation_experiment = true;
    } else if (argc == 2) {
        only_id = argv[1];
    } else if (argc > 2) {
        std::cerr << "usage: thoth-cm1-live-evidence [fixture-id|--prompt-experiment|--separation-experiment|--revised-benchmark|--placement-experiment|--example-ablation|--core-benchmark]\n";
        return 2;
    }

    std::string why;
    if (!segmentationReady(why)) {
        std::cerr << "cm1-live-evidence STOP: " << why << "\n";
        return 2;
    }
    std::cout << "segmentation=pass fixtures=" << fixtures().size() << "\n";
    if (g_revised_benchmark && !revisedSetReady(why)) {
        std::cerr << "cm1-live-evidence STOP: " << why << "\n";
        return 2;
    }
    if (g_core_benchmark && !coreSetReady(why)) {
        std::cerr << "cm1-live-evidence STOP: " << why << "\n";
        return 2;
    }
    if (!timeoutIsApolloDefault(why) || !inferencePathIsLive(why)) {
        std::cerr << "cm1-live-evidence STOP: " << why << "\n";
        return 2;
    }

    std::string trace_directory;
    if (!isolateTraceDirectory(trace_directory)) {
        std::cerr << "cm1-live-evidence STOP: could not isolate the trace directory\n";
        return 2;
    }

    // Same resolution as the host process when agent_workspace/config.json is absent:
    // default Config, then env overrides. Endpoint and backend come from the
    // existing inference env, which is unset on Anubis and selects llama-server.
    Config config;
    std::string backend_error;
    const auto backend = Thoth::tryResolveInferenceBackend(backend_error);
    if (!backend) {
        std::cerr << "cm1-live-evidence STOP: " << backend_error << "\n";
        return 2;
    }
    const auto endpoints = Thoth::resolveInferenceEndpoints(config);
    std::cout << "trace_directory=" << trace_directory
              << " timeout_policy_s=" << Thoth::LlmTimeoutPolicy::timeoutSeconds()
              << " temperature=0 num_predict=" << Thoth::kCm1EvidenceNumPredict
              << " backend=" << Thoth::inferenceBackendName(*backend)
              << " endpoint=" << endpoints.base_url
              << " configured_model=" << config.llm_model
              << "\n";

    LLMInterface llm(LLMBackend::Ollama, &config);
    if (g_core_benchmark) {
        std::cout << "benchmark=cm1-core-1"
                  << " prompt=examples+closed-blocks+stop-after-claim-end"
                  << "\n";
        std::vector<Observed> first;
        std::vector<Observed> second;
        first.reserve(coreFixtures().size());
        second.reserve(coreFixtures().size());
        for (const auto& fixture : coreFixtures()) {
            const Observed item = judgeFixture(llm, fixture, 1);
            first.push_back(item);
            printItem(1, item);
        }
        for (const auto& fixture : coreFixtures()) {
            const Observed item = judgeFixture(llm, fixture, 2);
            second.push_back(item);
            printItem(2, item);
        }
        const RunScore first_score = scoreRun(first);
        const RunScore second_score = scoreRun(second);
        const int repeated = repeatedLabels(first, second);
        const bool consistency = repeated >= 15;
        printScore(1, first_score);
        printScore(2, second_score);
        std::cout << "consistency=" << repeated << "/16"
                  << " consistency_pass=" << (consistency ? "pass" : "fail")
                  << " model=" << llm.getSelectedModel()
                  << "\n";
        const bool suite = first_score.pass && second_score.pass && consistency;
        std::cout << "suite=" << (suite ? "pass" : "fail") << "\n";
        return suite ? 0 : 1;
    }
    if (g_example_ablation) {
        static const char* kIds[] = {"paraphrase-3", "paraphrase-4"};
        std::cout << "example_ablation=removed-three-json-examples"
                  << " instruction_placement=after-claim-end"
                  << "\n";
        std::vector<Observed> first;
        std::vector<Observed> second;
        int accepted = 0;
        for (int run = 1; run <= 2; ++run) {
            auto& dest = run == 1 ? first : second;
            for (const char* id : kIds) {
                const Fixture* fixture = findFixture(id);
                if (fixture == nullptr) {
                    std::cerr << "cm1-live-evidence STOP: unknown fixture " << id << "\n";
                    return 2;
                }
                const Observed item = judgeFixture(llm, *fixture, run);
                dest.push_back(item);
                printItem(run, item);
                const bool pass = itemAccepted(item);
                if (pass) ++accepted;
                std::cout << "item_pass=" << (pass ? "pass" : "fail") << "\n";
            }
        }
        const int repeated = repeatedLabels(first, second);
        std::cout << "experiment_accepted=" << accepted << "/4"
                  << " consistency=" << repeated << "/2"
                  << " model=" << llm.getSelectedModel()
                  << "\n";
        return accepted == 4 ? 0 : 1;
    }
    if (g_placement_experiment) {
        static const char* kIds[] = {"paraphrase-3", "paraphrase-4"};
        std::cout << "placement_experiment=before-evidence"
                  << " instruction=" << kPromptExperimentInstruction;
        std::vector<Observed> first;
        std::vector<Observed> second;
        int accepted = 0;
        for (int run = 1; run <= 2; ++run) {
            auto& dest = run == 1 ? first : second;
            for (const char* id : kIds) {
                const Fixture* fixture = findFixture(id);
                if (fixture == nullptr) {
                    std::cerr << "cm1-live-evidence STOP: unknown fixture " << id << "\n";
                    return 2;
                }
                const Observed item = judgeFixture(llm, *fixture, run);
                dest.push_back(item);
                printItem(run, item);
                const bool pass = itemAccepted(item);
                if (pass) ++accepted;
                std::cout << "item_pass=" << (pass ? "pass" : "fail") << "\n";
            }
        }
        const int repeated = repeatedLabels(first, second);
        std::cout << "experiment_accepted=" << accepted << "/4"
                  << " consistency=" << repeated << "/2"
                  << " model=" << llm.getSelectedModel()
                  << "\n";
        return accepted == 4 ? 0 : 1;
    }
    if (g_revised_benchmark) {
        std::cout << "revised_benchmark=cm1-revised-1"
                  << " paraphrase-1_claim=w_t is less than zero."
                  << " original_paraphrase-1=Trajectory weight is below zero."
                  << " prompt=closed-blocks+one-json-instruction"
                  << "\n";
        std::vector<Observed> first;
        std::vector<Observed> second;
        first.reserve(revisedFixtures().size());
        second.reserve(revisedFixtures().size());
        for (const auto& fixture : revisedFixtures()) {
            const Observed item = judgeFixture(llm, fixture, 1);
            first.push_back(item);
            printItem(1, item);
        }
        for (const auto& fixture : revisedFixtures()) {
            const Observed item = judgeFixture(llm, fixture, 2);
            second.push_back(item);
            printItem(2, item);
        }
        const RunScore first_score = scoreRun(first);
        const RunScore second_score = scoreRun(second);
        const int repeated = repeatedLabels(first, second);
        const bool consistency = repeated >= 15;
        printScore(1, first_score);
        printScore(2, second_score);
        std::cout << "consistency=" << repeated << "/16"
                  << " consistency_pass=" << (consistency ? "pass" : "fail")
                  << " model=" << llm.getSelectedModel()
                  << "\n";
        const bool suite = first_score.pass && second_score.pass && consistency;
        std::cout << "suite=" << (suite ? "pass" : "fail") << "\n";
        return suite ? 0 : 1;
    }
    if (g_separation_experiment) {
        static const char* kIds[] = {"negation-4", "absent-2"};
        std::cout << "separation_experiment=on fences=EVIDENCE PASSAGE START/END, CLAIM START/END\n"
                  << "baseline_instruction=" << kPromptExperimentInstruction;
        std::vector<Observed> first;
        std::vector<Observed> second;
        int accepted = 0;
        for (int run = 1; run <= 2; ++run) {
            auto& dest = run == 1 ? first : second;
            for (const char* id : kIds) {
                const Fixture* fixture = findFixture(id);
                if (fixture == nullptr) {
                    std::cerr << "cm1-live-evidence STOP: unknown fixture " << id << "\n";
                    return 2;
                }
                const Observed item = judgeFixture(llm, *fixture, run);
                dest.push_back(item);
                printItem(run, item);
                const bool pass = itemAccepted(item);
                if (pass) ++accepted;
                std::cout << "item_pass=" << (pass ? "pass" : "fail") << "\n";
            }
        }
        const int repeated = repeatedLabels(first, second);
        std::cout << "experiment_accepted=" << accepted << "/4"
                  << " consistency=" << repeated << "/2"
                  << " model=" << llm.getSelectedModel()
                  << "\n";
        return accepted == 4 ? 0 : 1;
    }
    if (g_prompt_experiment) {
        static const char* kIds[] = {"paraphrase-4", "negation-2", "negation-4", "absent-2"};
        std::cout << "prompt_experiment=on appended_instruction=" << kPromptExperimentInstruction;
        std::vector<Observed> first;
        std::vector<Observed> second;
        int accepted = 0;
        for (int run = 1; run <= 2; ++run) {
            auto& dest = run == 1 ? first : second;
            for (const char* id : kIds) {
                const Fixture* fixture = findFixture(id);
                if (fixture == nullptr) {
                    std::cerr << "cm1-live-evidence STOP: unknown fixture " << id << "\n";
                    return 2;
                }
                const Observed item = judgeFixture(llm, *fixture, run);
                dest.push_back(item);
                printItem(run, item);
                const bool pass = itemAccepted(item);
                if (pass) ++accepted;
                std::cout << "item_pass=" << (pass ? "pass" : "fail") << "\n";
            }
        }
        const int repeated = repeatedLabels(first, second);
        std::cout << "experiment_accepted=" << accepted << "/8"
                  << " consistency=" << repeated << "/4"
                  << " model=" << llm.getSelectedModel()
                  << "\n";
        return accepted == 8 ? 0 : 1;
    }
    if (only_id != nullptr) {
        const Fixture* fixture = findFixture(only_id);
        if (fixture == nullptr) {
            std::cerr << "cm1-live-evidence STOP: unknown fixture " << only_id << "\n";
            return 2;
        }
        const Observed item = judgeFixture(llm, *fixture, 1);
        printItem(1, item);
        std::cout << "model=" << llm.getSelectedModel() << "\n";
        return item.status.empty() ? 1 : 0;
    }

    std::vector<Observed> first;
    std::vector<Observed> second;
    first.reserve(fixtures().size());
    second.reserve(fixtures().size());
    for (const auto& fixture : fixtures()) {
        const Observed item = judgeFixture(llm, fixture, 1);
        first.push_back(item);
        printItem(1, item);
    }
    for (const auto& fixture : fixtures()) {
        const Observed item = judgeFixture(llm, fixture, 2);
        second.push_back(item);
        printItem(2, item);
    }

    const RunScore first_score = scoreRun(first);
    const RunScore second_score = scoreRun(second);
    const int repeated = repeatedLabels(first, second);
    const bool consistency = repeated >= 15;
    printScore(1, first_score);
    printScore(2, second_score);
    std::cout << "consistency=" << repeated << "/16"
              << " consistency_pass=" << (consistency ? "pass" : "fail")
              << " model=" << llm.getSelectedModel()
              << "\n";
    const bool suite = first_score.pass && second_score.pass && consistency;
    std::cout << "suite=" << (suite ? "pass" : "fail") << "\n";
    return suite ? 0 : 1;
}
