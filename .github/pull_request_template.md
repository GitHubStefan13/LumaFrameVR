# What this changes

<!-- One paragraph. What does it do, and why does it need doing? -->

## How it was checked

<!-- The commands you ran, and what they printed. "Tests pass" on its own is not enough:
     say which command, and what it said. -->

- [ ] `cmake --build build --parallel 2` gives no errors
- [ ] `ctest --test-dir build --output-on-failure` passes
- [ ] `python3 -B -m unittest discover -s tests -v` passes
- [ ] `clang-format -i` run over the files touched

## Guidelines

- [ ] One thing per file and per pull request; a file that has become two things is split
- [ ] No generated artifacts, archives or build trees committed
- [ ] No new dependencies without a note here explaining why
- [ ] Comments only where the code cannot speak for itself
- [ ] Copyright, attribution and licence notices preserved; new third-party material noted in
      `licenses/`
- [ ] Documentation updated where behaviour changed

## Anything unverified

<!-- What you could not check, and what would prove it. A device-facing change cannot be proven by
     the host suite, and saying so is better than implying otherwise. -->
