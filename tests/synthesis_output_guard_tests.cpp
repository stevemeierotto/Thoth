/*
 * Copyright (c) 2025 Steve Meierotto
 *
 * Thoth — synthesis repetition-guard regression tests
 *
 * Licensed under the MIT License (see LICENSE in project root)
 */

#include "synthesis_output_guard.h"

#include "config.h"
#include "controller_event.h"
#include "executive_controller.h"
#include "iplanner.h"
#include "llm_interface.h"
#include "memory.h"
#include "mock_inference_client.h"
#include "plan.h"
#include "plan_reuse_config.h"
#include "step_metrics_repository.h"
#include "tools.h"
#include "workflow_engine.h"

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace {

std::string makeInstructionLoop(int copies) {
    std::ostringstream out;
    out << " When the goal requests a summary, write that summary and stop. "
           "When the goal requests a step-by-step guide, write that guide and stop.\n";
    for (int i = 0; i < copies; ++i) {
        out << "When the goal requests a set of steps to investigate item " << i
            << ", write those steps and stop.\n";
        out << "When the goal requests a set of steps to troubleshoot item " << i
            << ", write those steps and stop.\n";
    }
    return out.str();
}

std::string makeDiagnosticAnswer() {
    return R"( Do not continue with additional text.

[Answer]
The issue with GRUB not finding the boot logical volume (LV) could be due to changes made to an unrelated LV. Here are some plausible causes and steps to diagnose and recover from the situation:

**Possible Causes:**
1. **Metadata Changes**: If an unrelated LV had its metadata changed, it might have inadvertently altered the metadata of the boot LV, leading to GRUB not recognizing it.
2. **Disk Space Issues**: Changes to another LV might have caused disk space issues, which could affect the boot process if the boot LV is on a different partition or volume.
3. **Logical Volume Manager (LVM) Configuration**: Changes to LVM configurations, such as volume group (VG) changes or LV resizing, might have impacted the boot LV.

**Safe Non-Destructive Checks:**
1. **Check LVM Configuration**: Use `lvdisplay` to verify the current configuration of the boot LV and other relevant LVM volumes.
2. **Check Disk Usage**: Use `df -h` to check disk usage and ensure there is enough space for the boot LV.
3. **Verify GRUB Configuration**: Use `grub-probe` to check if GRUB can detect the boot LV. Run `grub-probe /boot` and observe if it reports any errors.
4. **Check Boot Loader Files**: Verify the integrity of GRUB files using `grub-mkconfig -o /boot/grub/grub.cfg` and check for any missing or corrupt files.

**Recovery/Prevention Steps:**
1. **Restore LVM Configuration**: If the LVM configuration is altered, restore it to the known good state.
2. **Resize or Resize Back**: If changes to another LV caused disk space issues, resize the boot LV or the volume group to accommodate the changes.
3. **Reinstall GRUB**: If GRUB files are corrupt, reinstall GRUB by running `grub-install /dev/sdX` (replace `/dev/sdX` with the appropriate device) and then `update-grub`.

**Evidence vs. Assumptions:**
- **Evidence**: The system is accessible via PXE recovery, and GRUB diagnostic utilities are showing trouble locating the expected LVM device.
- **Assumptions**: Changes to an unrelated LV might have affected the boot LV, but further investigation is needed to confirm this.

By following these steps, you can diagnose the issue and take appropriate actions to recover the system or prevent similar
)";
}

