/*
 * Copyright (c) 2026 Steve Meierotto
 *
 * Thoth — trajectory injection formatting regression.
 * Legacy goal/score layout, prior-synthesis excerpt, unboosted gate, and
 * combined past-experience cap (kMaxTrajectoryChars).
 *
 * Licensed under the MIT License (see LICENSE in project root)
 */

#include "config.h"
#include "embedding_engine.h"
#include "executive_controller.h"
#include "goal_text_utils.h"
#include "index_manager.h"
#include "json.hpp"
#include "memory.h"
#include "plan_reuse_config.h"
#include "planner_injection_config.h"
#include "prompt_factory.h"
#include "rag.h"
#include "tools.h"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>

namespace Thoth {
struct ExecutiveControllerP0TestAccess {
    static std::string reuseContext(const ExecutiveController& controller,
                                    const std::vector<Memory::PastPlanRecord>& plans) {
        return controller.build_plan_reuse_context(plans);
    }
};
}

namespace {

namespace fs = std::filesystem;

int failures = 0;

void check(const std::string& name, bool ok) {
    if (!ok) {
        ++failures;
        std::cerr << "FAIL: " << name << "\n";
    }
}

std::string scoreText(float score) {
    std::ostringstream oss;
    oss << score;
    return oss.str();
}

std::string legacyFormat(const std::string& goal, float score) {
    std::ostringstream oss;
    oss << "[PAST EXPERIENCE — RELEVANT TRAJECTORY]\n";
    oss << "- Goal: " << Thoth::cleanGoalForStorage(goal) << "\n";
    oss << "  Success: " << score << "\n";
    return oss.str();
}

const char* kInstruction =
    "Investigate the following Linux problem using the supplied historical case evidence, "
    "your general Linux knowledge, and permitted diagnostic tools.\n"
    "\n"
    "Identify the most likely underlying cause and explain which evidence supports your conclusion.\n"
    "\n"
    "Propose a technically justified remedy. If the available evidence is insufficient to establish "
    "the cause, identify the remaining possibilities, explain what evidence is missing, and propose "
    "the next useful diagnostic step.\n"
    "\n"
    "Distinguish observed facts from hypotheses. Do not invent logs, test results, or successful repairs.\n"
    "\n"
    "Problem:\n";

std::string keptPrefix(const std::string& capped) {
    const std::string marker = "\n...(truncated)...";
    const auto pos = capped.find(marker);
    if (pos == std::string::npos) {
        return capped;
    }
    return capped.substr(0, pos);
}

/** Same concatenation and cap as create_plan plus buildPlanPrompt. */
std::string applyCombinedBudget(const std::string& trajectoryBlock, const std::string& reuseBlock) {
    std::string past = trajectoryBlock;
    if (!reuseBlock.empty()) {
        const std::string cappedReuse =
            Thoth::capInjectionText(reuseBlock, Thoth::PlannerInjection::kMaxPlanReuseChars);
        if (!past.empty()) {
            past += "\n\n";
        }
        past += cappedReuse;
    }
    return Thoth::capInjectionText(past, Thoth::PlannerInjection::kMaxTrajectoryChars);
}

std::string sampleTrajectoryJson(const std::string& response) {
    nlohmann::json root;
    root["steps"] = nlohmann::json::array({
        {{"step_id", "retrieve-context"}, {"type", 1}, {"result", {{"status", "success"}}}},
        {{"step_id", "synthesize"},
         {"type", 2},
         {"result",
          {{"status", "success"},
           {"data", {{"response", response}, {"prompt_chars", 100}}}}}}
    });
    return root.dump();
}

void testValidExtraction() {
    const std::string evidence = "The journal stops before login prompts.";
    const std::string goal = std::string(kInstruction) + evidence;
    const float score = 0.9f;
    const auto section = Thoth::standaloneProblemSection(goal);
    check("valid section is the case text",
          section.has_value() && *section == evidence);

    const std::string formatted = Thoth::formatTrajectoryInjection(goal, score);
    check("valid block keeps the trajectory header",
          formatted.find("[PAST EXPERIENCE — RELEVANT TRAJECTORY]\n") == 0);
    check("valid block keeps the stored score",
          formatted.find("  Success: " + scoreText(score) + "\n") != std::string::npos);
    check("valid block carries the case text",
          formatted.find("- Case: " + evidence + "\n") != std::string::npos);
    check("valid block omits the shared instruction",
          formatted.find("Investigate the following Linux problem") == std::string::npos);
    check("valid block does not use the full-goal line",
          formatted.find("- Goal:") == std::string::npos);

    const std::string crlf = "Shared preamble\r\nProblem:\r\n" + evidence;
    const auto crlfSection = Thoth::standaloneProblemSection(crlf);
    check("CRLF Problem line is a marker",
          crlfSection.has_value() && *crlfSection == evidence);

    const std::string indented = "Shared preamble\n  Problem:\n" + evidence + "\n";
    const auto indentedSection = Thoth::standaloneProblemSection(indented);
    check("indented Problem line is a marker",
          indentedSection.has_value() && *indentedSection == evidence);
}

void testMissingAndEmptyMarker() {
    const float score = 0.8f;
    const std::string plain = "Ship the widget relay";
    check("missing marker keeps the historical layout",
          Thoth::formatTrajectoryInjection(plain, score) == legacyFormat(plain, score));

    const std::string substring = "Note the Problem: is still on this line and needs a log";
    check("Problem inside a longer line is not a marker",
          !Thoth::standaloneProblemSection(substring).has_value());
    check("substring Problem keeps the historical layout",
          Thoth::formatTrajectoryInjection(substring, score) == legacyFormat(substring, score));

    const std::string sameLine = "Problem: the boot stops on this same line";
    check("Problem with text on the same line is not a marker",
          !Thoth::standaloneProblemSection(sameLine).has_value());
    check("same-line Problem keeps the historical layout",
          Thoth::formatTrajectoryInjection(sameLine, score) == legacyFormat(sameLine, score));

    const std::string empty = std::string(kInstruction);
    check("marker with no case text is empty",
          !Thoth::standaloneProblemSection(empty).has_value());
    check("empty Problem section keeps the historical layout",
          Thoth::formatTrajectoryInjection(empty, score) == legacyFormat(empty, score));

    const std::string blank = std::string(kInstruction) + " \n\t\n";
    check("whitespace Problem section is empty",
          !Thoth::standaloneProblemSection(blank).has_value());
    check("whitespace Problem section keeps the historical layout",
          Thoth::formatTrajectoryInjection(blank, score) == legacyFormat(blank, score));

    const std::string buried =
        "plain production goal\n[RELEVANT PAST APPROACHES\nProblem:\nsealed diagnosis\n";
    check("Problem inside a stripped reuse block is ignored",
          Thoth::formatTrajectoryInjection(buried, score) == legacyFormat(buried, score));
    check("stripped reuse text is not injected",
          Thoth::formatTrajectoryInjection(buried, score).find("sealed diagnosis") == std::string::npos);
}

void testScoreRetentionAndLongCase() {
    check("trajectory budget is 1536", Thoth::PlannerInjection::kMaxTrajectoryChars == 1536);
    check("prior synthesis budget is 768", Thoth::PlannerInjection::kMaxPriorSynthesisChars == 768);

    const std::string evidenceStart = "CASE-TOKEN-START journal stops before login prompts";
    std::string evidence = evidenceStart;
    evidence.append(4000, 'x');
    evidence += "CASE-TOKEN-END";
    const std::string goal = std::string(kInstruction) + evidence;
    check("shared instruction is shorter than the injection budget",
          std::string(kInstruction).size() < Thoth::PlannerInjection::kMaxTrajectoryChars);

    const float score = 0.85f;
    const std::string formatted = Thoth::formatTrajectoryInjection(goal, score);
    const auto scorePos = formatted.find("  Success: " + scoreText(score));
    const auto casePos = formatted.find(evidenceStart);
    check("score is formatted before the case text",
          scorePos != std::string::npos && casePos != std::string::npos && scorePos < casePos);

    const std::string capped =
        Thoth::capInjectionText(formatted, Thoth::PlannerInjection::kMaxTrajectoryChars);
    const std::string kept = keptPrefix(capped);
    check("long case uses the existing truncation marker",
          capped.size() == kept.size() + std::string("\n...(truncated)...").size());
    check("kept trajectory prefix matches kMaxTrajectoryChars",
          kept.size() == Thoth::PlannerInjection::kMaxTrajectoryChars);
    check("truncated block retains the score",
          kept.find("  Success: " + scoreText(score)) != std::string::npos);
    check("truncated block retains the start of the case",
          kept.find(evidenceStart) != std::string::npos);
    check("truncated block drops the tail of the case",
          capped.find("CASE-TOKEN-END") == std::string::npos);
    check("truncated block drops the shared instruction",
          capped.find("Investigate the following Linux problem") == std::string::npos);
}

void testCombinedBudget() {
    const std::string evidenceStart = "CASE-TOKEN-START journal stops before login prompts";
    std::string evidence = evidenceStart;
    evidence.append(4000, 'y');
    const std::string goal = std::string(kInstruction) + evidence;
    const float score = 0.85f;
    const std::string formatted = Thoth::formatTrajectoryInjection(goal, score);

    const std::string reuse =
        "[RELEVANT PAST APPROACHES — REUSE-ONLY-TOKEN]\n" + std::string(kInstruction) +
        std::string(2500, 'z') + "CASE-TAIL-BEYOND-BUDGET";
    const std::string capped = applyCombinedBudget(formatted, reuse);
    const std::string kept = keptPrefix(capped);
    check("combined cap keeps kMaxTrajectoryChars characters",
          kept.size() == Thoth::PlannerInjection::kMaxTrajectoryChars);
    check("combined cap retains the trajectory score",
          kept.find(scoreText(score)) != std::string::npos);
    check("combined cap retains the case start",
          kept.find(evidenceStart) != std::string::npos);
    check("combined cap drops the appended reuse block when case dominates",
          capped.find("REUSE-ONLY-TOKEN") == std::string::npos);
    check("combined cap drops the instruction inside reuse",
          capped.find("Investigate the following Linux problem") == std::string::npos);

    const std::string shortGoal = "Ship the widget relay";
    const std::string shortBlock = Thoth::formatTrajectoryInjection(shortGoal, 0.8f);
    const std::string shortCombined = applyCombinedBudget(shortBlock, reuse);
    const std::string shortKept = keptPrefix(shortCombined);
    check("short goal without a marker survives the combined cap",
          shortKept.find("- Goal: " + shortGoal) != std::string::npos
              && shortKept.find(scoreText(0.8f)) != std::string::npos);
    check("short goal combined block keeps outline space for reuse head",
          shortCombined.find("REUSE-ONLY-TOKEN") != std::string::npos);
    check("combined cap drops the reuse tail after a short goal",
          shortCombined.find("CASE-TAIL-BEYOND-BUDGET") == std::string::npos);
}

void testPriorSynthesisExtractionAndLabels() {
    const std::string goal = "Diagnose VPN DNS overwrite";
    const std::string diagnosis =
        "VPN profiles often rewrite resolv.conf; check NetworkManager and systemd-resolved.";
    const std::string json = sampleTrajectoryJson(diagnosis);

    const std::string excerpt =
        Thoth::extractPriorSynthesisExcerpt(json, Thoth::PlannerInjection::kMaxPriorSynthesisChars);
    check("extract returns the stored synthesis response", excerpt == diagnosis);

    const std::string enriched = Thoth::formatTrajectoryInjection(
        goal, 1.0f, json, true, Thoth::PlannerInjection::kMaxPriorSynthesisChars);
    check("enriched block includes prior synthesis label",
          enriched.find("Prior synthesis (unverified experience, excerpt): " + diagnosis)
              != std::string::npos);
    check("enriched block labels mechanical success",
          enriched.find("Mechanical success: 1") != std::string::npos
              && enriched.find("not diagnostic verification") != std::string::npos);
    check("enriched block states verified outcome is not recorded",
          enriched.find("Verified diagnostic outcome: not recorded") != std::string::npos);
    check("enriched block does not use legacy Success line",
          enriched.find("  Success: ") == std::string::npos);

    std::string longResponse(Thoth::PlannerInjection::kMaxPriorSynthesisChars + 200, 'A');
    longResponse.replace(0, 12, "SYNTH-START-");
    longResponse.replace(longResponse.size() - 10, 10, "SYNTH-END");
    const std::string cappedExcerpt = Thoth::extractPriorSynthesisExcerpt(
        sampleTrajectoryJson(longResponse), Thoth::PlannerInjection::kMaxPriorSynthesisChars);
    check("long synthesis is truncated with marker",
          cappedExcerpt.find("SYNTH-START-") != std::string::npos
              && cappedExcerpt.find("\n...(truncated)...") != std::string::npos
              && cappedExcerpt.find("SYNTH-END") == std::string::npos);
    check("truncated synthesis kept prefix matches kMaxPriorSynthesisChars",
          keptPrefix(cappedExcerpt).size() == Thoth::PlannerInjection::kMaxPriorSynthesisChars);
}

void testRelevanceGateAndMalformedFallback() {
    const std::string goal = "Ship the widget relay";
    const float score = 0.8f;
    const std::string json = sampleTrajectoryJson("should not appear when gated off");

    const std::string gatedOff = Thoth::formatTrajectoryInjection(
        goal, score, json, false, Thoth::PlannerInjection::kMaxPriorSynthesisChars);
    check("gate false preserves legacy formatting", gatedOff == legacyFormat(goal, score));
    check("gate false omits prior synthesis",
          gatedOff.find("Prior synthesis") == std::string::npos);

    check("unboosted floor constant remains 0.55",
          Thoth::PlannerTrajectory::kMinSimilarityFloor == 0.55f);
    const bool includeAbove =
        0.60f >= Thoth::PlannerTrajectory::kMinSimilarityFloor;
    const bool includeBelow =
        0.50f >= Thoth::PlannerTrajectory::kMinSimilarityFloor;
    check("heuristic includes excerpt at or above unboosted floor", includeAbove);
    check("heuristic excludes excerpt below unboosted floor", !includeBelow);

    const std::string missing = Thoth::formatTrajectoryInjection(
        goal, score, "", true, Thoth::PlannerInjection::kMaxPriorSynthesisChars);
    check("missing trajectory json falls back to legacy", missing == legacyFormat(goal, score));

    const std::string malformed = Thoth::formatTrajectoryInjection(
        goal, score, "{not-json", true, Thoth::PlannerInjection::kMaxPriorSynthesisChars);
    check("malformed trajectory json falls back to legacy",
          malformed == legacyFormat(goal, score));

    const std::string noLlm = Thoth::formatTrajectoryInjection(
        goal,
        score,
        R"({"steps":[{"step_id":"retrieve-context","type":1,"result":{"status":"success"}}]})",
        true,
        Thoth::PlannerInjection::kMaxPriorSynthesisChars);
    check("trajectory without LLM response falls back to legacy",
          noLlm == legacyFormat(goal, score));

    check("empty extract on malformed json",
          Thoth::extractPriorSynthesisExcerpt("{", 64).empty());
}

class UnusedPlanner : public IPlanner {
public:
    Plan create_plan(const std::string&) override { return {}; }
    Plan revise_plan(const Plan&, const nlohmann::json&) override { return {}; }
};

/** Workflow outline injection still present; enriched synthesis can reach the planner prompt. */
void testObservedPayloadOrder() {
    const std::string goal =
        "Diagnose this synthetic service failure using only the supplied evidence.\n"
        "\n"
        "Problem:\n"
        "The fictional service test-daemon fails to start because its configuration file is missing.\n"
        "\n"
        "Recommend the next diagnostic action. Do not execute commands.\n";
    const std::string outline = Thoth::sanitizePlanOutline(
        R"({"steps":[{"step_id":"retrieve-context","type":1},{"step_id":"synthesize","type":2}]})");
    check("observed outline is the two-step summary",
          outline == "RETRIEVAL(retrieve-context) → LLM(synthesize)");

    const std::string diagnosis =
        "The absence of its configuration file explains the start failure; restore the unit drop-in.";
    const std::string trajectory = Thoth::formatTrajectoryInjection(
        goal,
        1.0f,
        sampleTrajectoryJson(diagnosis),
        true,
        Thoth::PlannerInjection::kMaxPriorSynthesisChars);
    check("enriched trajectory carries prior synthesis",
          trajectory.find(diagnosis) != std::string::npos);

    const fs::path workspace = fs::temp_directory_path() / "thoth-trajectory-payload-order";
    fs::remove_all(workspace);
    fs::create_directories(workspace);
    const char* previousWorkspace = std::getenv("THOTH_WORKSPACE_PATH");
    const std::string savedWorkspace = previousWorkspace ? previousWorkspace : "";
    const bool hadWorkspace = previousWorkspace != nullptr;
    setenv("THOTH_WORKSPACE_PATH", workspace.string().c_str(), 1);

    {
        Config cfg;
        cfg.database_path = (workspace / "memory.db").string();
        auto memory = std::make_shared<Memory>(cfg);
        auto engine = std::make_unique<EmbeddingEngine>(EmbeddingEngine::Method::TfIdf);
        auto* index = new IndexManager(engine.get());
        auto rag = std::make_shared<RAGPipeline>(std::move(engine), index);
        auto registry = std::make_shared<ToolRegistry>();
        auto planner = std::make_shared<UnusedPlanner>();
        Thoth::ExecutiveController controller(planner, registry, rag, memory);

        Memory::PastPlanRecord record;
        record.plan_id = "plan-1793492085678-1";
        record.goal = goal;
        record.outline =
            R"({"steps":[{"step_id":"retrieve-context","type":1},{"step_id":"synthesize","type":2}]})";
        record.success_score = 1.0f;
        record.plan_origin = "model";
        const std::string built = Thoth::ExecutiveControllerP0TestAccess::reuseContext(
            controller, {record});
        const auto split = Thoth::splitPlanReuseInjection(std::string("evaluation goal\n") + built);
        check("past-plan workflow outline is unchanged",
              split.second.find("  Outline: " + outline) != std::string::npos
                  && split.second.find("  Outline: " + outline) < split.second.find("  Goal:"));

        const std::string past = applyCombinedBudget(trajectory, split.second);
        PromptFactory prompts(*memory, *rag);
        Thoth::PlannerPromptMetrics metrics;
        const std::string prompt = prompts.buildPlanPrompt("evaluation goal", "", past, &metrics);
        check("planner prompt keeps Rules and Schema core sections",
              prompt.find("Rules:\n") == 0 && prompt.find("\n\nSchema:\n") != std::string::npos);
        check("planner prompt keeps the workflow outline",
              prompt.find("  Outline: " + outline) != std::string::npos);
        check("planner prompt receives the prior synthesis excerpt",
              prompt.find(diagnosis) != std::string::npos);
        check("experience budget drop stays separate from the section cap",
              !metrics.experience_dropped);
        check("trajectory section cap remains configured at 1536",
              Thoth::PlannerInjection::kMaxTrajectoryChars == 1536);

        // Assembly budget: core + 1536 experience must fit under kMinPlanPromptBudget.
        const std::size_t coreBytes = metrics.rules_bytes + metrics.schema_bytes + metrics.goal_bytes;
        check("core prompt bytes leave room for 1536 experience under min budget",
              coreBytes + Thoth::PlannerInjection::kMaxTrajectoryChars
                  < Thoth::PlannerInjection::kMinPlanPromptBudget);
        check("assembled prompt does not drop experience under min budget",
              !metrics.experience_dropped
                  && metrics.total_bytes > metrics.rules_bytes + metrics.schema_bytes);
    }

    if (hadWorkspace) {
        setenv("THOTH_WORKSPACE_PATH", savedWorkspace.c_str(), 1);
    } else {
        unsetenv("THOTH_WORKSPACE_PATH");
    }
    fs::remove_all(workspace);
}

} // namespace

int main() {
    testValidExtraction();
    testMissingAndEmptyMarker();
    testScoreRetentionAndLongCase();
    testCombinedBudget();
    testPriorSynthesisExtractionAndLabels();
    testRelevanceGateAndMalformedFallback();
    testObservedPayloadOrder();
    if (failures != 0) {
        std::cerr << failures << " trajectory injection formatting checks failed\n";
        return 1;
    }
    std::cout << "trajectory injection formatting checks passed\n";
    return 0;
}
