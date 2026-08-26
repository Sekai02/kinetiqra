# Contributing to kinetiqra

Thanks for taking an interest. This document covers what the repository expects
of a pull request.

Most of the rules below are enforced by CI, so a pull request that ignores them
turns red rather than being merged and corrected afterwards. Reading this first
is faster than discovering them one failed check at a time.

Reference documents:

- [docs/PREREQUISITES.md](docs/PREREQUISITES.md): what to install, and vcpkg
- [docs/PROJECT_LAYOUT.md](docs/PROJECT_LAYOUT.md): modules and their dependencies
- [docs/INVARIANTS.md](docs/INVARIANTS.md): design rules that must not be broken
- [docs/CODESTYLE.md](docs/CODESTYLE.md): formatting

## Finding something to work on

Issues labelled `good first issue` are self-contained and do not require
understanding the whole engine. `help wanted` marks work that is open to anyone.

If you want to work on something already filed, say so on the issue first, so
that two people do not build the same thing twice.

## Branches

The repository follows gitflow.

- `dev` is the integration branch. **Branch from `dev` and target `dev`.**
- `main` holds releases only, and is reachable exclusively from `release/X.Y.Z`
  or `hotfix/<slug>`.

Branch names are validated:

```
<type>/<kebab-case-slug>

type   feat | fix | refactor | perf | docs | build | test | chore
slug   lower case letters, digits and single hyphens
```

So `feat/gltf-importer` is fine; `feature/gltf-importer`, `feat/GltfImporter`
and `feat/gltf_importer` are rejected.

## Commits

Commit subjects use the same vocabulary, in the imperative and in lower case:

```
feat: add the gltf accessor reader
fix: correct the inverse bind matrices
```

Keep the subject to a single line.

## Pull requests

### Title

The title is validated and drives the labels, so it is not free text:

```
<type>: <summary>
<type>(<area>): <summary>
```

The summary must start in lower case and must not end with a full stop.

The area is optional and, when given, must be one of the eight module names:

```
core   math   geom   scene   anim   io   render   app
```

[PROJECT_LAYOUT.md](docs/PROJECT_LAYOUT.md) describes what each one covers.

```
feat: add the gltf accessor reader        ok
feat(io): add the gltf accessor reader    ok
Add the gltf accessor reader              rejected: no type
feat: Add the gltf accessor reader        rejected: upper case
feat: add the gltf accessor reader.       rejected: trailing full stop
```

### Labels

Labels are applied automatically and do not need to be set by hand.

- `type:` comes from the title prefix.
- `area:` comes from the files you changed. Paths under `src/<module>` or
  `tests/<module>` name a module, and several can apply at once. The title scope
  is used only when the changed files touch no module at all, such as tooling or
  documentation changes.

### Assignee and review

Every pull request needs an assignee, and you are assigned automatically when
you open one. That can fail for contributors without repository access, and if
it does, a maintainer will assign you.

Every pull request also needs an approving review from a code owner before it
can merge.

### Before pushing

Build and run the tests with the `debug` preset, where warnings are errors, and
format what you changed as described in [CODESTYLE.md](docs/CODESTYLE.md).

## Opening an issue

Issues are the way to raise anything before code exists: a defect, a feature, a
question, or a decision that needs settling. Label what you open if you can; if
you are unsure, leave it and a maintainer will triage it.

### Reporting a bug

Label it `bug`. Describe what you did, what happened, and what you expected.

For build failures, include your OS, compiler version, CMake version, and the
relevant output. For anything visual, a screenshot saves a great deal of
description.

### Requesting a feature

Label it `type: feat`, and add the relevant `area:` label if you know which
module it belongs to.

Describe **the problem before the solution**. What are you trying to do, and
what stops you from doing it today? A feature request that only names a
solution is hard to evaluate, because the reasoning that led there is missing,
and that reasoning is often what points at a better answer.

Say whether you intend to implement it yourself. That changes what happens next:
a proposal you plan to build gets discussed in more detail up front, whereas one
you are handing over gets weighed against everything else on the list.

### Proposing a design change

Something that alters the architecture, such as a file format, a module boundary
or how undo works, belongs in an issue labelled `needs-design` before any code
is written. These are the changes that are expensive to reverse, so they are
worth arguing about while they are still text.

### Asking a question

Label it `question`. Questions about how something is meant to work are useful
even when nothing is wrong: they usually mean the documentation is missing
something, and the answer often ends up in `docs/`.
