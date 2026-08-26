# AGENTS.md

Rules for AI agents working in this repository. They are requirements, not
suggestions.

## Respect the invariants

[docs/INVARIANTS.md](docs/INVARIANTS.md) defines the design rules the engine is
built on. Do not break them, and do not work around them.

If a task appears to require breaking one, stop and say so instead of choosing
for yourself. That is a design decision, and it belongs in an issue labelled
`needs-design` before any code is written.

## A change is not done until it builds, passes and is formatted

Before reporting a change as finished:

```sh
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

**Every test must pass.** A failing test is not an acceptable end state, and
neither is a test that was disabled, skipped or weakened to make the suite go
green. If a test cannot be made to pass, report that plainly rather than
presenting the change as complete.

The `debug` preset treats warnings as errors, so the build failing on a warning
is the build working as intended.

Format what you changed, as described in [docs/CODESTYLE.md](docs/CODESTYLE.md):

```sh
clang-format -i $(git diff --name-only --diff-filter=ACM origin/dev... | grep -E '\.(cpp|hpp)$')
```

## Branches, commits and pull requests

Follow [CONTRIBUTING.md](CONTRIBUTING.md) exactly. It defines the branch naming
rules, the commit subject format and the pull request title format, and all
three are validated by CI, so anything else is rejected rather than corrected
later.

A pull request description is **one short paragraph**. No headings, no bullet
lists, no summary of the diff. Say what the change does and why, and stop.

## Everything is written in English

Code, comments, identifiers, commit messages, branch names, pull request titles
and descriptions, issues, and documentation. Without exception, and regardless
of the language the request was made in.
