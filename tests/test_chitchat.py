# Chit Chat layout + transcript tests (no Adobe SDK required).
# Mirrors src/core/MessageParse.cpp and ChatLayout.cpp.

import math
import unittest


def try_header(line, auto_id):
    stripped = line.strip()
    if "|" in stripped:
        parts = [p.strip() for p in stripped.split("|", 2)]
        if len(parts) != 3:
            return None
        try:
            mid = int(parts[0])
            sender = sender_token(parts[1])
        except ValueError:
            return None
        return {"id": mid, "sender": sender, "text": unescape_newlines(parts[2])}
    if ":" in stripped:
        prefix, body = stripped.split(":", 1)
        try:
            sender = sender_token(prefix)
        except ValueError:
            return None
        return {"id": auto_id, "sender": sender, "text": unescape_newlines(body.strip())}
    return None


def parse_transcript(text):
    messages = []
    used = set()
    auto_id = 1
    current = None
    for raw in text.splitlines():
        header = try_header(raw, auto_id) if raw.strip() and not raw.strip().startswith("#") else None
        if header is not None:
            if current is not None:
                current["text"] = current["text"].rstrip("\n")
                if not current["text"] or current["id"] in used or current["id"] < 1:
                    raise ValueError("bad message")
                used.add(current["id"])
                messages.append(current)
            current = header
            auto_id = max(auto_id, current["id"]) + 1
            continue
        if current is not None:
            current["text"] += "\n" + raw.rstrip("\r")
            continue
        if not raw.strip() or raw.strip().startswith("#"):
            continue
        raise ValueError("line format")
    if current is not None:
        current["text"] = current["text"].rstrip("\n")
        if not current["text"] or current["id"] in used or current["id"] < 1:
            raise ValueError("bad message")
        used.add(current["id"])
        messages.append(current)
    messages.sort(key=lambda m: m["id"])
    return messages


def unescape_newlines(s):
    out = []
    i = 0
    while i < len(s):
        if s[i] == "\\" and i + 1 < len(s):
            if s[i + 1] == "n":
                out.append("\n")
                i += 2
                continue
            if s[i + 1] == "\\":
                out.append("\\")
                i += 2
                continue
        out.append(s[i])
        i += 1
    return "".join(out)


def sender_token(token):
    t = token.strip().lower()
    if t in ("me", "you", "right", "self", "owner"):
        return "you"
    if t in ("them", "other", "left", "friend", "they"):
        return "them"
    raise ValueError("sender")


def smoothstep(t):
    t = max(0.0, min(1.0, t))
    return t * t * (3.0 - 2.0 * t)


def layout(bubbles, ages, layer_w=400, layer_h=800, spacing=10, margin_x=16, margin_y=16, fade=0.25, offset=24):
    n = len(bubbles)
    rest_y = [0.0] * n
    cursor = layer_h - margin_y
    for i in range(n - 1, -1, -1):
        cursor -= bubbles[i]["height"]
        rest_y[i] = cursor
        cursor -= spacing

    incoming_first = n
    min_age = 1e30
    if fade > 0:
        for i, age in enumerate(ages):
            if 0 <= age < fade and age < min_age - 1e-6:
                min_age = age
                incoming_first = i

    incoming_last = incoming_first - 1
    incoming_block = 0.0
    progress = 1.0
    if incoming_first < n:
        incoming_last = incoming_first
        while incoming_last + 1 < n and abs(ages[incoming_last + 1] - ages[incoming_first]) <= 1e-4:
            incoming_last += 1
        for i in range(incoming_first, incoming_last + 1):
            incoming_block += bubbles[i]["height"]
            if i < incoming_last:
                incoming_block += spacing
        incoming_block += spacing
        progress = smoothstep(min_age / fade)

    out = []
    for i, b in enumerate(bubbles):
        y = rest_y[i]
        opacity = 1.0
        incoming = incoming_first <= i <= incoming_last
        if incoming:
            y += offset * (1.0 - progress)
            opacity = progress
        elif incoming_first < n and i < incoming_first:
            y += incoming_block * (1.0 - progress)
        x = margin_x if b["sender"] == "them" else layer_w - margin_x - b["width"]
        out.append({"id": b["id"], "x": x, "y": y, "opacity": opacity, "sender": b["sender"]})
    return out