std::string readFileIfPresent(const std::string& path) {
    std::ifstream in(path);
    if (!in) {
        return {};
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

struct EnvSnapshot {
    const char* mock_llm = nullptr;
    const char* test_suite = nullptr;
    const char* workspace = nullptr;
    const char* logs = nullptr;
    std::string root;

    EnvSnapshot() {
        mock_llm = std::getenv("THOTH_MOCK_LLM");
        test_suite = std::getenv("THOTH_TEST_SUITE_DEV");
        workspace = std::getenv("THOTH_WORKSPACE_PATH");
        logs = std::getenv("THOTH_LOGS_PATH");
        root = "/tmp/thoth-synth-guard-" +
               std::to_string(
                   std::chrono::steady_clock::now().time_since_epoch().count());
        std::filesystem::create_directories(root + "/workspace");
        std::filesystem::create_directories(root + "/logs");
        setenv("THOTH_WORKSPACE_PATH", (root + "/workspace").c_str(), 1);
        setenv("THOTH_LOGS_PATH", (root + "/logs").c_str(), 1);
        // Real generateCall path — these env flags would bypass the guard.
        unsetenv("THOTH_MOCK_LLM");
        unsetenv("THOTH_TEST_SUITE_DEV");
    }

    ~EnvSnapshot() {
        restore("THOTH_MOCK_LLM", mock_llm);
        restore("THOTH_TEST_SUITE_DEV", test_suite);
        restore("THOTH_WORKSPACE_PATH", workspace);
        restore("THOTH_LOGS_PATH", logs);
        std::error_code ec;
        std::filesystem::remove_all(root, ec);
    }

    static void restore(const char* key, const char* value) {
        if (value) {
            setenv(key, value, 1);
        } else {
            unsetenv(key);
        }
    }
};

PlanStep makeLlmStep() {
    PlanStep step;
    step.step_id = "synthesize-findings";
    step.description = "Summarize findings";
    step.type = StepType::LLM;
    step.status = StepStatus::PENDING;
    step.failure_policy.max_retries = 0;
    return step;
}

std::unique_ptr<Thoth::MockInferenceClient> makeClient(std::string response) {
    auto client = std::make_unique<Thoth::MockInferenceClient>();
    client->on_generate = [response = std::move(response)](const Thoth::InferenceGenerateRequest&) {
        Thoth::InferenceGenerateResult out;
        out.ok = true;
        out.text = response;
        out.finish_reason = "length";
        out.provider_usage_reported = true;
        out.token_usage.prompt_tokens = 100;
        out.token_usage.completion_tokens = 512;
        out.token_usage.total_tokens = 612;
        return out;
    };
    client->on_embed = [](const Thoth::InferenceEmbedRequest& req) {
        Thoth::InferenceEmbedResult out;
        out.ok = true;
        out.embeddings.assign(req.inputs.size(), std::vector<float>(8, 0.1f));
        return out;
    };
    return client;
}

std::unique_ptr<Thoth::MockInferenceClient> makeSequencedClient(std::vector<std::string> responses) {
    auto client = std::make_unique<Thoth::MockInferenceClient>();
    auto shared = std::make_shared<std::vector<std::string>>(std::move(responses));
    auto index = std::make_shared<std::atomic<size_t>>(0);
    client->on_generate = [shared, index](const Thoth::InferenceGenerateRequest&) {
        const size_t i = index->fetch_add(1, std::memory_order_relaxed);
        const std::string& text =
            i < shared->size() ? (*shared)[i] : shared->empty() ? std::string{} : shared->back();
        Thoth::InferenceGenerateResult out;
        out.ok = true;
        out.text = text;
        out.finish_reason = "length";
        out.provider_usage_reported = true;
        out.token_usage.prompt_tokens = 100;
        out.token_usage.completion_tokens = 512;
        out.token_usage.total_tokens = 612;
        return out;
    };
    client->on_embed = [](const Thoth::InferenceEmbedRequest& req) {
        Thoth::InferenceEmbedResult out;
        out.ok = true;
        out.embeddings.assign(req.inputs.size(), std::vector<float>(8, 0.1f));
        return out;
    };
    return client;
}

bool waitFor(const std::function<bool()>& ready) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(8);
    while (std::chrono::steady_clock::now() < deadline) {
        if (ready()) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    return false;
}

class ScriptPlanner : public IPlanner {
public:
    Plan plan;
    Plan recovery_plan;
    bool use_recovery = false;
    std::atomic<int> calls{0};
    Plan create_plan(const std::string&) override {
        const int n = calls.fetch_add(1, std::memory_order_relaxed);
        if (use_recovery && n >= 1) {
            return recovery_plan;
        }
        return plan;
    }
    Plan revise_plan(const Plan& existing, const nlohmann::json&) override { return existing; }
};

bool testRejectsInstructionLoop() {
    const std::string loop = makeInstructionLoop(20);
    if (!Thoth::synthesisOutputLooksUnusableRepetition(loop)) {
        std::cerr << "testRejectsInstructionLoop: expected rejection\n";
        return false;
    }
    return true;
}

bool testAcceptsDiagnosticAnswer() {
    const std::string answer = makeDiagnosticAnswer();
    if (Thoth::synthesisOutputLooksUnusableRepetition(answer)) {
        std::cerr << "testAcceptsDiagnosticAnswer: false positive on diagnostic text\n";
        return false;
    }
    return true;
}

bool testAcceptsShortAndEmpty() {
    if (Thoth::synthesisOutputLooksUnusableRepetition("")) {
        std::cerr << "testAcceptsShortAndEmpty: empty should not match guard\n";
        return false;
    }
    if (Thoth::synthesisOutputLooksUnusableRepetition("Short answer about metadata.")) {
        std::cerr << "testAcceptsShortAndEmpty: short text should not match guard\n";
        return false;
    }
    return true;
}

bool testAcceptsVariedLongList() {
    std::ostringstream out;
    out << "Diagnostic plan with distinct checks:\n";
    for (int i = 0; i < 30; ++i) {
        out << (i + 1) << ". Inspect signal " << i
            << " using toolset-" << i
            << " and record observation-" << i
            << " before continuing to the next distinct host path /var/log/app-" << i << ".log.\n";
    }
    out << "Conclude with a mechanism hypothesis that ties these independent observations together.\n";
    if (Thoth::synthesisOutputLooksUnusableRepetition(out.str())) {
        std::cerr << "testAcceptsVariedLongList: false positive on varied long list\n";
        return false;
    }
    return true;
}

bool testRejectsLx10ArmBArtifactIfPresent() {
    const std::string path =
        "/home/steve/egar_lx_r3_pilot_v121/adjudication/r3_v1.2.1_blind_20261117T184232Z/"
        "reviewer_packet/responses/R02.md";
    const std::string packet = readFileIfPresent(path);
    if (packet.empty()) {
        return true;
    }
    const std::string marker = "## Model diagnostic output (verbatim)\n\n";
    const auto start = packet.find(marker);
    if (start == std::string::npos) {
        std::cerr << "testRejectsLx10ArmBArtifactIfPresent: missing model output section\n";
        return false;
    }
    auto body = packet.substr(start + marker.size());
    const auto end = body.find("\n---\n\n## Scores");
    if (end != std::string::npos) {
        body = body.substr(0, end);
    }
    if (!Thoth::synthesisOutputLooksUnusableRepetition(body)) {
        std::cerr << "testRejectsLx10ArmBArtifactIfPresent: expected R02 rejection\n";
        return false;
    }
    return true;
}

bool testExecuteLlmWiresGuard() {
    std::ifstream in("external/basic_agent/src/workflow_engine.cpp");
    if (!in) {
        std::cerr << "testExecuteLlmWiresGuard: cannot open workflow_engine.cpp\n";
        return false;
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    const std::string src = ss.str();
    if (src.find("synthesis_output_guard.h") == std::string::npos ||
        src.find("synthesisOutputLooksUnusableRepetition") == std::string::npos ||
        src.find("unusable repetitive output") == std::string::npos) {
        std::cerr << "testExecuteLlmWiresGuard: missing wiring\n";
        return false;
    }
    return true;
}

bool runExecuteStepCase(const std::string& response, bool expect_success, const char* label) {
    EnvSnapshot env;
    Config cfg;
    cfg.database_path = env.root + "/metrics.db";
    LLMInterface llm(LLMBackend::Ollama, &cfg);
    llm.setInferenceClientForTests(makeClient(response));

    auto tools = std::make_shared<ToolRegistry>();
    auto metrics = std::make_shared<Thoth::StepMetricsRepository>(cfg.database_path);
    Thoth::WorkflowEngine engine(tools, nullptr, nullptr, metrics, &llm);
    engine.setConfig(&cfg);

    PlanStep step = makeLlmStep();
    Thoth::StepExecutionContext ctx;
    ctx.goal = "Explain why unrelated LV changes can hide the boot LV. Identify checks and recovery.";
    const Thoth::StepResult result = engine.executeStep(step, "plan-synth-guard", ctx);

    if (result.success != expect_success) {
        std::cerr << label << ": expected success=" << expect_success
                  << " got success=" << result.success
                  << " error=" << result.error_message << "\n";
        return false;
    }
    if (!expect_success) {
        if (result.error_message.find("unusable repetitive output") == std::string::npos) {
            std::cerr << label << ": missing repetitive-output failure message: "
                      << result.error_message << "\n";
            return false;
        }
    }
    return true;
}

bool testExecuteLlmRejectsRepetitiveResponse() {
    return runExecuteStepCase(makeInstructionLoop(20), false,
                              "testExecuteLlmRejectsRepetitiveResponse");
}

bool testExecuteLlmAcceptsDiagnosticResponse() {
    return runExecuteStepCase(makeDiagnosticAnswer(), true,
                              "testExecuteLlmAcceptsDiagnosticResponse");
}

bool testControllerRejectsRepetitiveAsReusableExperience() {
    EnvSnapshot env;
    Config cfg;
    cfg.database_path = env.root + "/controller.db";

    LLMInterface llm(LLMBackend::Ollama, &cfg);
    // First synthesis loops (fail guard); recovery synthesis is legitimate.
    llm.setInferenceClientForTests(
        makeSequencedClient({makeInstructionLoop(20), makeDiagnosticAnswer()}));

    auto memory = std::make_shared<Memory>(cfg);
    memory->setActiveSessionId("synth-guard-session");
    auto tools = std::make_shared<ToolRegistry>();
    auto planner = std::make_shared<ScriptPlanner>();

    Plan plan;
    plan.plan_id = "synth-guard-plan";
    plan.goal = "Explain why unrelated LV changes can hide the boot LV.";
    plan.plan_origin = "model";
    plan.steps = {makeLlmStep()};
    planner->plan = plan;

    Plan recovery;
    recovery.plan_id = "synth-guard-recovery";
    recovery.goal = plan.goal;
    recovery.plan_origin = "model";
    recovery.steps = {makeLlmStep()};
    planner->recovery_plan = recovery;
    planner->use_recovery = true;

    Thoth::ExecutiveController controller(planner, tools, nullptr, memory);
    controller.set_config(&cfg);
    controller.set_llm_interface(&llm);
    controller.set_session_id("synth-guard-session");
    controller.set_max_reflections(1); // score < 0.6 must enter reflection

    std::atomic<bool> reflection_seen{false};
    std::atomic<float> reflection_score{-1.0f};
    controller.set_event_callback([&](const ControllerEvent& ev) {
        if (ev.type == EventType::REFLECTION_REPLAN) {
            reflection_score.store(ev.metadata.value("trajectory_score", -1.0f),
                                   std::memory_order_relaxed);
            reflection_seen.store(true, std::memory_order_relaxed);
        }
    });

    controller.execute_goal(plan.goal);
    if (!waitFor([&]() {
            return controller.get_state() == Thoth::ControllerState::FAILED ||
                   controller.get_state() == Thoth::ControllerState::COMPLETED;
        })) {
        std::cerr << "testControllerRejectsRepetitiveAsReusableExperience: timed out state="
                  << static_cast<int>(controller.get_state()) << "\n";
        return false;
    }

    if (!waitFor([&]() { return reflection_seen.load(); })) {
        std::cerr << "testControllerRejectsRepetitiveAsReusableExperience: missing REFLECTION_REPLAN\n";
        return false;
    }

    const float score = reflection_score.load();
    if (!(score < Thoth::PlanReuse::kMinSuccessScore) ||
        !(score < Thoth::Reflection::kScoreThreshold)) {
        std::cerr << "testControllerRejectsRepetitiveAsReusableExperience: reflection score "
                  << score << " did not trigger below reuse/reflection thresholds\n";
        return false;
    }

    // Reflection handoff stores history without emitting PLAN_HISTORY_STORED;
    // authoritative reuse gate is the past_plans / trajectory row.
    const auto past = memory->getAllPastPlans();
    bool saw_low = false;
    float stored_fail_score = -1.0f;
    for (const auto& p : past) {
        if (p.plan_id == "synth-guard-plan") {
            stored_fail_score = p.success_score;
            saw_low = p.success_score < Thoth::PlanReuse::kMinSuccessScore;
        }
    }
    if (!saw_low) {
        std::cerr << "testControllerRejectsRepetitiveAsReusableExperience: past_plans missing low score "
                  << "(got " << stored_fail_score << ")\n";
        return false;
    }

    const auto traj = memory->loadTrajectory("traj-synth-guard-plan");
    if (!traj || !(traj->success_score < Thoth::PlanReuse::kMinSuccessScore)) {
        std::cerr << "testControllerRejectsRepetitiveAsReusableExperience: trajectory still reusable\n";
        return false;
    }

    // Mechanical reuse gate: success_score < 0.6 is never returned.
    for (const auto& p : past) {
        if (p.plan_id == "synth-guard-plan" &&
            p.success_score >= Thoth::PlanReuse::kMinSuccessScore) {
            std::cerr << "testControllerRejectsRepetitiveAsReusableExperience: failing plan reusable\n";
            return false;
        }
    }
    const auto injected = memory->retrieveSimilarPlans(std::vector<float>(8, 0.1f), 5);
    for (const auto& p : injected) {
        if (p.plan_id == "synth-guard-plan") {
            std::cerr << "testControllerRejectsRepetitiveAsReusableExperience: low-score plan injected\n";
            return false;
        }
    }
    return true;
}

bool testControllerAcceptsDiagnosticWithoutFalseFail() {
    EnvSnapshot env;
    Config cfg;
    cfg.database_path = env.root + "/controller-ok.db";

    LLMInterface llm(LLMBackend::Ollama, &cfg);
    llm.setInferenceClientForTests(makeClient(makeDiagnosticAnswer()));

    auto memory = std::make_shared<Memory>(cfg);
    auto tools = std::make_shared<ToolRegistry>();
    auto planner = std::make_shared<ScriptPlanner>();
    Plan plan;
    plan.plan_id = "synth-ok-plan";
    plan.goal = "Explain why unrelated LV changes can hide the boot LV.";
    plan.plan_origin = "model";
    plan.steps = {makeLlmStep()};
    planner->plan = plan;

    Thoth::ExecutiveController controller(planner, tools, nullptr, memory);
    controller.set_config(&cfg);
    controller.set_llm_interface(&llm);
    controller.set_session_id("synth-ok-session");
    controller.set_max_reflections(0);

    std::atomic<float> stored_score{-1.0f};
    controller.set_event_callback([&](const ControllerEvent& ev) {
        if (ev.type == EventType::PLAN_HISTORY_STORED) {
            stored_score.store(ev.metadata.value("success_score", -1.0f),
                               std::memory_order_relaxed);
        }
    });

    controller.execute_goal(plan.goal);
    if (!waitFor([&]() {
            return controller.get_state() == Thoth::ControllerState::COMPLETED ||
                   controller.get_state() == Thoth::ControllerState::FAILED;
        })) {
        std::cerr << "testControllerAcceptsDiagnosticWithoutFalseFail: timed out\n";
        return false;
    }
    if (controller.get_state() != Thoth::ControllerState::COMPLETED) {
        std::cerr << "testControllerAcceptsDiagnosticWithoutFalseFail: expected COMPLETED\n";
        return false;
    }
    if (!waitFor([&]() { return stored_score.load() >= 0.0f; })) {
        std::cerr << "testControllerAcceptsDiagnosticWithoutFalseFail: missing history score\n";
        return false;
    }
    if (stored_score.load() < Thoth::PlanReuse::kMinSuccessScore) {
        std::cerr << "testControllerAcceptsDiagnosticWithoutFalseFail: unexpected low score "
                  << stored_score.load() << "\n";
        return false;
    }
    return true;
}

} // namespace

int main() {
    int failures = 0;
    if (!testRejectsInstructionLoop()) failures++;
    if (!testAcceptsDiagnosticAnswer()) failures++;
    if (!testAcceptsShortAndEmpty()) failures++;
    if (!testAcceptsVariedLongList()) failures++;
    if (!testRejectsLx10ArmBArtifactIfPresent()) failures++;
    if (!testExecuteLlmWiresGuard()) failures++;
    if (!testExecuteLlmRejectsRepetitiveResponse()) failures++;
    if (!testExecuteLlmAcceptsDiagnosticResponse()) failures++;
    if (!testControllerRejectsRepetitiveAsReusableExperience()) failures++;
    if (!testControllerAcceptsDiagnosticWithoutFalseFail()) failures++;

    if (failures == 0) {
        std::cout << "All synthesis_output_guard tests passed.\n";
        return 0;
    }
    std::cerr << failures << " synthesis_output_guard test(s) failed.\n";
    return 1;
}
