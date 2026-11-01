/*
 * Copyright (c) 2026 Steve Meierotto
 *
 * Thoth — one-shot workspace directory index tests
 *
 * Each CTest invocation runs one case in a fresh process.
 *
 * Licensed under the MIT License (see LICENSE in project root)
 */

#include "../external/basic_agent/include/embedding_engine.h"
#include "../external/basic_agent/include/fixed_evidence_containment.h"
#include "../external/basic_agent/include/index_manager.h"
#include "../external/basic_agent/include/workspace_index_run.h"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

namespace {

namespace fs = std::filesystem;

void fail(const std::string& message) {
    std::cerr << message << "\n";
}

bool writeText(const fs::path& path, const std::string& text) {
    fs::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        return false;
    }
    out << text;
    return static_cast<bool>(out);
}

struct Workspace {
    fs::path root;

    explicit Workspace(const std::string& label) {
        root = fs::temp_directory_path() / "thoth_lx_index" / label;
        fs::remove_all(root);
        fs::create_directories(root);
        setenv("THOTH_WORKSPACE_PATH", root.c_str(), 1);
        setenv("THOTH_LOGS_PATH", (root / "logs").c_str(), 1);
        unsetenv("THOTH_ALP_ENABLED");
        unsetenv("THOTH_ALP_TX_INDEX");
        unsetenv("THOTH_LOG_CONFIG");
    }

    ~Workspace() {
        fs::remove_all(root);
    }
};

struct IndexSession {
    std::unique_ptr<EmbeddingEngine> engine;
    std::unique_ptr<IndexManager> index;

    IndexSession() {
        engine = std::make_unique<EmbeddingEngine>(EmbeddingEngine::Method::TfIdf, nullptr);
        index = std::make_unique<IndexManager>(engine.get());
    }

    void loadSaved() {
        index->init("");
    }
};

bool chunksContain(IndexManager& index, const std::string& marker) {
    for (const auto& chunk : index.getChunks()) {
        if (chunk.code.find(marker) != std::string::npos
            || chunk.fileName.find(marker) != std::string::npos) {
            return true;
        }
    }
    return false;
}

bool testPersist() {
    Workspace workspace("persist");
    const fs::path real = workspace.root / "real";
    const std::string marker = "EGAR index evidence alpha marker for persistence.";
    if (!writeText(real / "alpha.txt", marker + "\n")) {
        fail("persist: write failed");
        return false;
    }
    const fs::path alias = workspace.root / "alias";
    std::error_code ec;
    fs::create_directory_symlink(real, alias, ec);
    if (ec) {
        fail("persist: symlink failed: " + ec.message());
        return false;
    }

    IndexSession session;
    std::string error;
    Thoth::WorkspaceIndexResult result;
    const bool ok = Thoth::indexWorkspaceDirectory(*session.index, alias.string(), error, result);
    const std::string line = Thoth::formatWorkspaceIndexStatus(result);
    const fs::path saved = workspace.root / "rag" / "rag_index.bin";
    if (!ok || result.added <= 0 || result.chunks == 0 || result.no_evidence_added
        || line.find("no_evidence_added") != std::string::npos
        || !fs::is_regular_file(saved)
        || Thoth::FixedEvidenceContainment::enabled()) {
        fail("persist: ok=" + std::to_string(ok) + " error=" + error + " line=" + line);
        return false;
    }

    IndexSession reloaded;
    reloaded.loadSaved();
    if (!chunksContain(*reloaded.index, marker) || reloaded.index->chunkCount() != result.chunks) {
        fail("persist: reloaded index lost the evidence");
        return false;
    }
    return true;
}

bool testNoEvidence() {
    Workspace workspace("no_evidence");
    const fs::path evidence = workspace.root / "evidence";
    fs::create_directories(evidence);
    IndexSession session;
    std::string error;
    Thoth::WorkspaceIndexResult empty_result;
    if (!Thoth::indexWorkspaceDirectory(*session.index, evidence.string(), error, empty_result)
        || empty_result.added != 0 || !empty_result.no_evidence_added
        || Thoth::formatWorkspaceIndexStatus(empty_result).find("no_evidence_added") == std::string::npos
        || !fs::is_regular_file(workspace.root / "rag" / "rag_index.bin")) {
        fail("no_evidence: empty directory: " + error + " " + Thoth::formatWorkspaceIndexStatus(empty_result));
        return false;
    }

    if (!writeText(evidence / "blank.txt", " \n\t\n")
        || !writeText(evidence / "notes.dat", "EGAR unsupported extension should not be indexed.\n")) {
        fail("no_evidence: write failed");
        return false;
    }
    Thoth::WorkspaceIndexResult skipped;
    if (!Thoth::indexWorkspaceDirectory(*session.index, evidence.string(), error, skipped)
        || skipped.added != 0 || !skipped.no_evidence_added || skipped.chunks != empty_result.chunks) {
        fail("no_evidence: skipped files changed the index: " + Thoth::formatWorkspaceIndexStatus(skipped));
        return false;
    }
    return true;
}

