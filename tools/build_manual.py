#!/usr/bin/env python3
"""Builds docs/manual/manual.html: the whole user manual as one self-contained page (images embedded).

The plugin embeds this file and opens it from the ? button, so it is committed to the repo and CI checks it is up to date.

    pip install markdown==3.10.3
    python3 tools/manual_reference.py     # only after parameters changed (needs a fresh docs/parameters.json)
    python3 tools/build_manual.py
"""
import base64, html, os, re, sys

import markdown

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, ".."))
MAN = os.path.join(ROOT, "docs", "manual")
REPO = "https://github.com/ljw46/EmberlineSpectralis/blob/main/"
CHAPTERS = ["README", "01-getting-started", "02-oscillators", "03-filters-and-warmth", "04-modulation", "05-effects",
            "06-arp-and-stutter", "07-presets-and-sharing", "08-wavetables", "09-recipes", "10-troubleshooting", "reference"]

# ---- version, from the single source of truth in CMakeLists.txt
cmake = open(os.path.join(ROOT, "CMakeLists.txt")).read()
number = re.search(r"project\(\w+ VERSION ([\d.]+)", cmake).group(1)
label = re.search(r'set\(EMBER_VERSION_LABEL "([^"]*)"', cmake).group(1)
version = "v" + number + (" " + label if label else "")

# keep the human-readable README line in step
readme_path = os.path.join(MAN, "README.md")
readme = open(readme_path).read()
readme2 = re.sub(r"_Version: .*?(?=\. Things may move)", f"_Version: {version[1:]}", readme)   # idempotent: only the text before ". Things may move"
if readme2 != readme:
    open(readme_path, "w").write(readme2)

def slug(text):
    return re.sub(r"[^a-z0-9]+", "-", text.lower()).strip("-")

def embed_image(match):
    path = os.path.join(MAN, match.group(1))
    data = base64.b64encode(open(path, "rb").read()).decode()
    return f'src="data:image/png;base64,{data}"'

sections, toc = [], []
for stem in CHAPTERS:
    text = open(os.path.join(MAN, stem + ".md"), encoding="utf-8").read()
    text = re.sub(r"\]\(((?:\d\d-[a-z-]+|reference)\.md)\)", lambda m: "](#ch-" + m.group(1)[:-3] + ")", text)
    text = re.sub(r"\]\((?:\.\./)+([A-Za-z0-9_/.-]+\.md)\)", lambda m: "](" + REPO + ("CONTRIBUTING.md" if "CONTRIBUTING" in m.group(1) else "docs/" + m.group(1)) + ")", text)
    md = markdown.Markdown(extensions=["tables", "fenced_code", "toc"],
                           extension_configs={"toc": {"slugify": lambda v, s, st=stem: st + "-" + slug(v)}})
    body = md.convert(text)
    body = re.sub(r'src="(img/[^"]+)"', embed_image, body)
    body = body.replace("<table>", '<div class="table"><table>').replace("</table>", "</table></div>")
    body = re.sub(r"<a href=\"(https?://[^\"]+)\"", r'<a target="_blank" rel="noopener" href="\1"', body)
    title = re.search(r"<h1[^>]*>(.*?)</h1>", body).group(1)
    title = "Welcome" if stem == "README" else re.sub(r"<[^>]+>", "", title)
    subs = [t for t in (md.toc_tokens[0]["children"] if md.toc_tokens else [])]
    toc.append((f"ch-{stem}", title, [(t["id"], t["name"]) for t in subs]))
    sections.append(f'<section id="ch-{stem}">\n{body}\n</section>')

nav = []
for sid, title, subs in toc:
    nav.append(f'<a class="top" href="#{sid}">{html.escape(title)}</a>')
    for i, name in subs:
        nav.append(f'<a class="sub" href="#{i}">{html.escape(name)}</a>')

