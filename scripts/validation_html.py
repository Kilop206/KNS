"""Deterministic, escaped, offline HTML for evaluated simulation assertions."""
import hashlib
from html import escape
import json
from pathlib import Path

STYLE = """
*{box-sizing:border-box}body{margin:0;background:#f3f6fa;color:#172b43;font:15px/1.55 system-ui,sans-serif}
main{max-width:1280px;margin:auto;padding:clamp(16px,3vw,36px)}h1{font-size:clamp(24px,4vw,36px);line-height:1.2;margin:8px 0 16px}
h2{font-size:19px;margin:0 0 12px}.eyebrow{font-weight:700;letter-spacing:.08em;font-size:12px;color:#475569}
.summary{display:flex;justify-content:space-between;align-items:center;gap:16px;flex-wrap:wrap}.badge{border-radius:999px;padding:5px 12px;font-weight:700;white-space:nowrap}
.pass{color:#065f46;background:#d1fae5}.fail{color:#991b1b;background:#fee2e2}.card{background:white;border:1px solid #d5dfeb;border-radius:12px;padding:20px;margin:20px 0}
p{margin:10px 0}.muted,small{color:#475569}small{display:block;font-size:12px}nav{display:flex;gap:16px;flex-wrap:wrap}a{color:#1754a1;text-underline-offset:3px}
.table-wrap{overflow-x:auto}.table-wrap:focus-visible{outline:3px solid #1754a1;outline-offset:4px}table{border-collapse:collapse;width:100%;min-width:980px}th,td{padding:14px 12px;border-bottom:1px solid #dbe3ed;text-align:left;vertical-align:top}thead th{background:#edf2f8;font-size:12px;text-transform:uppercase;letter-spacing:.04em}
tbody th{font-weight:600;max-width:230px}.value{font-variant-numeric:tabular-nums;font-weight:600;white-space:nowrap}code,pre{font-family:ui-monospace,monospace;font-size:12px;overflow-wrap:anywhere}code{word-break:break-all}pre{white-space:pre-wrap;margin:8px 0}
summary{cursor:pointer;color:#1754a1;font-size:13px}details p{font-size:12px;max-width:270px}.provenance{display:grid;grid-template-columns:minmax(160px,220px) minmax(0,1fr);gap:8px 16px;margin:0}.provenance dt{font-weight:600}.provenance dd{margin:0}.limitations{border-left:4px solid #5279a7}.reason{max-width:220px;margin-top:8px}
@media(max-width:700px){.card{padding:14px}.provenance{grid-template-columns:1fr;gap:4px}.provenance dd{margin-bottom:10px}}
@media print{body{background:white}main{max-width:none;padding:0}.card{break-inside:avoid}nav{display:none}a{color:inherit}}
"""


def text(value):
    content = "N/A" if value is None else str(value)
    if len(content) > 4096:
        content = content[:4096] + " [truncated; full value in JSON]"
    return escape(content, quote=True)


def number(value):
    # Preserve the round-trip numeric text; a shortened display could hide a
    # threshold violation at adjacent floating-point values.
    return "N/A" if value is None else text(value)


