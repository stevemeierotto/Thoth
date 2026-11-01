/*
 * Copyright (c) 2026 Steve Meierotto
 *
 * Thoth — EGAR-LX fixed-evidence retrieval scope tests
 *
 * Each CTest invocation runs one case in a fresh process.
 *
 * Licensed under the MIT License (see LICENSE in project root)
 */

#include "../external/basic_agent/include/agent_context_retrieval.h"
#include "../external/basic_agent/include/chunkers/code_chunk.h"
#include "../external/basic_agent/include/config.h"
#include "../external/basic_agent/include/embedding_engine.h"
#include "../external/basic_agent/include/executive_controller.h"
#include "../external/basic_agent/include/fixed_evidence_containment.h"
#include "../external/basic_agent/include/fixed_evidence_goal_run.h"
#include "../external/basic_agent/include/index_manager.h"
#include "../external/basic_agent/include/iplanner.h"
#include "../external/basic_agent/include/memory.h"
#include "../external/basic_agent/include/plan.h"
#include "../external/basic_agent/include/rag.h"
#include "../external/basic_agent/include/tools.h"
#include "../external/basic_agent/include/workflow_engine.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

void fail(const std::string& message) {
    std::cerr << message << "\n";
}

bool writeFile(const fs::path& path, const std::string& text) {
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        return false;
    }
    out << text;
    return static_cast<bool>(out);
}

struct TempTree {
    fs::path root;
    explicit TempTree(const std::string& label) {
        root = fs::temp_directory_path() / "thoth_lx_scope" / label;
        fs::remove_all(root);
        fs::create_directories(root);
    }
    ~TempTree() {
        std::error_code ec;
        fs::remove_all(root, ec);
    }
};

CodeChunk orphanAt(const fs::path& path) {
    CodeChunk chunk;
    chunk.fileName = path.string();
    chunk.corpus_tier = "legacy_orphan";
    chunk.code = "evidence note";
    return chunk;
}

bool tierListed(const Thoth::RetrievalScope& scope, const std::string& tier) {
    return std::find(scope.allowed_tiers.begin(), scope.allowed_tiers.end(), tier) !=
           scope.allowed_tiers.end();
}

bool testAuthorized() {
    TempTree tree("authorized");
    const fs::path evidence = tree.root / "evidence";
    fs::create_directories(evidence);
    const fs::path note = evidence / "note.txt";
    if (!writeFile(note, "authorized-only-token is the evidence sentence for this note.\n")) {
        fail("authorized: could not write note");
        return false;
    }
    const fs::path link = tree.root / "evidence-link";
    std::error_code ec;
    fs::create_directory_symlink(evidence, link, ec);
    if (ec) {
        fail("authorized: could not create directory symlink");
        return false;
    }

    std::string resolved;
    std::string error;
    if (!Thoth::resolveAuthorizedEvidenceDirectory(link.string(), resolved, error)) {
        fail("authorized: symlink directory rejected: " + error);
        return false;
    }
    std::string evidence_resolved;
    if (!Thoth::resolveAuthorizedEvidenceDirectory(evidence.string(), evidence_resolved, error)
        || resolved != evidence_resolved) {
        fail("authorized: symlink directory did not resolve to the evidence directory");
        return false;
    }

    const Thoth::RetrievalScope fixed =
        Thoth::resolveFixedEvidenceRetrievalScope("session-a", nullptr, {resolved});
    const Thoth::RetrievalScope production =
        Thoth::resolveAgentContextRetrievalScope("session-a", nullptr);
    const CodeChunk chunk = orphanAt(note);
    if (!Thoth::chunkPassesRetrievalScope(chunk, fixed)
        || Thoth::chunkPassesRetrievalScope(chunk, production)
        || fixed.scope_type != "fixed_evidence"
        || production.scope_type != "default_agent_context"
        || !tierListed(fixed, "legacy_orphan")
        || !tierListed(fixed, "session_attachment")
        || tierListed(fixed, "benchmark")
        || tierListed(fixed, "system_reference")
        || tierListed(production, "legacy_orphan")
        || !production.authorized_evidence_roots.empty()) {
        fail("authorized: scope admission mismatch");
        return false;
    }
    return true;
}

