"""Normalize and merge ESLint-style UMG lint reports."""

from __future__ import annotations

from typing import Any, Callable, Dict, Iterable, List, Optional, Set, Tuple


def issue_fingerprint(issue: Dict[str, Any]) -> Tuple[str, str, str]:
    return (
        str(issue.get("ruleId", "")),
        str(issue.get("widgetPath", "")),
        str(issue.get("message", "")),
    )


def _issue_key(issue: Dict[str, Any]) -> tuple:
    return (
        issue.get("ruleId", ""),
        issue.get("widgetPath", ""),
        issue.get("message", ""),
        tuple(issue.get("relatedWidgets") or []),
    )


def summarize_issues(issues: Iterable[Dict[str, Any]]) -> Dict[str, int]:
    summary = {"error": 0, "warning": 0, "info": 0}
    for issue in issues:
        severity = str(issue.get("severity", "info")).lower()
        if severity not in summary:
            severity = "info"
        summary[severity] += 1
    return summary


def normalize_report(raw: Optional[Dict[str, Any]]) -> Dict[str, Any]:
    """Ensure a lint report has summary/ok/issues fields."""
    if not raw:
        return {
            "success": False,
            "ok": False,
            "geometryTrusted": False,
            "summary": {"error": 0, "warning": 0, "info": 0},
            "issues": [],
            "error": "Empty lint response.",
        }

    if raw.get("success") is False and not raw.get("issues"):
        return {
            "success": False,
            "ok": False,
            "geometryTrusted": raw.get("geometryTrusted", False),
            "summary": {"error": 1, "warning": 0, "info": 0},
            "issues": [],
            "error": raw.get("error", "Lint failed."),
            **{k: v for k, v in raw.items() if k not in {"ok", "summary", "issues"}},
        }

    issues = list(raw.get("issues") or [])
    summary = raw.get("summary") or summarize_issues(issues)
    geometry_trusted = bool(raw.get("geometryTrusted", True))
    ok = raw.get("ok")
    if ok is None:
        ok = summary.get("error", 0) == 0 and geometry_trusted

    normalized = dict(raw)
    normalized["issues"] = issues
    normalized["summary"] = summary
    normalized["geometryTrusted"] = geometry_trusted
    normalized["ok"] = ok
    if "success" not in normalized:
        normalized["success"] = True
    return normalized


def merge_reports(*reports: Dict[str, Any]) -> Dict[str, Any]:
    """Merge multiple lint reports, deduplicating issues."""
    merged_issues: List[Dict[str, Any]] = []
    seen = set()
    asset_path = None
    success = True
    geometry_trusted = True
    errors: List[str] = []

    for report in reports:
        normalized = normalize_report(report)
        if normalized.get("success") is False:
            success = False
            if normalized.get("error"):
                errors.append(str(normalized["error"]))
        if not asset_path and normalized.get("assetPath"):
            asset_path = normalized["assetPath"]
        geometry_trusted = geometry_trusted and bool(normalized.get("geometryTrusted", True))

        for issue in normalized.get("issues", []):
            key = _issue_key(issue)
            if key in seen:
                continue
            seen.add(key)
            merged_issues.append(issue)

    summary = summarize_issues(merged_issues)
    result: Dict[str, Any] = {
        "success": success,
        "ok": summary["error"] == 0 and geometry_trusted,
        "geometryTrusted": geometry_trusted,
        "summary": summary,
        "issues": merged_issues,
    }
    if asset_path:
        result["assetPath"] = asset_path
    if errors:
        result["error"] = "; ".join(errors)
    return result


def is_auto_applicable_fix(fix_suggestion: Optional[Dict[str, Any]]) -> bool:
    if not fix_suggestion:
        return False
    if fix_suggestion.get("action") != "set_widget_properties":
        return False
    meta = fix_suggestion.get("meta") or {}
    return bool(meta.get("autoApplicable"))


def detect_stalled(previous: Dict[str, Any], current: Dict[str, Any]) -> bool:
    prev_fps: Set[Tuple[str, str, str]] = {
        issue_fingerprint(issue) for issue in previous.get("issues", [])
    }
    curr_fps: Set[Tuple[str, str, str]] = {
        issue_fingerprint(issue) for issue in current.get("issues", [])
    }
    return bool(prev_fps) and prev_fps == curr_fps


def collect_error_issues(report: Dict[str, Any]) -> List[Dict[str, Any]]:
    return [
        issue
        for issue in report.get("issues", [])
        if str(issue.get("severity", "")).lower() == "error"
    ]


async def auto_fix_lint_loop(
    lint_fn: Callable[[], Any],
    apply_fix_fn: Callable[[Dict[str, Any]], Any],
    *,
    max_iterations: int = 3,
) -> Dict[str, Any]:
    """Bounded lint/fix loop with stall detection."""
    history: List[Dict[str, Any]] = []
    previous_report: Optional[Dict[str, Any]] = None

    for iteration in range(1, max_iterations + 1):
        raw = await lint_fn()
        report = normalize_report(raw)
        history.append(
            {
                "iteration": iteration,
                "phase": "lint",
                "report": report,
            }
        )

        if report.get("ok"):
            return {
                "status": "ok",
                "iterations": iteration,
                "history": history,
                "report": report,
            }

        if not report.get("geometryTrusted", True):
            return {
                "status": "blocked",
                "reason": "geometry_not_trusted",
                "iterations": iteration,
                "history": history,
                "report": report,
            }

        if previous_report and detect_stalled(previous_report, report):
            return {
                "status": "stalled",
                "reason": "unchanged_issues",
                "iterations": iteration,
                "history": history,
                "report": report,
            }

        fixable = next(
            (
                issue
                for issue in collect_error_issues(report)
                if is_auto_applicable_fix(issue.get("fixSuggestion"))
            ),
            None,
        )
        if not fixable:
            return {
                "status": "manual_required",
                "reason": "no_auto_fixable_errors",
                "iterations": iteration,
                "history": history,
                "report": report,
            }

        fix = fixable["fixSuggestion"]
        params = fix.get("params") or {}
        apply_result = await apply_fix_fn(params)
        history.append(
            {
                "iteration": iteration,
                "phase": "apply_fix",
                "issue": issue_fingerprint(fixable),
                "result": apply_result,
            }
        )

        if apply_result.get("success") is False:
            return {
                "status": "apply_failed",
                "iterations": iteration,
                "history": history,
                "report": report,
                "apply_result": apply_result,
            }

        previous_report = report

    final_raw = await lint_fn()
    final_report = normalize_report(final_raw)
    history.append({"iteration": max_iterations, "phase": "lint", "report": final_report})
    return {
        "status": "max_iterations_reached",
        "iterations": max_iterations,
        "history": history,
        "report": final_report,
    }
