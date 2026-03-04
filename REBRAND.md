REBRAND.md — Thoth Project Initialization Plan
Objective

Create a fully independent project named Thoth by rebuilding the existing codebase into a new root directory.

When complete:

The project name will be Thoth

No references to any previous project name will remain

No historical branding will exist in source, documentation, binaries, or UI

The result will appear as a brand new system

This is a controlled reconstruction and identity reset.

Phase 0 — Source and Target
Source Directory
/home/steve/Hawkeinstein2

This is the reference codebase.

Target Directory
/home/steve/Thoth

This is the new project root.

Do not modify the source directory.

All work happens inside /home/steve/Thoth.

Phase 1 — Clean Structural Copy

Perform a full recursive copy of all files and directories from:

/home/steve/Hawkeinstein2

into:

/home/steve/Thoth

Requirements:

Preserve directory structure

Preserve file permissions

Preserve hidden files

Preserve CMake configuration

Preserve third-party dependencies

Do not modify file content during this phase

Phase 2 — Global Identity Replacement

After copying, perform a complete identity transformation inside /home/steve/Thoth.

2.1 Replace All Project Identifiers

Replace every occurrence of:

Hawkeinstein2

Hawkeinstein
hawkeinstein

With:

Thoth

Thoth

thoth

Apply to:

Class names

Namespaces

Methods

Variables

Comments

String literals

Documentation

Build configuration

Window titles

Test names

Restrictions:

Do not modify third-party library code

Do not modify binary files

Do not modify compiled artifacts

Do not modify generated build directories

2.2 CMake Project Name

In root CMakeLists.txt, ensure:

project(Thoth)

Update:

Executable target names

Library output names (if project-branded)

Install rules if branded

2.3 Namespace Consistency

If a namespace exists for project branding, it must become:

namespace thoth {

All internal references must match.

No mixed casing allowed.

2.4 Header Guards

Convert header guards to match the new project identity.

Example pattern:

THOTH_<FILE_NAME>_H

Apply consistently.

2.5 GUI Branding

Ensure UI strings reflect only:

Thoth

Thoth Control Panel (if applicable)

No legacy names may appear in:

Window titles

Dialog boxes

Logs

About panels

Error messages

Phase 3 — Documentation Reset

Update:

AGENTS.md

docs/improvements.md

docs/completed_improvements_log.md

README.md (if present)

DEPENDENCIES.md (if project-branded)

Requirements:

Remove any historical references

Remove identity transition language

Remove any explanation of prior naming

Present Thoth as a standalone system

Documentation should read as if Thoth has always existed.

Phase 4 — Memory State Reset

If the project contains:

agent_workspace/

Delete the entire directory inside /home/steve/Thoth.

Do not edit internal files individually.

Allow the application to regenerate runtime state naturally.

Phase 5 — Build Validation

Inside /home/steve/Thoth:

Configure:

cmake --preset <preset>

Build:

cmake --build --preset <preset>

Run unit tests:

ctest

or:

./tests/unit_tests

Launch application and verify:

No runtime errors

No missing symbol errors

No namespace mismatches

No UI branding issues

Phase 6 — Clean Build Artifacts

Ensure the following do not exist before finalizing:

Old build directories

Old binaries with previous names

Cached CMake artifacts referencing prior identity

Perform a clean rebuild from scratch.

Phase 7 — Final Verification Sweep

Perform a full project text search for:

hawkeinstein

Hawkeinstein

HAWKEINSTEIN

The result must return zero matches inside /home/steve/Thoth.

If any matches remain, correct them.

Phase 8 — Initialize Version Control

Inside /home/steve/Thoth:

git init
git add .
git commit -m "Initial commit"
git branch -M main
git remote add origin <Thoth repo URL>
git push -u origin main

This must be the first and only historical commit in the repository.

The repository must not reference prior identity in commit messages.

End State

Thoth is:

A standalone C++ AI agent system

Built with CMake

Using a hybrid memory architecture

Using a registry-based tool system

Using a RAG pipeline

Fully branded as Thoth

With no trace of prior naming
