#!/usr/bin/env python3
"""Post a release's notes to a Discord webhook (release job, .github/workflows/build.yml).

Usage: discord_release.py NOTES_FILE TITLE URL   (webhook URL in $DISCORD_WEBHOOK)
Discord messages hold 2000 characters: the notes go out in as many messages as needed, split
between lines (a single overlong line between words)."""
import json, os, sys, time, urllib.request

LIMIT = 1900


def chunks(text):
    lines = []
    for line in text.splitlines():                    # a single overlong line: split between words
        while len(line) > LIMIT:
            cut = line.rfind(" ", 0, LIMIT)
            cut = cut if cut > 0 else LIMIT
            lines.append(line[:cut])
            line = line[cut:].lstrip()
        lines.append(line)
    out, cur = [], ""
    for line in lines:
        if cur and len(cur) + 1 + len(line) > LIMIT:
            out.append(cur)
            cur = line
        else:
            cur = cur + "\n" + line if cur else line
    out.append(cur)
    return [c for c in out if c.strip()]


def post(hook, content):
    data = json.dumps({"content": content, "allowed_mentions": {"parse": []}, "flags": 4}).encode()  # 4: no link previews
    req = urllib.request.Request(hook, data=data, headers={"Content-Type": "application/json", "User-Agent": "recvx-release"})
    for attempt in range(5):
        try:
            urllib.request.urlopen(req, timeout=30).read()
            return
        except urllib.error.HTTPError as e:
            if e.code != 429:
                raise
            time.sleep(float(e.headers.get("Retry-After", "2")))   # rate limited: wait and retry
    raise SystemExit("discord: rate limited")


def main():
    hook = os.environ.get("DISCORD_WEBHOOK", "")
    if not hook:
        print("discord: no DISCORD_WEBHOOK, nothing posted")
        return
    notes_file, title, url = sys.argv[1:4]
    notes = open(notes_file, encoding="utf-8").read()
    notes = "\n".join(l for l in notes.splitlines() if l.strip() not in ("> [!NOTE]", "[!NOTE]"))   # GitHub alert marker
    parts = chunks(f"## {title}\n{url}\n\n{notes}")
    for i, part in enumerate(parts):
        post(hook, part)
        time.sleep(1)
    print(f"discord: posted {len(parts)} message(s)")


if __name__ == "__main__":
    main()