bool testSibling() {
    TempTree tree("sibling");
    const fs::path evidence = tree.root / "evidence";
    const fs::path extra = tree.root / "evidence-extra";
    fs::create_directories(evidence);
    fs::create_directories(extra);
    const fs::path inside = evidence / "note.txt";
    const fs::path outside = extra / "note.txt";
    if (!writeFile(inside, "inside the authorized directory note.\n")
        || !writeFile(outside, "outside the sibling prefix directory note.\n")) {
        fail("sibling: could not write notes");
        return false;
    }
    std::string resolved;
    std::string error;
    if (!Thoth::resolveAuthorizedEvidenceDirectory(evidence.string(), resolved, error)) {
        fail("sibling: " + error);
        return false;
    }
    const Thoth::RetrievalScope scope =
        Thoth::resolveFixedEvidenceRetrievalScope("session-a", nullptr, {resolved});
    const fs::path dotted = evidence / ".." / "evidence-extra" / "note.txt";
    if (!Thoth::chunkPassesRetrievalScope(orphanAt(inside), scope)
        || Thoth::chunkPassesRetrievalScope(orphanAt(outside), scope)
        || Thoth::chunkPassesRetrievalScope(orphanAt(dotted), scope)) {
        fail("sibling: prefix or parent path was admitted");
        return false;
    }
    return true;
}

bool testSymlink() {
    TempTree tree("symlink");
    const fs::path evidence = tree.root / "evidence";
    const fs::path outside = tree.root / "outside";
    fs::create_directories(evidence);
    fs::create_directories(outside);
    const fs::path real = evidence / "real.txt";
    const fs::path secret = outside / "secret.txt";
    if (!writeFile(real, "real evidence sentence inside the directory.\n")
        || !writeFile(secret, "secret sentence outside the directory.\n")) {
        fail("symlink: could not write files");
        return false;
    }
    const fs::path escape = evidence / "escape.txt";
    const fs::path inside_link = evidence / "inside.txt";
    std::error_code ec;
    fs::create_symlink(secret, escape, ec);
    if (ec) {
        fail("symlink: could not create escaping symlink");
        return false;
    }
    ec.clear();
    fs::create_symlink(real, inside_link, ec);
    if (ec) {
        fail("symlink: could not create inside symlink");
        return false;
    }
    std::string resolved;
    std::string error;
    if (!Thoth::resolveAuthorizedEvidenceDirectory(evidence.string(), resolved, error)) {
        fail("symlink: " + error);
        return false;
    }
    const Thoth::RetrievalScope scope =
        Thoth::resolveFixedEvidenceRetrievalScope("session-a", nullptr, {resolved});
    if (!Thoth::chunkPassesRetrievalScope(orphanAt(real), scope)
        || !Thoth::chunkPassesRetrievalScope(orphanAt(inside_link), scope)
        || Thoth::chunkPassesRetrievalScope(orphanAt(escape), scope)
        || Thoth::chunkPassesRetrievalScope(orphanAt(secret), scope)) {
        fail("symlink: link target was not confined to the evidence directory");
        return false;
    }
    return true;
}

bool testDuplicate() {
    TempTree tree("duplicate");
    const fs::path evidence = tree.root / "evidence";
    const fs::path other = tree.root / "other";
    fs::create_directories(evidence);
    fs::create_directories(other);
    const fs::path inside = evidence / "note.txt";
    const fs::path outside = other / "note.txt";
    if (!writeFile(inside, "authorized copy of the shared basename.\n")
        || !writeFile(outside, "unrelated copy of the shared basename.\n")) {
        fail("duplicate: could not write notes");
        return false;
    }
    if (inside.filename() != outside.filename()) {
        fail("duplicate: basenames differ");
        return false;
    }
    std::string resolved;
    std::string error;
    if (!Thoth::resolveAuthorizedEvidenceDirectory(evidence.string(), resolved, error)) {
        fail("duplicate: " + error);
        return false;
    }
    const Thoth::RetrievalScope scope =
        Thoth::resolveFixedEvidenceRetrievalScope("session-a", nullptr, {resolved});
    if (!Thoth::chunkPassesRetrievalScope(orphanAt(inside), scope)
        || Thoth::chunkPassesRetrievalScope(orphanAt(outside), scope)) {
        fail("duplicate: basename was used instead of the resolved path");
        return false;
    }
    return true;
}

