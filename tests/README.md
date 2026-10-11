# Additional find regression tests

Run from Ubuntu/WSL with the normal xv6 toolchain installed:

```sh
python3 tests/test-find-regressions.py --out /tmp/find-regression-results
```

Add `--kernel-tests` to run the full kernel `usertests` suite as well.

The script tests an exact copy of the working-tree `user/find.c` in a new
temporary directory. It does not clean the real repository, replace its
`fs.img`, modify the course grader, or change kernel parameters.

It runs the full course grader (19 tests, 156 points), then checks:

- Starting directories and files with trailing slashes, including `/`, `///`
  and paths beginning with `./`.
- Recursion and `-exec` at depths 10, 11, 20, 40, 50 and 61.
- All directory matches, output ordering, duplicates, 14-character names,
  and 20 independent branches.
- A valid 127-character path and graceful rejection of longer descendants.
- Allocation failure, failed/short directory reads, and 100 repeated traversals
  with no remaining heap allocations or lost file descriptors.
- `MAXARG` boundaries and failed command execution.

There are 49 shell-level regression cases and 10 internal checks (59 additional
checks in total). The complete logs and `summary.json` are saved under `--out`.

`find-regression-driver.c` includes the actual implementation with wrapper
functions for deterministic fault injection. The script builds this driver
only in its temporary copy; it is not part of normal `UPROGS`. Its argument
tests call the real find entry point directly to reach the `MAXARG` guard,
which the xv6 shell's smaller argument limit would otherwise prevent testing.
The normal `user/_find` is tested separately by both the grader and shell cases.

The deepest tree is created using `cd` and short relative names to avoid the
xv6 shell's 100-byte command buffer. Commands use absolute executable paths
after `cd`, because xv6 sh has no PATH lookup.