def render(result):
    checks = result["checks"]
    if not checks or type(result["passed"]) is not bool or any(type(c["passed"]) is not bool for c in checks):
        raise ValueError("Validation HTML requires nonempty evaluated boolean verdicts")
    failed = sum(not check["passed"] for check in checks)
    if result["assertion_count"] != len(checks) or result["failed_assertions"] != failed or result["passed"] != (failed == 0):
        raise ValueError("Validation HTML verdict/counts are inconsistent")
    rows = []
    for check in checks:
        reference = check.get("baseline")
        baseline = '<span class="muted">Absolute rule</span>'
        if reference is not None:
            adjustment = (
                f'Change {number(reference["percent"])}%'
                if "percent" in reference
                else f'Offset {number(reference["offset"])} {text(reference["unit"])}'
            )
            baseline = (
                f'<span class="value">{number(reference["observed"])}</span>'
                f'<small>{adjustment}</small>'
            )
        expected = check.get("expected_exact", check["expected"])
        evidence = text(json.dumps(check["evidence"], sort_keys=True, indent=2, allow_nan=False))
        details = f'<p><strong>Candidate scope:</strong> {text(check["scope"])}</p><pre>{evidence}</pre>'
        if reference is not None:
            details += f'<p><strong>Baseline scope:</strong> {text(reference["scope"])}</p><pre>{text(json.dumps(reference["evidence"], sort_keys=True, indent=2, allow_nan=False))}</pre>'
        css, label = ("pass", "PASS") if check["passed"] else ("fail", "FAIL")
        flow = "All selected flows" if check["flow_id"] is None else f'Flow {check["flow_id"]}'
        rows.append(f'<tr><th scope="row">{text(check["name"])}<small>{text(check["metric"])}</small><small>{text(flow)}</small></th>'
            f'<td><span class="value">{number(check["observed"])}</span><small>{text(check["unit"])}</small></td>'
            f'<td><span class="value">{text(check["operator"])} {number(expected)}</span><small>{text(check["unit"])}</small></td>'
            f'<td>{baseline}</td><td><span class="badge {css}">{label}</span><small class="reason">{text(check["reason"])}</small></td>'
            f'<td><details><summary>Scope and evidence</summary>{details}</details></td></tr>')
    origins = [("Candidate report", "source_sha256"), ("Baseline report", "baseline_sha256"),
        ("Assertion rules", "rules_sha256"), ("Workload configuration", "configuration_sha256"),
        ("Baseline workload", "baseline_configuration_sha256"), ("Validator source", "validator_sha256")]
    provenance = ''.join(f'<dt>{text(label)}</dt><dd><code>{text(result[key])}</code></dd>' for label,key in origins if result.get(key) is not None)
    renderer_hash = hashlib.sha256(Path(__file__).read_bytes()).hexdigest()
    provenance += f'<dt>HTML exporter source</dt><dd><code>{renderer_hash}</code></dd>'
    css, label = ("pass", "Assertions passed") if result["passed"] else ("fail", "Assertions failed")
    return f"""<!doctype html>
<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<meta http-equiv="Content-Security-Policy" content="default-src 'none'; style-src 'unsafe-inline'; base-uri 'none'; form-action 'none'">
<title>KNS Validate - {label}</title><style>{STYLE}</style></head><body><main>
<header><div class="eyebrow">KNS VALIDATE / SIMULATION ASSERTIONS</div><div class="summary"><h1>{label}</h1><span class="badge {css}">{len(checks)-failed} / {len(checks)} passed</span></div>
<p class="muted">Declared model outputs checked against explicit rules. This verdict does not certify a real network.</p>
<nav aria-label="Report formats"><a href="validation.json">JSON data</a><a href="validation.xml">JUnit XML</a><a href="validation.md">Markdown</a></nav></header>
<section class="card"><h2>Model and format</h2><p>{text(result.get('model', 'Model label not supplied'))}</p><small>Result schema {text(result['schema_version'])}. Numeric values retain full round-trip precision; unavailable values are N/A.</small></section>
<section class="card"><h2>Assertion results</h2><div class="table-wrap" tabindex="0" role="region" aria-label="Assertion results table"><table><thead><tr><th>Assertion</th><th>Observed</th><th>Effective threshold</th><th>Baseline</th><th>Verdict</th><th>Population / retention</th></tr></thead><tbody>{''.join(rows)}</tbody></table></div></section>
<section class="card limitations"><h2>Scope and limitations</h2><p>{text(result['limitations'])}</p></section>
<section class="card"><h2>Reproducibility hashes</h2><dl class="provenance">{provenance}</dl></section>
</main></body></html>
"""


def write(result, path):
    path.write_text(render(result), encoding="utf-8")
