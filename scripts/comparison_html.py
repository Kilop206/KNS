"""Self-contained comparison report: escaped HTML, inline CSS and accessible SVG."""
from html import escape
from pathlib import Path


LABELS = {"delivery_rate": "Delivery counter ratio", "loss_rate": "Loss counter ratio",
          "throughput_pps": "Throughput (packets/s)", "avg_latency_s": "Delivered DATA latency (s)",
          "simulation_duration_s": "Virtual duration (s)", "queue_overflow_drops": "Queue overflow drops",
          "red_early_drops": "RED early drops"}


def number(value) -> str:
    return "N/A" if value is None else f"{value:.6g}"


def chart(values: list[float], summary: dict) -> str:
    interval = summary["conditional_ci95"]
    extent = max([abs(value) for value in values] +
                 ([abs(interval["low"]), abs(interval["high"])] if interval else []) + [1e-300])
    def x(value):
        return 160 + (value / extent) * 130
    dots = "".join(f'<circle cx="{x(value):.2f}" cy="30" r="3" fill="#276b9c" opacity="0.55"/>' for value in values)
    whisker = (f'<line x1="{x(interval["low"]):.2f}" x2="{x(interval["high"]):.2f}" y1="20" y2="20" stroke="#735321" stroke-width="3"/>'
               if interval else "")
    mean = summary["mean_delta"]
    marker = f'<circle cx="{x(mean):.2f}" cy="20" r="4" fill="#152b42"/>' if mean is not None else ""
    title = escape(f"Paired deltas: {len(values)} samples; mean {number(mean)}; conditional CI {interval or 'unavailable'}")
    return (f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 320 54" role="img" aria-label="{title}">'
            f'<title>{title}</title><line x1="160" x2="160" y1="4" y2="39" stroke="#a0acb7"/>'
            f'{whisker}{dots}{marker}<text x="160" y="51" text-anchor="middle" font-size="10" fill="#526478">0</text></svg>')


