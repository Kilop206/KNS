#!/usr/bin/env python3
"""Export a complete native Analytics JSON as offline HTML, CSV and Markdown."""
import argparse
import csv
from html import escape
import json
from pathlib import Path
import sys

import analytics_config


def get(value, *keys):
    for key in keys:
        value = value.get(key) if isinstance(value, dict) else None
    return value


def fmt(value):
    return "N/A" if value is None else f"{value:.6g}"


def write_csv(path, fields, rows):
    with path.open("w", encoding="utf-8", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=fields)
        writer.writeheader()
        writer.writerows(rows)


def plot(points, title, ceiling=None):
    valid = [(t, value) for t, value in points if value is not None]
    if not valid:
        return '<p class="muted">No observations available for this series.</p>'
    first, last = points[0][0], points[-1][0]
    high = ceiling if ceiling is not None else max(value for _, value in valid) or 1.0
    width = last - first or 1.0
    paths, segment = [], []
    for at, value in points:
        if value is None:
            if segment:
                paths.append(segment)
            segment = []
        else:
            segment.append((45 + 600 * ((at - first) / width), 150 - 125 * (value / high)))
    if segment:
        paths.append(segment)
    shapes = []
    for segment in paths:
        if len(segment) > 1:
            coordinates = " ".join(f"{x:.3f},{y:.3f}" for x, y in segment)
            shapes.append(f'<polyline points="{coordinates}" fill="none" stroke="#21688e" stroke-width="2"/>')
        else:
            x, y = segment[0]
            shapes.append(f'<circle cx="{x:.3f}" cy="{y:.3f}" r="3" fill="#21688e"/>')
    label = escape(title, quote=True)
    return (f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 680 180" role="img" aria-label="{label}"><title>{label}</title>'
            f'<path d="M45 20 V150 H645" fill="none" stroke="#a0b0be"/>{"".join(shapes)}'
            f'<text x="4" y="28">{fmt(high)}</text><text x="20" y="150">0</text>'
            f'<text x="45" y="173">{fmt(first)} s</text><text x="590" y="173">{fmt(last)} s</text></svg>')


