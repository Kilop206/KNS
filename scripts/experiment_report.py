"""Offline overview of an experiment bundle and its recorded evidence."""

import json
import os
from html import escape
from pathlib import Path
from urllib.parse import quote

import experiment_manifest


def write(
    output: Path,
    manifest: dict,
    comparison: dict,
    validation: dict | None,
    source: Path | None = None,
) -> None:
    source = source or output
    profile = manifest["configuration"]["profile"]

    def text(value):
        return escape(str(value), quote=True)

    def link(relative, label, root=output):
        path = Path(relative)
        # Only link existing evidence contained in this bundle.
        try:
            target = (root / path).resolve()
            target.relative_to(root.resolve())
        except ValueError:
            return text(label)
        if not target.is_file():
            return text(label)
        href = Path(os.path.relpath(target, output)).as_posix()
        return f'<a href="{quote(href, safe="/")}">{text(label)}</a>'

    benchmark = json.loads(
        (source / "runs" / "benchmark.json").read_text(encoding="utf-8")
    )
    verdicts = (
        {case["case_id"]: case for case in validation["cases"]} if validation else {}
    )
    rows = []
    for case in benchmark["cases"]:
        verdict = verdicts.get(case["case_id"])
        evidence = []
        for field, label in (
            ("csv", "CSV"),
            ("log", "Log"),
            ("traffic_report", "Traffic JSON"),
            ("analytics_report", "Analytics JSON"),
            ("fault_report", "Fault JSON"),
        ):
            if case.get(field):
                # Older bundles record absolute paths. Files live directly in runs/.
                artifact = experiment_manifest.case_artifact(source, case[field])
                evidence.append(link(artifact.relative_to(source.resolve()), label, source))
        verdict_text = "No rules configured"
        if verdict:
            verdict_text = text(verdict["status"])
            if verdict.get("report"):
                verdict_text = link(
                    Path(verdict["report"]).with_suffix(".html"), verdict["status"]
                )
            if verdict.get("failed_assertions") is not None:
                verdict_text += f' ({text(verdict["failed_assertions"])} failed assertions)'
            if verdict.get("baseline_case_id"):
                verdict_text += f'<small>Reference case {text(verdict["baseline_case_id"])}</small>'
            if verdict.get("error"):
                verdict_text += f'<small>{text(verdict["error"])}</small>'
        error = f'<small>{text(case["error"])}</small>' if case.get("error") else ""
        rows.append(
            f'<tr><th scope="row">{text(case["case_id"])}</th>'
            f'<td>{text(case["status"])}{error}</td>'
            f'<td>{verdict_text}</td><td>{" · ".join(evidence)}</td></tr>'
        )

    validation_section = "<p>No validation rules configured.</p>"
    if validation:
        validation_section = (
            f'<p>{text(validation["passed_cases"])} passed, '
            f'{text(validation["failed_cases"])} failed, '
            f'{text(validation["error_cases"])} errors. '
            f'{link("validation/summary.json", "Validation summary JSON")} · '
            f'{link("validation/summary.xml", "JUnit suite")} · '
            f'{link("validation/rules.json", "Rules JSON")}</p>'
        )
    artifacts = " · ".join(
        link(path, label, source) for path, label in (
            ("manifest.json", "Manifest"),
            ("profile.json", "Profile"),
            ("source-topology.json", "Source topology"),
            ("runs/benchmark.json", "Benchmark JSON"),
            ("runs/metrics.csv", "Metrics CSV"),
            ("runs/report.md", "Benchmark Markdown"),
        )
    )
    analysis_section = ""
    if (output / "analysis.json").is_file() and source != output:
        analysis = json.loads((output / "analysis.json").read_text(encoding="utf-8"))
        analysis_section = (
            '<section><h2>Saved-result analysis</h2>'
            f'<p>Analysis {text(analysis["status"])} · Exit code {text(analysis["returncode"])}. '
            f'{link("analysis.json", "Analysis provenance JSON")}</p>'
            '<p>The execution status below comes from the original manifest.</p></section>'
        )
    provenance = "".join(
        f"<dt>{text(label)}</dt><dd><code>{text(manifest.get(key))}</code></dd>"
        for key, label in (
            ("run_id", "Run ID"),
            ("replay_of", "Replay of"),
            ("source_revision", "Source revision"),
            ("configuration_sha256", "Configuration SHA-256"),
            ("executable_sha256", "Executable SHA-256"),
            ("created_at", "Created UTC"),
            ("finished_at", "Finished UTC"),
        ) if manifest.get(key) is not None
    )
    document = f"""<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<meta http-equiv="Content-Security-Policy" content="default-src 'none'; style-src 'unsafe-inline'; base-uri 'none'; form-action 'none'">
<title>{text(profile["name"])} — KNS experiment</title>
<style>
body {{ margin: 0; background: #f3f6f8; color: #203348; font: 16px/1.6 system-ui, sans-serif; }}
main {{ max-width: 1200px; margin: auto; padding: 28px; }}
h1 {{ margin-bottom: 8px; overflow-wrap: anywhere; }}
h2 {{ font-size: 1.25rem; margin-top: 0; }}
section {{ background: white; border: 1px solid #cbd6df; border-radius: 10px; padding: 22px; margin: 22px 0; }}
a {{ color: #175f91; text-underline-offset: 3px; }}
a:focus-visible {{ outline: 3px solid #175f91; outline-offset: 3px; }}
.table {{ overflow-x: auto; }}
table {{ width: 100%; border-collapse: collapse; text-align: left; }}
th, td {{ border-bottom: 1px solid #d9e2e9; padding: 12px; vertical-align: top; }}
tbody th {{ max-width: 360px; overflow-wrap: anywhere; font-weight: 500; }}
small {{ display: block; overflow-wrap: anywhere; }}
dt {{ margin-top: 12px; font-weight: 600; }}
dd {{ margin-left: 0; overflow-wrap: anywhere; }}
@media (max-width: 600px) {{ main {{ padding: 12px; }} section {{ padding: 14px; }} }}
</style>
</head>
<body><main>
<h1>{text(profile["name"])}</h1>
<p>Experiment {text(manifest["status"])} · Exit code {text(manifest["returncode"])}
· {text(benchmark["case_count"])} cases</p>
{analysis_section}
<section><h2>Validation</h2>{validation_section}</section>
<section><h2>Cases</h2>
<p>Execution status and rule verdicts are recorded for each case.</p>
<div class="table"><table><thead><tr><th scope="col">Case</th>
<th scope="col">Execution</th><th scope="col">Validation</th><th scope="col">Evidence</th>
</tr></thead><tbody>{"".join(rows)}</tbody></table></div></section>
<section><h2>Comparison</h2>
<p>Reference variant {text(profile["baseline"])}.
{text(comparison["matched_pairs"])} of {text(comparison["expected_pairs"])} expected pairs matched.</p>
<p>{link("comparison.html", "Comparison HTML")} ·
{link("comparison.json", "Comparison JSON")} · {link("comparison.csv", "Comparison CSV")}</p>
</section>
<section><h2>Inputs and execution evidence</h2><p>{artifacts}</p><dl>{provenance}</dl>
<p>The linked reports document metric populations, model assumptions and comparison methodology.</p>
</section>
</main></body></html>
"""
    (output / "report.html").write_text(document, encoding="utf-8")
