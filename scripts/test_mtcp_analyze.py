#!/usr/bin/env python3
"""Synthetic MTCP decision, manifest, and S0 checks. No model traffic."""

from __future__ import annotations

import json
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))

from mtcp_analyze import Call, load_calls, material, pressure_count, select  # noqa: E402
from mtcp_manifest import validate  # noqa: E402


def call(task, kind, *, length=False, fallback=False, overflow=False, tokens=10, requested=512, provider_ok=True, validation_ok=True):
    completion = requested if length else tokens
    return {
        "event": "GENERATION_CALL",
        "task_id": task,
        "call_type": kind,
        "finish_reason": "length" if length else "stop",
        "prompt_tokens": 8000 if overflow else 20,
        "completion_tokens": completion,
        "requested_max_tokens": requested,
        "provider_ok": provider_ok,
        "validation_ok": validation_ok,
        "fallback_used": fallback,
        "context_overflow": overflow,
        "generation_id": f"{task}-{kind}-{length}-{fallback}-{overflow}",
    }


def calls(rows):
    loaded, _invalid = load_calls(rows, mode="keep")
    return loaded


def assert_eq(actual, expected, label):
    if actual != expected:
        raise SystemExit(f"{label}: {actual!r} != {expected!r}")


def test_stage_branches():
    quiet = calls([call("C1", "chat"), call("G1", "plan")])
    same = calls([call("C1", "chat"), call("G1", "plan")])
    result = select(quiet, same, None)
    assert_eq(result["selected"], 512, "no improvement")
    assert_eq(result["stage_b"], False, "stage b stays closed")

    pressured = calls([
        call("C3", "chat", length=True),
        call("G2", "synthesis", length=True),
        call("G1", "plan"),
    ])
    cleared = calls([call("C3", "chat"), call("G2", "synthesis"), call("G1", "plan")])
    result = select(pressured, cleared, None)
    assert_eq(result["selected"], 1024, "adequate 1024")
    assert_eq(result["stage_b"], False, "clean 1024 skips stage b")

    heavily = calls([
        call("C3", "chat", length=True),
        call("G2", "synthesis", length=True),
        call("G1", "plan", length=True),
    ])
    still = calls([call("C3", "chat", length=True), call("G2", "synthesis"), call("G1", "plan")])
    result = select(heavily, still, None)
    assert_eq(result["status"], "stage_b_required", "residual requires stage b")

    residual_low = calls([
        call("C3", "chat", length=True),
        call("G2", "synthesis", length=True),
        call("G1", "plan", length=True),
    ])
    residual_mid = calls([
        call("C3", "chat", length=True),
        call("G2", "synthesis"),
        call("G1", "plan"),
    ])
    residual_high = calls([call("C3", "chat"), call("G2", "synthesis"), call("G1", "plan")])
    assert material(residual_mid, residual_high, residual=1)
    result = select(residual_low, residual_mid, residual_high)
    assert_eq(result["selected"], 2048, "min(2,N) clears one residual")

    four = calls([
        call("C1", "chat", length=True),
        call("C2", "chat", length=True),
        call("C3", "chat", length=True),
        call("G2", "synthesis", length=True),
    ])
    two = calls([
        call("C1", "chat"),
        call("C2", "chat"),
        call("C3", "chat", length=True),
        call("G2", "synthesis", length=True),
    ])
    one_left = calls([
        call("C1", "chat"),
        call("C2", "chat"),
        call("C3", "chat", length=True),
        call("G2", "synthesis"),
    ])
    assert not material(two, one_left, residual=2)
    result = select(four, two, one_left)
    assert_eq(result["selected"], 1024, "one of two residual slots is not enough")

    worse = calls([call("C3", "chat"), call("G1", "plan", fallback=True, validation_ok=False, length=True)])
    assert not material(pressured, worse)
    long_stop = calls([call("G2", "synthesis", tokens=400, requested=512)])
    assert pressure_count(long_stop) == 0

    overflow = calls([call("C3", "chat", length=True, overflow=True, requested=2048)])
    result = select(quiet, overflow, None)
    assert_eq(result["status"], "invalid_condition", "overflow invalidates")
    assert_eq(pressure_count(overflow), 0, "overflow is not ceiling pressure")