bool testEmpty() {
    TempTree tree("empty");
    const fs::path evidence = tree.root / "evidence";
    fs::create_directories(evidence);
    const fs::path note = evidence / "note.txt";
    if (!writeFile(note, "note that must stay hidden without authorization.\n")) {
        fail("empty: could not write note");
        return false;
    }
    Thoth::RetrievalScope open_tier;
    open_tier.scope_type = "fixed_evidence";
    open_tier.allowed_tiers = {"legacy_orphan"};
    open_tier.selected_documents = {"note.txt"};
    const Thoth::RetrievalScope unresolved =
        Thoth::resolveFixedEvidenceRetrievalScope("session-a", nullptr, {});
    std::string resolved;
    std::string error;
    const bool absent = !Thoth::resolveAuthorizedEvidenceDirectory("", resolved, error)
        && error.find("absent") != std::string::npos
        && resolved.empty();
    error.clear();
    const bool blank = !Thoth::resolveAuthorizedEvidenceDirectory(" \n\t", resolved, error)
        && error.find("absent") != std::string::npos;
    if (Thoth::chunkPassesRetrievalScope(orphanAt(note), open_tier)
        || Thoth::chunkPassesRetrievalScope(orphanAt(note), unresolved)
        || !unresolved.authorized_evidence_roots.empty()
        || !absent
        || !blank) {
        fail("empty: legacy_orphan was admitted without an evidence directory");
        return false;
    }
    return true;
}

bool testProduction() {
    TempTree tree("production");
    const fs::path evidence = tree.root / "evidence";
    const fs::path elsewhere = tree.root / "elsewhere";
    fs::create_directories(evidence);
    fs::create_directories(elsewhere);
    const fs::path inside = evidence / "note.txt";
    const fs::path outside = elsewhere / "owned.txt";
    if (!writeFile(inside, "orphan inside the evidence directory.\n")
        || !writeFile(outside, "attachment owned by the active session.\n")) {
        fail("production: could not write notes");
        return false;
    }
    std::string resolved;
    std::string error;
    if (!Thoth::resolveAuthorizedEvidenceDirectory(evidence.string(), resolved, error)) {
        fail("production: " + error);
        return false;
    }

    const Thoth::RetrievalScope production =
        Thoth::resolveAgentContextRetrievalScope("session-a", nullptr);
    const Thoth::RetrievalScope fixed =
        Thoth::resolveFixedEvidenceRetrievalScope("session-a", nullptr, {resolved});
    CodeChunk orphan = orphanAt(inside);
    CodeChunk benchmark = orphanAt(inside);
    benchmark.corpus_tier = "benchmark";
    CodeChunk reference = orphanAt(inside);
    reference.corpus_tier = "system_reference";
    CodeChunk other_owner = orphanAt(inside);
    other_owner.corpus_tier = "session_attachment";
    other_owner.owner_context_id = "session-b";
    CodeChunk owner = orphanAt(outside);
    owner.corpus_tier = "session_attachment";
    owner.owner_context_id = "session-a";

    const nlohmann::json production_json = production.toJson();
    const bool production_isolated =
        production.scope_type == "default_agent_context"
        && !tierListed(production, "legacy_orphan")
        && production.authorized_evidence_roots.empty()
        && !production_json.contains("authorized_evidence_roots")
        && !Thoth::chunkPassesRetrievalScope(orphan, production)
        && !Thoth::chunkPassesRetrievalScope(benchmark, production)
        && !Thoth::chunkPassesRetrievalScope(reference, production)
        && !Thoth::chunkPassesRetrievalScope(other_owner, production)
        && Thoth::chunkPassesRetrievalScope(owner, production);
    const bool fixed_keeps_exclusions =
        !Thoth::chunkPassesRetrievalScope(benchmark, fixed)
        && !Thoth::chunkPassesRetrievalScope(reference, fixed)
        && !Thoth::chunkPassesRetrievalScope(other_owner, fixed)
        && Thoth::chunkPassesRetrievalScope(orphan, fixed)
        && Thoth::chunkPassesRetrievalScope(owner, fixed)
        && !tierListed(fixed, "benchmark")
        && !tierListed(fixed, "system_reference");
    if (!production_isolated || !fixed_keeps_exclusions) {
        fail("production: session, benchmark, or system_reference rule changed");
        return false;
    }
    return true;
}

bool testContainmentFlag() {
    TempTree tree("containment");
    const fs::path evidence = tree.root / "evidence";
    fs::create_directories(evidence);
    const fs::path note = evidence / "note.txt";
    if (!writeFile(note, "orphan that containment must not admit.\n")) {
        fail("containment: could not write note");
        return false;
    }
    Thoth::FixedEvidenceContainment::enable();
    const Thoth::RetrievalScope production =
        Thoth::resolveAgentContextRetrievalScope("session-a", nullptr);
    if (!Thoth::FixedEvidenceContainment::enabled()
        || production.scope_type != "default_agent_context"
        || tierListed(production, "legacy_orphan")
        || Thoth::chunkPassesRetrievalScope(orphanAt(note), production)) {
        fail("containment: sticky flag widened the production scope");
        return false;
    }
    return true;
}