class ParseTests(unittest.TestCase):
    def test_me_them_prefixes(self):
        msgs = parse_transcript("me: Hello\nthem: Hi")
        self.assertEqual(msgs[0]["id"], 1)
        self.assertEqual(msgs[0]["sender"], "you")
        self.assertEqual(msgs[1]["id"], 2)
        self.assertEqual(msgs[1]["sender"], "them")

    def test_explicit_ids_and_skip(self):
        msgs = parse_transcript("1 | me | A\n3 | them | C")
        self.assertEqual([m["id"] for m in msgs], [1, 3])

    def test_comments_and_blanks(self):
        msgs = parse_transcript("# hi\n\nme: Only")
        self.assertEqual(len(msgs), 1)

    def test_escaped_line_breaks(self):
        msgs = parse_transcript("me: Hello\\nthere")
        self.assertEqual(msgs[0]["text"], "Hello\nthere")

    def test_literal_line_breaks(self):
        msgs = parse_transcript("me: Hello\nthere\nthem: Hi")
        self.assertEqual(msgs[0]["text"], "Hello\nthere")
        self.assertEqual(msgs[1]["text"], "Hi")
        self.assertEqual(msgs[1]["sender"], "them")

    def test_rejects_duplicate_ids(self):
        with self.assertRaises(ValueError):
            parse_transcript("1 | me | A\n1 | them | B")


class LayoutTests(unittest.TestCase):
    def test_you_right_them_left(self):
        bubbles = [
            {"id": 1, "sender": "them", "width": 80, "height": 20},
            {"id": 2, "sender": "you", "width": 90, "height": 20},
        ]
        items = layout(bubbles, [10, 10], fade=0.0, offset=0)
        self.assertEqual(items[0]["x"], 16)
        self.assertEqual(items[1]["x"], 400 - 16 - 90)

    def test_newest_at_bottom(self):
        bubbles = [
            {"id": 1, "sender": "you", "width": 50, "height": 20},
            {"id": 2, "sender": "you", "width": 50, "height": 30},
        ]
        items = layout(bubbles, [10, 10], fade=0.0, layer_h=200, margin_y=10, spacing=10)
        self.assertGreater(items[1]["y"], items[0]["y"])
        self.assertAlmostEqual(items[1]["y"], 200 - 10 - 30)

    def test_incoming_fades_and_pushes(self):
        bubbles = [
            {"id": 1, "sender": "you", "width": 50, "height": 20},
            {"id": 2, "sender": "them", "width": 50, "height": 20},
        ]
        start = layout(bubbles, [10, 0.0], fade=0.25, offset=24, layer_h=200, margin_y=10, spacing=10)
        done = layout(bubbles, [10, 10], fade=0.25, offset=24, layer_h=200, margin_y=10, spacing=10)
        self.assertAlmostEqual(start[1]["opacity"], 0.0)
        self.assertAlmostEqual(done[1]["opacity"], 1.0)
        self.assertGreater(start[1]["y"], done[1]["y"])
        self.assertGreater(start[0]["y"], done[0]["y"])

    def test_skipped_ids_animate_as_group(self):
        bubbles = [
            {"id": 1, "sender": "you", "width": 40, "height": 20},
            {"id": 2, "sender": "them", "width": 40, "height": 20},
            {"id": 3, "sender": "you", "width": 40, "height": 20},
        ]
        items = layout(bubbles, [0.0, 0.0, 0.0], fade=0.25, offset=10)
        self.assertTrue(all(math.isclose(i["opacity"], 0.0) for i in items))


class AppearTests(unittest.TestCase):
    def test_hold_and_skip(self):
        samples = [(0.0, 0), (1.0, 1), (2.0, 5)]

        def appear(mid):
            for t, v in samples:
                if v >= mid:
                    return t
            return -1

        self.assertEqual(appear(1), 1.0)
        self.assertEqual(appear(2), 2.0)
        self.assertEqual(appear(5), 2.0)
        self.assertEqual(appear(6), -1)


if __name__ == "__main__":
    unittest.main()
