/*
 * Copyright (c) 2026 Steve Meierotto
 *
 * Thoth — synthesis prompt suffix regression.
 * The suffix follows the current goal's requested format. It does not add a
 * universal one-sentence or one-command limit.
 *
 * Licensed under the MIT License (see LICENSE in project root)
 */

#include "plan.h"
#include "workflow_engine.h"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <unistd.h>

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

std::string synthesisPrompt(const std::string& goal,
                            const std::string& task,
                            const std::string& workspace) {
    ::setenv("THOTH_WORKSPACE_PATH", workspace.c_str(), 1);
    ::setenv("THOTH_MOCK_LLM", "true", 1);
    Thoth::WorkflowEngine engine(nullptr, nullptr, nullptr, nullptr, nullptr);
    PlanStep step;
    step.step_id = "synthesize";
    step.type = StepType::LLM;
    step.description = task;
    Thoth::StepExecutionContext context;
    context.goal = goal;
    const Thoth::StepResult result = engine.executeStep(step, "synthesis-prompt", context);
    if (!result.success || !result.data.contains("data") || !result.data["data"].is_object()) {
        return {};
    }
    return result.data["data"].value("synthesis_prompt", "");
}

std::string afterTask(const std::string& prompt) {
    const auto marker = prompt.rfind("[Task]\n");
    if (marker == std::string::npos) return {};
    return prompt.substr(marker);
}

}  // namespace

int main() {
    char tmpl[] = "/tmp/thoth-synthesis-prompt-XXXXXX";
    char* dir = ::mkdtemp(tmpl);
    if (!dir) {
        std::cerr << "FAIL: could not create a temporary workspace\n";
        return 1;
    }
    const std::string workspace = dir;

    const std::string boundedGoal =
        "Investigate the supplied evidence.\n"
        "State the cause in one sentence and the remedy as one command.\n"
        "Do not invent logs, test results, or successful repairs.";
    const std::string boundedTask =
        "Identify the most likely underlying cause and propose a remedy";
    const std::string bounded = synthesisPrompt(boundedGoal, boundedTask, workspace);
    const std::string boundedSuffix = afterTask(bounded);
    expect(!bounded.empty(), "bounded diagnostic prompt is built");
    expect(bounded.find(boundedGoal) != std::string::npos, "bounded prompt keeps the current goal");
    expect(bounded.find(boundedTask) != std::string::npos, "bounded prompt keeps the planner task");
    expect(boundedSuffix.find("Respond in natural language. Do not emit JSON or tool calls.")
               != std::string::npos,
           "bounded prompt still forbids JSON and tool calls");
    expect(boundedSuffix.find(
               "Follow the answer format and stopping point requested in the current goal.")
               != std::string::npos,
           "bounded prompt tells the model to stop at the goal's requested format");
    expect(boundedSuffix.find("When the goal requests a longer explanation, write that explanation and then stop.")
               != std::string::npos,
           "bounded prompt still allows a longer explanation when the goal requests one");
    expect(boundedSuffix.find("one sentence") == std::string::npos
               && boundedSuffix.find("one command") == std::string::npos,
           "the suffix does not impose a one-sentence or one-command limit");
    expect(bounded.find(boundedTask) < bounded.find(
               "Follow the answer format and stopping point requested in the current goal."),
           "the stopping reminder comes after the planner task");

    const std::string longGoal =
        "Write a full section-by-section analysis of the retrieved evidence, "
        "covering cause, alternatives, and remaining uncertainty.";
    const std::string longTask = "Write the full analysis";
    const std::string longForm = synthesisPrompt(longGoal, longTask, workspace);
    const std::string longSuffix = afterTask(longForm);
    expect(!longForm.empty(), "long-form prompt is built");
    expect(longForm.find(longGoal) != std::string::npos, "long-form prompt keeps the requested analysis");
    expect(longForm.find(longTask) != std::string::npos, "long-form prompt keeps its task");
    expect(longForm.find("one sentence") == std::string::npos
               && longForm.find("one command") == std::string::npos,
           "a long-form request does not receive a one-sentence or one-command limit");
    expect(longSuffix.find("When the goal requests a longer explanation, write that explanation and then stop.")
               != std::string::npos,
           "a long-form request is told to write the requested explanation");
    expect(longSuffix.find(
               "Follow the answer format and stopping point requested in the current goal.")
               != std::string::npos,
           "a long-form request still follows the goal's stopping point");

    ::unsetenv("THOTH_MOCK_LLM");
    std::error_code ec;
    fs::remove_all(workspace, ec);
    std::cout << "synthesis_prompt_tests failures=" << g_failures << "\n";
    return g_failures == 0 ? 0 : 1;
}