bool testPreserve() {
    Workspace workspace("preserve");
    const std::string kept = "EGAR kept evidence marker that must remain indexed.";
    const std::string added = "EGAR added evidence marker stored beside the kept one.";
    const fs::path kept_dir = workspace.root / "kept";
    const fs::path added_dir = workspace.root / "added";
    if (!writeText(kept_dir / "kept.txt", kept + "\n")) {
        fail("preserve: write kept");
        return false;
    }

    IndexSession first;
    std::string error;
    Thoth::WorkspaceIndexResult kept_result;
    if (!Thoth::indexWorkspaceDirectory(*first.index, kept_dir.string(), error, kept_result)
        || kept_result.added <= 0) {
        fail("preserve: first index failed: " + error);
        return false;
    }
    first.index.reset();

    if (!writeText(added_dir / "added.txt", added + "\n")) {
        fail("preserve: write added");
        return false;
    }
    IndexSession second;
    second.loadSaved();
    const std::size_t loaded = second.index->chunkCount();
    Thoth::WorkspaceIndexResult added_result;
    if (!Thoth::indexWorkspaceDirectory(*second.index, added_dir.string(), error, added_result)
        || added_result.added <= 0
        || added_result.chunks != loaded + static_cast<std::size_t>(added_result.added)
        || !chunksContain(*second.index, kept)
        || !chunksContain(*second.index, added)) {
        fail("preserve: merge failed: " + error + " " + Thoth::formatWorkspaceIndexStatus(added_result));
        return false;
    }

    IndexSession reloaded;
    reloaded.loadSaved();
    if (!chunksContain(*reloaded.index, kept) || !chunksContain(*reloaded.index, added)
        || reloaded.index->chunkCount() != added_result.chunks) {
        fail("preserve: saved index dropped a marker");
        return false;
    }
    return true;
}

bool rejected(const std::string& path, const std::string& needle) {
    std::string error;
    std::string resolved;
    const bool ok = Thoth::validateWorkspaceIndexDirectory(path, error, resolved);
    if (ok || error.find(needle) == std::string::npos) {
        fail("expected rejection [" + needle + "] for " + path + " got: " + error);
        return false;
    }
    return true;
}

