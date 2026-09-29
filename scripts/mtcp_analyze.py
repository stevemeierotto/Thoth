#!/usr/bin/env python3
"""Apply sealed MTCP v1.0 section 17. No thresholds are configurable."""

from __future__ import annotations

import json
from collections import defaultdict
from dataclasses import dataclass, field


OVERFLOW_LIMIT = 8192


@dataclass
class Call:
    task_id: str
    call_type: str
    ordinal: int
    finish_reason: str
    prompt_tokens: int
    completion_tokens: int
    requested_max_tokens: int
    provider_ok: bool
    validation_ok: bool | None
    fallback_used: bool
    kept_existing_plan: bool
    context_overflow: bool
    generation_id: str

    @property
    def overflow_invalid(self) -> bool:
        if self.context_overflow:
            return True
        return self.prompt_tokens + self.requested_max_tokens > OVERFLOW_LIMIT

    @property
    def ceiling_pressure(self) -> bool:
        if self.overflow_invalid or not self.provider_ok:
            return False
        if self.finish_reason == "length":
            return True
        if self.requested_max_tokens <= 0:
            return False
        return self.completion_tokens / self.requested_max_tokens >= 0.95

    @property
    def structured_failure(self) -> bool:
        if self.call_type in {"plan", "plan_retry", "revision", "revision_retry"}:
            return self.validation_ok is False or self.fallback_used or self.kept_existing_plan
        if self.call_type == "synthesis":
            return self.provider_ok and self.completion_tokens == 0 and self.finish_reason != "stop"
        return False

    @property
    def length_associated(self) -> bool:
        return self.structured_failure and self.ceiling_pressure


def load_calls(rows: list[dict], *, mode: str) -> tuple[list[Call], list[str]]:
    grouped: dict[tuple[str, str], int] = defaultdict(int)
    calls = []
    invalid = []
    for row in rows:
        if row.get("event") != "GENERATION_CALL" and "call_type" not in row:
            continue
        task_id = row.get("task_id") or ""
        call_type = row["call_type"]
        grouped[(task_id, call_type)] += 1
        call = Call(
            task_id=task_id,
            call_type=call_type,
            ordinal=grouped[(task_id, call_type)],
            finish_reason=row.get("finish_reason") or "",
            prompt_tokens=int(row.get("prompt_tokens") or 0),
            completion_tokens=int(row.get("completion_tokens") or 0),
            requested_max_tokens=int(row.get("requested_max_tokens") or 0),
            provider_ok=bool(row.get("provider_ok")),
            validation_ok=row.get("validation_ok"),
            fallback_used=bool(row.get("fallback_used")),
            kept_existing_plan=bool(row.get("kept_existing_plan")),
            context_overflow=bool(row.get("context_overflow")),
            generation_id=row.get("generation_id") or "",
        )
        if call.overflow_invalid or not call.provider_ok:
            invalid.append(call.generation_id or f"{task_id}:{call_type}")
            if mode == "strict":
                continue
        calls.append(call)
    return calls, invalid


def pressure_count(calls: list[Call]) -> int:
    return sum(1 for call in calls if call.ceiling_pressure and not call.overflow_invalid)


def fallback_count(calls: list[Call]) -> int:
    return sum(1 for call in calls if call.fallback_used and not call.overflow_invalid)


def length_failures(calls: list[Call]) -> int:
    return sum(1 for call in calls if call.length_associated and not call.overflow_invalid)


def material(low: list[Call], high: list[Call], *, residual: int | None = None) -> bool:
    pressure_drop = pressure_count(low) - pressure_count(high)
    fallback_drop = fallback_count(low) - fallback_count(high)
    needed = 2 if residual is None else min(2, residual)
    clause1 = (
        pressure_count(low) >= 1
        and pressure_drop >= needed
        and length_failures(high) <= length_failures(low)
        and fallback_count(high) <= fallback_count(low)
    )
    removed = [
        call for call in low
        if call.fallback_used and call.length_associated
    ]
    clause2 = (
        fallback_drop >= 1
        and bool(removed)
        and pressure_count(high) <= pressure_count(low)
    )
    return clause1 or clause2


def adequate(calls: list[Call]) -> bool:
    return pressure_count(calls) == 0 and length_failures(calls) == 0


def select(stage_a_low: list[Call], stage_a_high: list[Call], stage_b: list[Call] | None) -> dict:
    if any(call.overflow_invalid for call in stage_a_low + stage_a_high):
        return {"status": "invalid_condition", "selected": None, "stage_b": False}
    improved = material(stage_a_low, stage_a_high)
    if not improved:
        return {
            "status": "selected",
            "selected": 512,
            "adequate": adequate(stage_a_low),
            "stage_b": False,
            "label": "512 selected" if adequate(stage_a_low) else "512 selected; adequacy not met",
        }
    if adequate(stage_a_high):
        return {
            "status": "selected",
            "selected": 1024,
            "adequate": True,
            "stage_b": False,
            "label": "1024 selected",
        }
    if stage_b is None:
        return {"status": "stage_b_required", "selected": None, "stage_b": True, "label": "Stage B required"}
    if any(call.overflow_invalid for call in stage_b):
        return {"status": "invalid_condition", "selected": None, "stage_b": True}
    residual = pressure_count(stage_a_high)
    if material(stage_a_high, stage_b, residual=residual):
        ok = adequate(stage_b)
        return {
            "status": "selected",
            "selected": 2048,
            "adequate": ok,
            "stage_b": True,
            "label": "2048 selected" if ok else "2048 selected; adequacy not met",
        }
    return {
        "status": "selected",
        "selected": 1024,
        "adequate": False,
        "stage_b": True,
        "label": "1024 selected; adequacy not met",
    }


def majority_pressed(rows_by_rep: list[list[Call]], slot: tuple[str, str, int]) -> bool:
    hits = 0
    for calls in rows_by_rep:
        for call in calls:
            if (call.task_id, call.call_type, call.ordinal) == slot and call.ceiling_pressure:
                hits += 1
    return hits >= 2
