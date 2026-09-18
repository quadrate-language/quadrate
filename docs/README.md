# Documentation

Quadrate documentation built with MkDocs.

## Setup

```bash
cd docs
python -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
```

## Development

```bash
mkdocs serve    # http://127.0.0.1:8000
```

## Build

```bash
mkdocs build --strict    # Output in site/
```

`--strict` is what CI runs: it turns MkDocs warnings -- a broken internal link, a
nav entry pointing at a file that does not exist -- into a failed build. `make docs`
from the repository root does the same.

## Deployment

`.github/workflows/docs.yml` builds the site on every push to `master` that touches
`docs/`, and deploys it to GitHub Pages. Pull requests get the build without the
deploy, so a broken site is caught before it merges.

The workflow needs two things set once in the repository settings, and cannot set
them itself:

1. **Settings -> Pages -> Build and deployment -> Source: GitHub Actions.**
   With the default "Deploy from a branch" the workflow uploads an artifact that
   nothing publishes.
2. **Settings -> Pages -> Custom domain: `quad.r8.rs`**, plus a DNS CNAME record
   for `quad.r8.rs` pointing at `quadrate-language.github.io`. Until that is done
   the site is reachable at `https://quadrate-language.github.io/quadrate/` --
   every internal link is relative, so it works at either address.

`docs/CNAME` carries the domain into the built site so the setting survives a
redeploy. To publish under a subdomain instead, change that file and `site_url`
in `mkdocs.yml` together.

**Before pointing the apex at Pages**: `quad.r8.rs/play/` is the playground, and
that is `tools/playground`, a Go server that runs submitted code in a Docker
sandbox. Pages serves static files only, so whatever host answers `/play/` today
still has to answer it -- either keep the apex where it is and give the docs a
subdomain, or move the playground to one.

## Structure

- `docs/` - Markdown source
- `pygments-quadrate/` - Syntax highlighter
- `gen_docs.sh` - Regenerates `docs/stdlib/*.md` and `api/*.json` from the `///`
  comments in the stdlib `.qd` sources and from `reference.def`. Run it after
  changing a documented signature; the generated files are checked in.