class IdlePlanner : public IPlanner {
public:
    Plan create_plan(const std::string& goal) override {
        called = true;
        Plan plan;
        plan.goal = goal;
        plan.plan_id = "idle";
        return plan;
    }
    Plan revise_plan(const Plan& existing_plan, const nlohmann::json&) override {
        called = true;
        return existing_plan;
    }
    bool called = false;
};

bool rejected(Thoth::ExecutiveController& controller,
              IdlePlanner& planner,
              const std::string& directory,
              const std::string& needle) {
    const std::string result =
        controller.execute_fixed_evidence_goal("diagnose the case", directory);
    if (result.rfind("FIXED_EVIDENCE_REJECTED:", 0) != 0
        || result.find(needle) == std::string::npos
        || planner.called
        || Thoth::FixedEvidenceContainment::enabled()
        || controller.is_running()) {
        fail("reject: " + directory + " -> " + result);
        return false;
    }
    return true;
}

bool testReject() {
    if (Thoth::FixedEvidenceContainment::enabled()) {
        fail("reject: containment started enabled");
        return false;
    }
    TempTree tree("reject");
    const fs::path note = tree.root / "note.txt";
    if (!writeFile(note, "not a directory\n")) {
        fail("reject: could not write file");
        return false;
    }
    Config config;
    config.database_path = (tree.root / "memory.db").string();
    auto planner = std::make_shared<IdlePlanner>();
    auto memory = std::make_shared<Memory>(config);
    Thoth::ExecutiveController controller(
        planner, std::make_shared<ToolRegistry>(), nullptr, memory);

    if (!rejected(controller, *planner, "", "absent")
        || !rejected(controller, *planner, " \n\t", "absent")
        || !rejected(controller, *planner, (tree.root / "missing").string(), "not found")
        || !rejected(controller, *planner, note.string(), "not a directory")) {
        return false;
    }

    std::string error;
    std::string state;
    const std::string missing = (tree.root / "missing").string();
    if (Thoth::runFixedEvidenceGoal(controller, "diagnose the case", missing, error, state)
        || error.find("not found") == std::string::npos
        || planner->called
        || Thoth::FixedEvidenceContainment::enabled()) {
        fail("reject: runner consumed a bad directory: " + error);
        return false;
    }
    error.clear();
    if (Thoth::runFixedEvidenceGoal(controller, "diagnose the case", missing, error, state)
        || error.find("one fixed-evidence goal") != std::string::npos
        || error.find("not found") == std::string::npos) {
        fail("reject: second invalid directory consumed the slot: " + error);
        return false;
    }
    return true;
}

std::vector<std::string> chunkFiles(const Thoth::StepResult& result) {
    std::vector<std::string> files;
    if (!result.data.is_object() || !result.data.contains("data") || !result.data["data"].is_object()) {
        return files;
    }
    const nlohmann::json chunks = result.data["data"].value("chunks", nlohmann::json::array());
    if (!chunks.is_array()) {
        return files;
    }
    for (const auto& chunk : chunks) {
        files.push_back(chunk.value("file", ""));
    }
    return files;
}

Thoth::StepResult retrieve(Thoth::WorkflowEngine& workflow,
                    const std::string& query,
                    const std::vector<std::string>& roots) {
    PlanStep step;
    step.step_id = "retrieve";
    step.type = StepType::RETRIEVAL;
    step.description = query;
    step.payload = {{"query", query}, {"top_k", 5}};
    Thoth::StepExecutionContext context;
    context.goal = "diagnose the case";
    context.fixed_evidence_roots = roots;
    return workflow.executeStep(step, "fixed-evidence-plan", context);
}