def write(comparison: dict, path: Path, metrics: tuple) -> None:
    e = lambda value: escape(str(value), quote=True)
    rows = []
    keys = ("experiment", "routing_metric", "packet_size", "congestion_control")
    grouped = {}
    for pair in comparison["pairs"]:
        grouped.setdefault(tuple(pair[key] for key in keys), []).append(pair)
    for summary in comparison["summaries"]:
        group = grouped.get(tuple(summary[key] for key in keys), [])
        for field in metrics:
            m = summary["metrics"][field]
            values = [pair[f"{field}_delta"] for pair in group if pair[f"{field}_delta"] is not None]
            interval = m["conditional_ci95"]
            ci = f'[{number(interval["low"])}, {number(interval["high"])}]' if interval else "N/A"
            percent = number(m["mean_delta_percent"]) + "%" if m["mean_delta_percent"] is not None else "N/A"
            effect = m["direction"]
            css = effect if effect in {"improvement", "regression", "unchanged"} else "context"
            rows.append(f'<tr><th scope="row">{e(summary["experiment"])}<small>{e(summary["routing_metric"])} · {summary["packet_size"]} bytes · {e(summary["congestion_control"])}</small></th>'
                f'<td>{e(LABELS[field])}</td><td>{number(m["baseline_mean"])}</td><td>{number(m["experiment_mean"])}</td>'
                f'<td>{number(m["mean_delta"])}<small>{percent} · n={m["relative_sample_count"]} relative pairs</small></td>'
                f'<td><span class="{css}">{e(effect.replace("_", " "))}</span></td>'
                f'<td>{m["sample_count"]}<small>SD {number(m["sample_stddev_delta"])}</small></td>'
                f'<td>{ci}<small>{e(m["ci_status"].replace("_", " "))}</small></td>'
                f'<td class="plot">{chart(values, m)}</td></tr>')
    unavailable = [pair for pair in comparison["pairs"] if pair["status"] != "matched"]
    errors = "".join(f'<li>{e(pair["experiment"])} · seed {pair["seed"]}: reference {e(pair["baseline_status"])} / candidate {e(pair["experiment_status"])}</li>'
                     for pair in unavailable)
    limitations = "".join(f'<li>{e(item)}</li>' for item in comparison["methodology"]["limitations"])
    origins = comparison.get("sources")
    source_text = f'<details><summary>Recorded comparison sources</summary><pre>{e(origins)}</pre></details>' if origins else ""
    document = f'''<!doctype html>
<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<meta http-equiv="Content-Security-Policy" content="default-src 'none'; style-src 'unsafe-inline'; img-src data:; base-uri 'none'; form-action 'none'">
<title>KNS Compare · {e(comparison["profile"])}</title>
<style>
*{{box-sizing:border-box}}body{{margin:0;background:#f1f5f9;color:#152b42;font:15px/1.55 system-ui,sans-serif}}
main{{max-width:1600px;margin:auto;padding:36px 24px}}h1{{font-size:32px;margin:0}}h2{{font-size:20px}}
.tag{{color:#286289;font-weight:700;letter-spacing:.1em}}.cards{{display:flex;gap:16px;flex-wrap:wrap;margin:24px 0}}
.cards div,section{{background:white;border:1px solid #dbe3eb;border-radius:10px;padding:20px}}.cards strong{{font-size:28px;display:block}}
section{{margin-bottom:20px}}.table{{overflow:auto;padding:0}}table{{border-collapse:collapse;width:100%;font-size:13px}}
th,td{{padding:14px 12px;text-align:left;border-bottom:1px solid #e0e7ee;vertical-align:top}}thead{{background:#e8eff6}}
tbody th{{min-width:160px}}small{{display:block;color:#526478;font-weight:400;margin-top:5px}}.plot{{min-width:230px}}svg{{width:230px;height:auto}}
.improvement{{color:#176345}}.regression{{color:#a22735}}.unchanged,.context{{color:#526478}}pre{{white-space:pre-wrap;overflow-wrap:anywhere}}
footer{{color:#526478;font-size:13px}}@media(max-width:600px){{main{{padding:18px 10px}}h1{{font-size:25px}}}}
</style></head><body><main><div class="tag">KNS COMPARE</div><h1>{e(comparison["profile"])}</h1>
<p>Reference: <strong>{e(comparison["baseline"])}</strong>. Deltas = candidate − reference.</p>
<div class="cards"><div><strong>{comparison["matched_pairs"]}/{comparison["expected_pairs"]}</strong>matched workload pairs</div>
<div><strong>{comparison["unavailable_pairs"]}</strong>unavailable pairs</div><div><strong>{len(comparison["summaries"])}</strong>workload groups</div></div>
<section><h2>Interpretation</h2><p>Directions describe observed changes, not significance or causality. Conditional 95% intervals assume independent, normally distributed paired seed deltas. They are unadjusted for multiple comparisons. Zero variation does not establish zero uncertainty.</p>
<p>Charts: blue dots are individual seed deltas, dark dots are means, brown lines are conditional intervals; each chart uses its own scale around zero.</p></section>
<section class="table"><table><thead><tr><th>Variant / workload</th><th>Metric</th><th>Reference mean</th><th>Candidate mean</th><th>Paired delta</th><th>Direction</th><th>Pairs / variation</th><th>Conditional CI95</th><th>Delta distribution</th></tr></thead>
<tbody>{''.join(rows) or '<tr><td colspan="9">No comparable variants.</td></tr>'}</tbody></table></section>
<section><h2>Unavailable outcomes</h2>{'<ul>'+errors+'</ul>' if errors else '<p>No missing or failed pairs.</p>'}</section>
<section><h2>Method and limitations</h2><ul>{limitations}</ul><p>{e(comparison["methodology"]["ci"])}</p>
<p>Critical values: <a href="https://www.itl.nist.gov/div898/handbook/eda/section3/eda3672.htm">NIST Student-t table</a>.</p>{source_text}</section>
<footer>Simulation results · comparison schema {e(comparison["schema_version"])} · local HTML/CSS/SVG; no external assets or services.</footer>
</main></body></html>'''
    path.write_text(document, encoding="utf-8")
