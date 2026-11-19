"""Authored UI fixtures; native integration tests supply actual model verdicts."""
from html.parser import HTMLParser
import unittest

import validation_html


class Document(HTMLParser):
    def __init__(self):
        super().__init__()
        self.tags, self.links, self.words, self.elements = [], [], [], []

    def handle_starttag(self, tag, attrs):
        self.tags.append(tag)
        self.elements.append((tag, dict(attrs)))
        for key, value in attrs:
            if key in ("href", "src"):
                self.links.append(value)

    def handle_data(self, data):
        self.words.append(data)


def verdict():
    return {"schema_version": "1.1", "passed": False, "assertion_count": 1, "failed_assertions": 1,
        "model": "Authored renderer fixture <script>alert(1)</script>",
        "configuration_sha256": "a" * 64, "limitations": "Model results only <img src=x onerror=alert(2)>",
        "checks": [{"name": "delivery-baseline", "metric": "delivery_ratio", "flow_id": None,
            "operator": ">=", "expected": 0.9593251533742331, "observed": 0.7423312883435583,
            "unit": "fraction", "scope": "generated", "evidence": {"numerator": 242, "denominator": 326},
            "passed": False, "reason": "threshold violated", "baseline": {"observed": 0.9693251533742331,
                "offset": -0.01, "unit": "fraction", "scope": "generated", "evidence": {"numerator": 316, "denominator": 326}}}]}


class ValidationHtmlTests(unittest.TestCase):
    def test_values_are_not_shortened_and_untrusted_text_cannot_become_markup(self):
        html = validation_html.render(verdict())
        page = Document(); page.feed(html)
        self.assertEqual(set(page.links), {"validation.json", "validation.xml", "validation.md"})
        self.assertFalse(set(page.tags) & {"script", "img", "iframe", "link"})
        self.assertIn("0.7423312883435583", "".join(page.words))
        self.assertIn("0.9593251533742331", "".join(page.words))
        self.assertIn("0.9693251533742331", "".join(page.words))
        self.assertIn("Assertions failed", "".join(page.words))
        self.assertIn("&lt;script&gt;", html)
        self.assertIn("default-src 'none'", html)
        region = next(attrs for tag, attrs in page.elements if attrs.get("aria-label") == "Assertion results table")
        self.assertEqual(region.get("tabindex"), "0")
        self.assertEqual(validation_html.render(verdict()), html)

    def test_unavailable_values_remain_unavailable_and_cannot_show_a_pass(self):
        data = verdict(); data["checks"][0]["observed"] = None
        data["checks"][0]["expected"] = None; data["checks"][0]["baseline"]["observed"] = None
        data["checks"][0]["reason"] = "baseline metric unavailable or denominator zero"
        page = Document(); page.feed(validation_html.render(data))
        self.assertGreaterEqual("".join(page.words).count("N/A"), 3)
        self.assertIn("baseline metric unavailable", "".join(page.words))
        data["passed"] = True
        with self.assertRaisesRegex(ValueError, "inconsistent"):
            validation_html.render(data)
        data = verdict(); data["checks"] = []
        with self.assertRaisesRegex(ValueError, "nonempty"):
            validation_html.render(data)

    def test_large_model_text_has_an_explicit_bounded_preview(self):
        data = verdict(); data["model"] = "x" * 100000
        html = validation_html.render(data)
        self.assertIn("truncated; full value in JSON", html)
        self.assertNotIn("x" * 4097, html)
        self.assertLess(len(html), 20000)


if __name__ == "__main__":
    unittest.main()