bool testRetrieval() {
    unsetenv("THOTH_ALP_ENABLED");
    unsetenv("THOTH_ALP_TX_INDEX");
    TempTree tree("retrieval");
    const fs::path workspace = tree.root / "workspace";
    const fs::path evidence = workspace / "evidence";
    const fs::path other = workspace / "evidence-extra";
    const fs::path outside = tree.root / "outside";
    fs::create_directories(evidence);
    fs::create_directories(other);
    fs::create_directories(outside);
    fs::create_directories(workspace / "logs");
    const fs::path note = evidence / "note.txt";
    const fs::path sibling = other / "note.txt";
    const fs::path reference = evidence / "GRAG.md";
    const fs::path secret = outside / "secret.txt";
    if (!writeFile(note, "authorized-only-token is the evidence sentence for this note.\n")
        || !writeFile(sibling, "sibling-only-token is the neighboring directory note.\n")
        || !writeFile(reference, "system-reference-token stays excluded from fixed evidence.\n")
        || !writeFile(secret, "outside-secret-token is not evidence.\n")) {
        fail("retrieval: could not write corpus");
        return false;
    }
    const fs::path escape = evidence / "escape.txt";
    std::error_code ec;
    fs::create_symlink(secret, escape, ec);
    if (ec) {
        fail("retrieval: could not create symlink");
        return false;
    }

    const fs::path workspace_abs = fs::absolute(workspace).lexically_normal();
    setenv("THOTH_WORKSPACE_PATH", workspace_abs.string().c_str(), 1);
    setenv("THOTH_LOGS_PATH", (workspace_abs / "logs").string().c_str(), 1);

    std::string resolved;
    std::string error;
    if (!Thoth::resolveAuthorizedEvidenceDirectory(evidence.string(), resolved, error)) {
        fail("retrieval: " + error);
        return false;
    }

    Config config;
    auto engine = std::make_unique<EmbeddingEngine>(EmbeddingEngine::Method::TfIdf, &config);
    auto index = std::make_unique<IndexManager>(engine.get());
    index->indexFile(note.string());
    index->indexFile(sibling.string());
    index->indexFile(reference.string());
    index->indexFile(escape.string());
    if (index->chunkCount() < 4) {
        fail("retrieval: indexed " + std::to_string(index->chunkCount()) + " chunks");
        return false;
    }

    auto rag = std::make_shared<RAGPipeline>(nullptr, index.get(), &config, nullptr);
    Thoth::WorkflowEngine workflow(std::make_shared<ToolRegistry>(), rag, nullptr, nullptr);

    const Thoth::StepResult production = retrieve(workflow, "authorized-only-token", {});
    const std::string production_dump = production.data.dump();
    if (!production.success
        || production.data.value("retrieved_chunk_count", -1) != 0
        || production.data.value("index_populated", false) != true
        || production_dump.find("default_agent_context") == std::string::npos
        || production_dump.find("fixed_evidence") != std::string::npos) {
        fail("retrieval: production scope returned evidence\n" + production_dump);
        return false;
    }

    const Thoth::StepResult admitted = retrieve(workflow, "authorized-only-token", {resolved});
    const auto admitted_files = chunkFiles(admitted);
    const std::string admitted_dump = admitted.data.dump();
    const bool only_note = admitted.success
        && admitted.data.value("retrieved_chunk_count", 0) == 1
        && admitted_files.size() == 1
        && admitted_files[0].find("/evidence/note.txt") != std::string::npos
        && admitted_dump.find("fixed_evidence") != std::string::npos
        && admitted_dump.find(resolved) != std::string::npos;
    if (!only_note) {
        fail("retrieval: authorized query -> " + admitted_dump);
        return false;
    }

    auto stays_inside = [](const Thoth::StepResult& result) {
        const std::string dump = result.data.dump();
        if (!result.success
            || dump.find("evidence-extra") != std::string::npos
            || dump.find("GRAG.md") != std::string::npos
            || dump.find("escape.txt") != std::string::npos
            || dump.find("outside/secret") != std::string::npos) {
            return false;
        }
        for (const auto& file : chunkFiles(result)) {
            if (file.find("/evidence/note.txt") == std::string::npos) {
                return false;
            }
        }
        return true;
    };
    const Thoth::StepResult sibling_hit = retrieve(workflow, "sibling-only-token", {resolved});
    const Thoth::StepResult reference_hit = retrieve(workflow, "system-reference-token", {resolved});
    const Thoth::StepResult secret_hit = retrieve(workflow, "outside-secret-token", {resolved});
    if (!stays_inside(sibling_hit) || !stays_inside(reference_hit) || !stays_inside(secret_hit)) {
        fail("retrieval: sibling, system_reference, or symlink target was admitted\n"
             + sibling_hit.data.dump() + "\n" + reference_hit.data.dump() + "\n"
             + secret_hit.data.dump());
        return false;
    }
    return true;
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: fixed_evidence_retrieval_scope_tests <case>\n";
        return 2;
    }
    const std::string name = argv[1];
    const bool ok =
        (name == "authorized" && testAuthorized())
        || (name == "sibling" && testSibling())
        || (name == "symlink" && testSymlink())
        || (name == "duplicate" && testDuplicate())
        || (name == "empty" && testEmpty())
        || (name == "production" && testProduction())
        || (name == "containment" && testContainmentFlag())
        || (name == "reject" && testReject())
        || (name == "retrieval" && testRetrieval());
    if (!ok && name != "authorized" && name != "sibling" && name != "symlink"
        && name != "duplicate" && name != "empty" && name != "production"
        && name != "containment" && name != "reject" && name != "retrieval") {
        std::cerr << "unknown case: " << name << "\n";
        return 2;
    }
    return ok ? 0 : 1;
}