CSS = """
:root{--bg:#0b0e14;--panel:#121824;--fill:#0e131d;--text:#d8e0ec;--dim:#8d9bb0;--cyan:#00f2fe;--coral:#ff5e36;--purple:#a100ff;--line:#243049}
*{box-sizing:border-box}html{scroll-behavior:smooth;scroll-padding-top:16px}
body{margin:0;background:var(--bg);color:var(--text);font:17px/1.65 system-ui,-apple-system,"Segoe UI",Roboto,sans-serif}
.wrap{display:grid;grid-template-columns:270px minmax(0,1fr);min-height:100vh}
nav{position:sticky;top:0;height:100vh;overflow:auto;padding:22px 14px 40px 20px;background:var(--panel);border-right:1px solid var(--line)}
nav .brand{font-weight:800;letter-spacing:.14em;font-size:15px;background:linear-gradient(90deg,var(--cyan),var(--coral));-webkit-background-clip:text;background-clip:text;color:transparent}
nav .ver{color:var(--dim);font-size:13px;margin:2px 0 14px}
nav a{display:block;color:var(--text);text-decoration:none;border-radius:6px;padding:4px 8px}
nav a.top{margin-top:8px;font-weight:600}nav a.sub{color:var(--dim);font-size:14px;padding:2px 8px 2px 20px}
nav a:hover{background:var(--fill);color:var(--cyan)}
main{padding:36px clamp(18px,5vw,64px) 80px;max-width:940px}
section{padding-bottom:36px;margin-bottom:36px;border-bottom:1px solid var(--line)}
h1{font-size:2rem;line-height:1.2;margin:.2em 0 .6em;background:linear-gradient(90deg,var(--cyan),var(--purple));-webkit-background-clip:text;background-clip:text;color:transparent}
h2{font-size:1.35rem;margin:1.8em 0 .5em;color:var(--cyan)}h3{font-size:1.1rem;margin:1.4em 0 .4em;color:var(--coral)}
a{color:var(--cyan)}p,li{max-width:70ch}
code{background:var(--fill);border:1px solid var(--line);border-radius:5px;padding:1px 6px;font-size:.88em}
pre{background:var(--fill);border:1px solid var(--line);border-radius:8px;padding:12px 14px;overflow:auto}pre code{border:0;padding:0}
img{max-width:100%;height:auto;border-radius:10px;border:1px solid var(--line);margin:8px 0}
.table{overflow-x:auto;margin:12px 0}table{border-collapse:collapse;min-width:60%;font-size:.95em}
th,td{border:1px solid var(--line);padding:6px 10px;text-align:left;vertical-align:top}th{background:var(--panel);color:var(--cyan)}
tr:nth-child(even) td{background:rgba(255,255,255,.02)}
footer{color:var(--dim);font-size:14px}
@media(max-width:820px){.wrap{display:block}nav{position:static;height:auto;border-right:0;border-bottom:1px solid var(--line)}nav a.sub{display:none}}
@media print{body{background:#fff;color:#111;font-size:11pt}nav{display:none}.wrap{display:block}h1,h2,h3{color:#111;background:none;-webkit-text-fill-color:#111}a{color:#06c}
code,pre,th,td,img{border-color:#bbb}code,pre,th{background:#f3f3f3;color:#111}section{page-break-before:always;border:0}}
"""

page = f"""<!doctype html>
<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Emberline Spectralis - Manual</title>
<style>{CSS}</style></head><body>
<div class="wrap">
<nav><div class="brand">EMBERLINE SPECTRALIS</div><div class="ver">Manual &middot; {html.escape(version)}</div>
{chr(10).join(nav)}</nav>
<main>
{chr(10).join(sections)}
<footer>Emberline Spectralis {html.escape(version)} &middot; presets by <a target="_blank" rel="noopener" href="https://www.instagram.com/emberlinesky/">EmberlineSky</a>
&middot; enjoying it? <a target="_blank" rel="noopener" href="https://buymeacoffee.com/emberlinesky">Buy me a coffee</a></footer>
</main></div></body></html>
"""
open(os.path.join(MAN, "manual.html"), "w", encoding="utf-8", newline="\n").write(page)
print(f"wrote docs/manual/manual.html ({len(page) // 1024} KB, {version})")