def write(report, output, source):
    samples = report["samples"]
    series = [{key: sample.get(key) for key in ("at_s", "from_s", "duration_s", "throughput_pps", "first_hop_admission_pps", "drops_per_s")} |
              {"data_count": get(sample, "data_latency", "count"), "mean_latency_s": get(sample, "data_latency", "mean_s"),
               "p95_latency_s": get(sample, "data_latency", "p95_s"), "percentile_scope": get(sample, "data_latency", "percentile_scope"),
               "variation_pairs": get(sample, "data_latency_variation", "count"),
               "mean_absolute_latency_change_s": get(sample, "data_latency_variation", "mean_s")} for sample in samples]
    write_csv(output / "series.csv", ["at_s", "from_s", "duration_s", "throughput_pps", "first_hop_admission_pps", "drops_per_s",
              "data_count", "mean_latency_s", "p95_latency_s", "percentile_scope", "variation_pairs", "mean_absolute_latency_change_s"], series)
    links, by_index = [], {}
    for sample in samples:
        for link in sample["links"]:
            row = {"at_s": sample["at_s"], "link_index": link["link_index"], "present": link.get("present", True),
                   "utilization_defined": link.get("utilization_defined"), "max_queue_fraction": link.get("max_queue_fraction"),
                   "queue_ab_or_shared": link.get("queue_ab_or_shared"), "queue_ba": link.get("queue_ba"),
                   **{f"utilization_{slot}": link["utilization"][slot] for slot in ("ab", "ba", "shared")}}
            links.append(row)
            values = [v for v in link["utilization"].values() if v is not None]
            by_index.setdefault(link["link_index"], []).append((sample["at_s"], max(values) if values else None))
    write_csv(output / "link-series.csv", ["at_s", "link_index", "present", "utilization_defined", "max_queue_fraction",
              "queue_ab_or_shared", "queue_ba", "utilization_ab", "utilization_ba", "utilization_shared"], links)
    groups = []
    for scope, key in (("flow", "flow_groups"), ("destination", "destination_node_groups")):
        for group in report[key]:
            groups.append({"scope": scope, "key": group["key"], "packets_delivered": group["packets_delivered"],
                "wire_bytes_delivered": group["wire_bytes_delivered"], "data_count": get(group, "data_latency", "count"),
                "mean_latency_s": get(group, "data_latency", "mean_s"), "p95_latency_s": get(group, "data_latency", "p95_s"),
                "percentile_scope": get(group, "data_latency", "percentile_scope"),
                "variation_pairs": get(group, "data_latency_variation", "count"),
                "mean_absolute_latency_change_s": get(group, "data_latency_variation", "mean_s")})
    write_csv(output / "groups.csv", ["scope", "key", "packets_delivered", "wire_bytes_delivered", "data_count",
              "mean_latency_s", "p95_latency_s", "percentile_scope", "variation_pairs", "mean_absolute_latency_change_s"], groups)
    bins = []
    for key in ("data_latency", "data_latency_variation"):
        histogram = get(report, key, "histogram")
        if histogram is None:
            continue
        edges = [0, *histogram["upper_edges_s"], None]
        bins.extend({"metric": key, "lower_s": edges[i], "upper_s": edges[i + 1], "upper_unbounded": edges[i + 1] is None,
                     "count": count} for i, count in enumerate(histogram["counts"]))
    write_csv(output / "histograms.csv", ["metric", "lower_s", "upper_s", "upper_unbounded", "count"], bins)
    recommendations = report["recommendations"]
    (output / "recommendations.md").write_text(
        "# Analytics diagnostic evidence\n\nHeuristic rules; inspect sampling interval, retention and model limitations.\n\n```json\n" +
        json.dumps(recommendations, indent=2, ensure_ascii=False, allow_nan=False) + "\n```\n", encoding="utf-8")
    e = lambda value: escape(str(value), quote=True)
    cards = [("Delivered network packets", report["packets_delivered_in_scope"]), ("DATA observations", report["data_latency"]["count"]),
             ("Retained DATA p95 (s)", report["data_latency"]["p95_s"]),
             ("Mean absolute latency change (s)", get(report, "data_latency_variation", "mean_s"))]
    card_html = "".join(f'<div><small>{e(label)}</small><strong>{fmt(value)}</strong></div>' for label, value in cards)
    charts = []
    for title, metric in (("Destination throughput (packets / virtual s)", "throughput_pps"),
                          ("Mean delivered DATA latency (s)", "mean_latency_s"),
                          ("Mean absolute DATA latency change within tracked flows (s)", "mean_absolute_latency_change_s"),
                          ("Path drops / virtual s", "drops_per_s")):
        charts.append(f'<section><h2>{e(title)}</h2>{plot([(row["at_s"], row[metric]) for row in series], title)}</section>')
    for index in sorted(by_index)[:16]:
        title = f"Link {index}: busiest directional/shared serialization fraction"
        charts.append(f'<section><h2>{e(title)}</h2>{plot(by_index[index], title, ceiling=1.0)}</section>')
    histograms = []
    for key, title in (("data_latency", "All observed DATA latency histogram"),
                       ("data_latency_variation", "Tracked-flow latency-change histogram")):
        selected = [row for row in bins if row["metric"] == key]
        if not selected:
            continue
        maximum = max(row["count"] for row in selected) or 1
        bars = "".join(f'<div class="hist-row"><span>[{fmt(row["lower_s"])}, {fmt(row["upper_s"]) if row["upper_s"] is not None else "∞"}) s</span>'
                       f'<span><span class="bar" style="width:{100 * row["count"] / maximum:.3f}%"></span></span><span>{row["count"]}</span></div>'
                       for row in selected)
        histograms.append(f'<section><h2>{e(title)}</h2>{bars}<small>Counts include all observations in the collection scope, including those outside percentile retention.</small></section>')
    evidence = "".join(f'<article><h3>{e(item["rule"])}</h3><p>{e(item["recommendation"])}</p>'
                       f'<pre>{e(json.dumps(item["evidence"], indent=2, allow_nan=False))}</pre><p class="muted">{e(item["limitations"])}</p></article>'
                       for item in recommendations) or '<p>No diagnostic rule triggered in the observed scope.</p>'
    document = f'''<!doctype html><html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<meta http-equiv="Content-Security-Policy" content="default-src 'none'; style-src 'unsafe-inline'; base-uri 'none'; form-action 'none'">
<title>KNS Analytics · {e(source)}</title><style>
*{{box-sizing:border-box}}body{{margin:0;background:#f2f6fa;color:#183146;font:15px/1.6 system-ui,sans-serif}}main{{max-width:1200px;margin:auto;padding:28px}}
h1{{font-size:30px;overflow-wrap:anywhere}}h2{{font-size:18px}}section,.cards div{{background:white;border:1px solid #d7e1ea;border-radius:10px;padding:18px;margin-bottom:16px}}
.cards,.charts{{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:16px}}strong{{display:block;font-size:28px}}small,.muted{{color:#516a7c}}svg{{width:100%;height:auto}}svg text{{font:11px system-ui;fill:#516a7c}}
pre{{white-space:pre-wrap;overflow-wrap:anywhere;background:#f5f7fa;padding:12px}}article{{border-bottom:1px solid #e4ebf1;padding:10px 0}}@media(max-width:700px){{main{{padding:12px}}.cards,.charts{{grid-template-columns:1fr}}}}
.hist-row{{display:grid;grid-template-columns:145px 1fr 45px;gap:8px;align-items:center;font-size:12px}}.bar{{display:block;height:12px;background:#21688e}}
</style></head><body><main><p>KNS · RUNTIME ANALYTICS</p><h1>{e(source)}</h1>
<p>Observed through {fmt(report["collection_until_s"])} virtual s. Simulation ended at {fmt(report.get("simulation_end_s"))} s.
Samples: {report["samples_seen"]}; discarded: {report["samples_discarded"]}.</p><div class="cards">{card_html}</div>
<section><h2>Scope and interpretation</h2><p>DATA percentile scope: {e(report["data_latency"]["percentile_scope"])}; retained {report["data_latency"]["retained_samples"]}, discarded {report["data_latency"]["discarded_samples"]}.</p>
<p>Tracked-flow latency-change pairs: {fmt(get(report, "data_latency_variation", "count"))}. A missing variation metric in older reports remains unavailable.</p>
<p>Link utilization charts share a fixed 0–1 axis. Collection completeness does not establish successful transport; inspect the benchmark case outcome.</p>
<p>Latency change pairs consecutive DATA deliveries within each tracked flow/direction, including retransmissions. It is not RFC interarrival jitter. Missing series remain gaps, not zeros. Small counts do not establish an SLA.</p>
<p>Charts show retained callbacks and at most the first 16 original links; all retained links are in link-series.csv. Queue entries include propagation. These observations are simulations, not physical network measurements or causal diagnoses.</p></section>
<div class="charts">{''.join(charts)}{''.join(histograms)}</div><section><h2>Diagnostic recommendations</h2>{evidence}</section>
<section><h2>Retention and coverage</h2><p>Ungrouped flow deliveries: {report["ungrouped_flow_deliveries"]}; ungrouped destination deliveries: {report["ungrouped_node_deliveries"]}.
Discarded serialization records: {report["reservation_records_discarded"]}; untracked-link transmissions: {report["untracked_link_transmissions"]}.</p>
<p>CSV exports preserve units, callback timestamps and undefined cells. Original JSON remains the source of full definitions and cumulative statistics.</p></section>
</main></body></html>'''
    (output / "analytics.html").write_text(document, encoding="utf-8")


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("report", type=Path)
    parser.add_argument("--output", type=Path, required=True, help="New directory for derived reports")
    args = parser.parse_args(argv)
    try:
        if args.report.stat().st_size > 64 * 1024 * 1024:
            raise ValueError("Analytics report exceeds the 64 MiB export limit")
        report = json.loads(args.report.read_text(encoding="utf-8"))
        if not isinstance(report, dict):
            raise ValueError("Analytics report must be an object")
        config = report.get("config")
        analytics_config.validate_report(report, config)
        output = args.output.resolve()
        output.mkdir(parents=True, exist_ok=False)
        write(report, output, args.report.name)
    except (OSError, ValueError, TypeError, KeyError, OverflowError, RecursionError) as error:
        print(f"[ERROR] {error}", file=sys.stderr)
        return 1
    print(f"Analytics report: {output / 'analytics.html'}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
