# Code style

## Formatting

`.clang-format` at the repository root defines the style: Google-based, 4-space
indent, 100 columns, with `<kinetiqra/...>` includes in their own group. Run it
over what you changed before pushing:

```sh
clang-format -i $(git diff --name-only --diff-filter=ACM origin/dev... | grep -E '\.(cpp|hpp)$')
```

Adding `--dry-run --Werror` checks without rewriting.

Style is how the code looks. The rules about how it is built are in
[INVARIANTS.md](INVARIANTS.md).