bool testIsolate() {
    Workspace workspace("isolate");
    const fs::path parent = workspace.root.parent_path();
    const fs::path outside = parent / "outside_evidence";
    const fs::path sibling = parent / "isolate_ws_evil";
    fs::remove_all(outside);
    fs::remove_all(sibling);
    const std::string outside_marker = "EGAR outside evidence must not enter the index.";
    const std::string kept = "EGAR isolated kept evidence marker remains stored.";
    const std::string also = "EGAR sibling file must stay unindexed when a link escapes.";
    if (!writeText(outside / "secret.txt", outside_marker + "\n")
        || !writeText(workspace.root / "kept" / "kept.txt", kept + "\n")
        || !writeText(sibling / "evil.txt", outside_marker + "\n")) {
        fail("isolate: write failed");
        fs::remove_all(outside);
        fs::remove_all(sibling);
        return false;
    }

    IndexSession seeded;
    std::string error;
    Thoth::WorkspaceIndexResult seeded_result;
    if (!Thoth::indexWorkspaceDirectory(
            *seeded.index, (workspace.root / "kept").string(), error, seeded_result)
        || seeded_result.added <= 0) {
        fail("isolate: seed failed: " + error);
        fs::remove_all(outside);
        fs::remove_all(sibling);
        return false;
    }
    const std::size_t seeded_chunks = seeded_result.chunks;
    seeded.index.reset();

    const fs::path bad = workspace.root / "bad";
    fs::create_directories(bad);
    if (!writeText(bad / "also.txt", also + "\n")) {
        fail("isolate: also write failed");
        fs::remove_all(outside);
        fs::remove_all(sibling);
        return false;
    }
    std::error_code ec;
    fs::create_symlink(outside / "secret.txt", bad / "secret.txt", ec);
    if (ec) {
        fail("isolate: file symlink failed: " + ec.message());
        fs::remove_all(outside);
        fs::remove_all(sibling);
        return false;
    }
    fs::create_directory_symlink(outside, workspace.root / "escape_dir", ec);
    if (ec) {
        fail("isolate: directory symlink failed: " + ec.message());
        fs::remove_all(outside);
        fs::remove_all(sibling);
        return false;
    }

    const bool boundaries =
        rejected(outside.string(), "outside the workspace")
        && rejected(sibling.string(), "outside the workspace")
        && rejected((workspace.root / "escape_dir").string(), "outside the workspace")
        && rejected((workspace.root / "kept" / "kept.txt").string(), "not a directory");
    if (!boundaries) {
        fs::remove_all(outside);
        fs::remove_all(sibling);
        return false;
    }

    IndexSession live;
    live.loadSaved();
    Thoth::WorkspaceIndexResult blocked;
    const bool indexed = Thoth::indexWorkspaceDirectory(*live.index, bad.string(), error, blocked);
    if (indexed || error.find("outside the workspace") == std::string::npos
        || chunksContain(*live.index, outside_marker)
        || chunksContain(*live.index, also)
        || live.index->chunkCount() != seeded_chunks
        || !chunksContain(*live.index, kept)) {
        fail("isolate: escaping symlink changed the index: " + error);
        fs::remove_all(outside);
        fs::remove_all(sibling);
        return false;
    }

    IndexSession reloaded;
    reloaded.loadSaved();
    const bool preserved = chunksContain(*reloaded.index, kept)
        && !chunksContain(*reloaded.index, outside_marker)
        && !chunksContain(*reloaded.index, also)
        && reloaded.index->chunkCount() == seeded_chunks
        && !Thoth::FixedEvidenceContainment::enabled();
    fs::remove_all(outside);
    fs::remove_all(sibling);
    if (!preserved) {
        fail("isolate: saved index was replaced");
        return false;
    }
    return true;
}

struct ChildResult {
    int exit_code = -1;
    std::string output;
    bool timed_out = false;
};

ChildResult runEngine(const std::string& binary,
                      const std::vector<std::string>& args,
                      const fs::path& workspace) {
    ChildResult result;
    int pipes[2];
    if (pipe(pipes) != 0) {
        result.output = "pipe failed";
        return result;
    }
    const pid_t pid = fork();
    if (pid < 0) {
        close(pipes[0]);
        close(pipes[1]);
        result.output = "fork failed";
        return result;
    }
    if (pid == 0) {
        setenv("THOTH_WORKSPACE_PATH", workspace.c_str(), 1);
        setenv("THOTH_LOGS_PATH", (workspace / "logs").string().c_str(), 1);
        unsetenv("THOTH_LOG_CONFIG");
        unsetenv("THOTH_ALP_ENABLED");
        unsetenv("THOTH_ALP_TX_INDEX");
        dup2(pipes[1], STDOUT_FILENO);
        dup2(pipes[1], STDERR_FILENO);
        close(pipes[0]);
        close(pipes[1]);
        std::vector<char*> argv;
        argv.push_back(const_cast<char*>(binary.c_str()));
        for (const auto& arg : args) {
            argv.push_back(const_cast<char*>(arg.c_str()));
        }
        argv.push_back(nullptr);
        execv(binary.c_str(), argv.data());
        _exit(127);
    }

    close(pipes[1]);
    const int flags = fcntl(pipes[0], F_GETFL, 0);
    fcntl(pipes[0], F_SETFL, flags | O_NONBLOCK);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);
    bool exited = false;
    int status = 0;
    while (std::chrono::steady_clock::now() < deadline) {
        pollfd fd {};
        fd.fd = pipes[0];
        fd.events = POLLIN;
        poll(&fd, 1, 100);
        char buf[1024];
        while (true) {
            const ssize_t n = read(pipes[0], buf, sizeof(buf));
            if (n > 0) {
                result.output.append(buf, static_cast<std::size_t>(n));
            } else {
                break;
            }
        }
        if (waitpid(pid, &status, WNOHANG) == pid) {
            exited = true;
            break;
        }
    }
    if (!exited) {
        kill(pid, SIGKILL);
        waitpid(pid, &status, 0);
        result.timed_out = true;
    }
    char buf[1024];
    while (true) {
        const ssize_t n = read(pipes[0], buf, sizeof(buf));
        if (n > 0) {
            result.output.append(buf, static_cast<std::size_t>(n));
        } else {
            break;
        }
    }
    close(pipes[0]);
    if (WIFEXITED(status)) {
        result.exit_code = WEXITSTATUS(status);
    }
    return result;
}