def test_manifest_and_s0():
    good = {
        "protocol": "MTCP v1.0",
        "lifecycle": "SEALED",
        "seal_commit": "a3690f7fc50da920992829bcbc39e19aa983894f",
        "product_sha": "a",
        "engine_sha": "b",
        "engine_image": "thoth-engine",
        "llama_image_digest": "sha256:abc",
        "llama_build": "b9994",
        "chat_model_path": "/models/chat.gguf",
        "chat_model_sha256": "1" * 64,
        "embedding_model_path": "/models/embed.gguf",
        "embedding_model_sha256": "2" * 64,
        "n_ctx": 8192,
        "embedding_context": 512,
        "generation_ceiling": 512,
        "temperature": 0.7,
        "top_p": 1.0,
        "top_k": 40,
        "min_p": 0.05,
        "seed_mode": "deterministic",
        "seed": 17001,
        "inference_backend": "llama_cpp",
        "inference_mode": "chat",
        "text_timeout_seconds": 900,
        "embedding_timeout_seconds": 300,
        "embedding_strict": 0,
        "reflection_maximum": 2,
        "retrieval_config_sha256": "b4eab95e104f84de05ee7c1ac26e1117447104c983ce924942e987db073df396",
        "corpus_set_sha256": "155f9f0be3a5210730fd5042bcaf228ea1d4421d21b411ea1890e769d58a8921",
        "appendix_asset_sha256": "7d8a3b4bd07663737c50d3a56304dedaca37e2b6db9cddea8c6e0140a353ec39",
        "task_set_sha256": "a41031ff84308edbaf40804f4e96aae8b85395a0ff9e00fffc7afe402002ec90",
        "s0_char_id": "s0",
        "s0_char_ok": True,
        "stage": "A",
        "started_at": "2026-09-28T00:00:00Z",
    }
    if validate(good):
        raise SystemExit(f"valid manifest rejected: {validate(good)}")
    bad = dict(good)
    bad["n_ctx"] = 2048
    if not validate(bad):
        raise SystemExit("bad context was accepted")

    retrieval = (ROOT / "docker/experiment/retrieval_config.json").read_bytes()
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        workspace = root / "ws"
        workspace.mkdir()
        (workspace / "retrieval_config.json").write_bytes(retrieval)
        for name in (
            "mtcp_configuration.md",
            "mtcp_recovery_order.md",
            "mtcp_service_failure.md",
        ):
            (workspace / name).write_bytes((ROOT / "docs/mtcp" / name).read_bytes())
        archive = root / "s0.tar"
        subprocess.check_call([sys.executable, str(ROOT / "scripts/mtcp_s0.py"), "snapshot", "--workspace", str(workspace), "--archive", str(archive)])
        (workspace / "extra.txt").write_text("contamination", encoding="utf-8")
        failed = subprocess.run([sys.executable, str(ROOT / "scripts/mtcp_s0.py"), "verify", "--workspace", str(workspace), "--archive", str(archive)])
        if failed.returncode == 0:
            raise SystemExit("contamination was not detected")
        subprocess.check_call([sys.executable, str(ROOT / "scripts/mtcp_s0.py"), "restore", "--workspace", str(workspace), "--archive", str(archive)])
        if (workspace / "extra.txt").exists():
            raise SystemExit("restore left contamination in place")


def test_c6_ignores_generation_rows():
    sys.path.insert(0, str(ROOT / "scripts"))
    from c6_longitudinal_join import validate_app_log_rows  # noqa: E402
    from types import SimpleNamespace
    summary = SimpleNamespace(total_invalid=0)
    rows = [{"event_name": "GENERATION_CALL", "task_id": "C1", "_source_line": 1}]
    valid = validate_app_log_rows(rows, summary, False)
    if valid:
        raise SystemExit("C6 analyzer accepted generation telemetry")


if __name__ == "__main__":
    test_stage_branches()
    test_manifest_and_s0()
    test_c6_ignores_generation_rows()
    print("mtcp synthetic checks passed")