bool expectReject(const std::string& binary,
                  const std::vector<std::string>& args,
                  const fs::path& workspace,
                  const std::string& needle,
                  const std::string& label) {
    const ChildResult child = runEngine(binary, args, workspace);
    const bool started = child.output.find("BasicAgentPlugin") != std::string::npos
        || child.output.find("WORKSPACE_INDEX") != std::string::npos;
    if (child.timed_out || child.exit_code == 0 || started
        || child.output.find(needle) == std::string::npos) {
        fail(label + ": exit=" + std::to_string(child.exit_code)
             + " timeout=" + std::to_string(child.timed_out) + "\n" + child.output);
        return false;
    }
    return true;
}

bool testCli(const std::string& binary) {
    if (binary.empty() || !fs::is_regular_file(binary)) {
        fail("cli: engine binary missing");
        return false;
    }
    Workspace workspace("cli");
    const fs::path outside = workspace.root.parent_path() / "cli_outside";
    fs::remove_all(outside);
    fs::create_directories(outside);
    const fs::path file_path = workspace.root / "note.txt";
    const fs::path link = workspace.root / "escape.txt";
    writeText(file_path, "not a directory\n");
    writeText(outside / "secret.txt", "outside\n");
    std::error_code ec;
    fs::create_symlink(outside / "secret.txt", link, ec);
    if (ec) {
        fail("cli: symlink failed: " + ec.message());
        fs::remove_all(outside);
        return false;
    }
    const fs::path missing = workspace.root / "missing_dir";
    const bool ok =
        expectReject(binary, {"--index-directory", missing.string()}, workspace.root,
                     "not found", "cli-missing")
        && expectReject(binary, {"--index-directory", file_path.string()}, workspace.root,
                        "not a directory", "cli-file")
        && expectReject(binary, {"--index-directory", outside.string()}, workspace.root,
                        "outside the workspace", "cli-outside")
        && expectReject(binary, {"--index-directory", link.string()}, workspace.root,
                        "not a directory", "cli-file-link")
        && expectReject(binary, {"--index-directory"}, workspace.root,
                        "requires a directory path", "cli-no-path")
        && expectReject(binary, {"--serve", "--index-directory", missing.string()}, workspace.root,
                        "cannot be combined", "cli-serve")
        && expectReject(binary, {"--fixed-evidence-goal", file_path.string(),
                                 "--index-directory", (workspace.root).string()},
                        workspace.root, "cannot be combined", "cli-goal")
        && expectReject(binary, {"--index-directory", workspace.root.string(),
                                 "--index-directory", workspace.root.string()},
                        workspace.root, "one index directory per process", "cli-two");

    const fs::path escape_dir = workspace.root / "escape_dir";
    fs::create_directory_symlink(outside, escape_dir, ec);
    const bool link_rejected = !ec && expectReject(
        binary, {"--index-directory", escape_dir.string()}, workspace.root,
        "outside the workspace", "cli-dir-link");

    const ChildResult help = runEngine(binary, {"--help"}, workspace.root);
    const bool help_ok = !help.timed_out && help.exit_code == 0
        && help.output.find("--index-directory") != std::string::npos
        && help.output.find("BasicAgentPlugin") == std::string::npos;
    if (!help_ok) {
        fail("cli: help exit=" + std::to_string(help.exit_code) + "\n" + help.output);
    }
    fs::remove_all(outside);
    return ok && link_rejected && help_ok;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: thoth-workspace-index-tests <persist|no_evidence|preserve|isolate|cli> [engine]\n";
        return 2;
    }
    const std::string id = argv[1];
    bool ok = false;
    if (id == "persist") ok = testPersist();
    else if (id == "no_evidence") ok = testNoEvidence();
    else if (id == "preserve") ok = testPreserve();
    else if (id == "isolate") ok = testIsolate();
    else if (id == "cli") ok = testCli(argc >= 3 ? argv[2] : "");
    else {
        std::cerr << "unknown case: " << id << "\n";
        return 2;
    }
    return ok ? 0 : 1;
}
