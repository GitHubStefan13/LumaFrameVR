# C++ guidelines

These are the rules this codebase actually follows. They exist so that a diff reads like an
explanation on its own, and so that a reviewer can tell at a glance whether a change belongs.

## Shape of the code

- C++20, one application, built by the CMake project in the root. No exceptions to that: the
  player is not a library, and there is no second executable besides the tests and harnesses.
- File length is not a rule. **SOLID decides**: a file holds one cohesive thing, and when it starts
  holding two, it is split. The check only fails past 360 lines, as a signal that a file has stopped
  being one thing rather than as a limit to design against. Formatting is `clang-format` with
  `.clang-format` at the repository root: the Google base, four space indentation and a 200 column
  limit, which this code effectively never reaches, so statements stay on one line. When it grows past that, split the responsibility rather
  than the line count. The single-file installer is the one documented exception, because it has
  to be one file to be self-extracting.
- Every header uses `#pragma once`.
- Names, in the order a reader meets them:
  - types `PascalCase` (`ControllerMesh`, `PersistentDirectory`)
  - functions and variables `snake_case` (`application_help`, `image_probe`, `saved_bytes`)
  - class members get a trailing underscore (`descriptor_`, `alignment_drag_`)
  - namespaces lowercase, nested as `lumaframevr::application`
  - files `snake_case.hpp` / `snake_case.cpp`
- Include the file's own header first in a `.cpp`, then the standard and library headers.

## Comments

Comments are allowed, and they stay rare. Start from a name that explains itself, because the
name travels with the code and the comment rots: if a line needs a sentence under it to be
understood, rename the thing first. A name like `first_frame_budget_us` or `flushing_at_end`
says what the sentence was going to say, and it keeps saying it in five years.

Write a comment when the code genuinely cannot say it: arithmetic that took an evening, a
decoder that lies about what it returned, a lock ordering that deadlocks if someone reorders
two lines, a lifetime rule a reader could break without noticing. One or two lines, saying only
what is invisible in the code. Not a narration of the next line, not an essay, not a restatement
of the function name.

Rules that keep it from turning into noise:

- Never where it could hide a bug, a unit, a failure path or a boundary check.
- Never in user-facing text. Everything the user reads comes from `lang/*.lang`, and
  `cpp/tests/test_no_hardcoded_ui_text.py` exists to catch you.
- One per file at most, and only in code that is already hard to read.
- No `TODO`, no `FIXME`, no commented-out code, whatever the excuse.
- If a reviewer says the joke makes the code harder to follow, the joke goes. Being funny is not
  a defence, and clarity always wins.

## Errors and ownership

- Bad input throws `std::invalid_argument`. An operational failure, a failed `open`, a decoder
  that refuses to start, throws `std::runtime_error`. A broken invariant inside the program
  throws `std::logic_error`.
- Never `assert` for anything that matters. It disappears in a release build, which is exactly
  when you need it.
- Catch at a boundary, report, and carry on if you can. Never swallow an error silently, and
  never let one escape into the render loop to take the session down.
- No owning raw pointers, no manual `new` or `delete`. Use `std::unique_ptr`,
  `std::optional` and `std::filesystem::path`, and let RAII do the cleanup.
- Move-only for anything that owns a device handle, a decoder or a frame. If it can be copied by
  accident, it can be freed twice by accident.

## Threads

- Nothing expensive runs on the render thread: decoding, directory scanning, thumbnail
  generation and controller-model loading all live on their own workers.
- Every queue and cache has a bound. A cache without an eviction rule is a leak with extra steps.
- The render loop reads immutable snapshots. Publish state, do not share mutable state.
- If a worker holds a lock, the waiter sets its own stop flag under the same lock. Half the
  teardown hangs in this kind of code come from a wakeup that was lost in a race with a
  destructor.

## Tests

- Write the acceptance test first: `cpp/tests/<topic>_acceptance.cpp`, one behavior per file,
  added to the component's CMake file.
- A test names the property in its output, so a failure says what broke without you reading the
  assertions.
- Fixtures are labeled as fixtures. A fake decoder that passes is not evidence that a real one
  does, and the test output says so.
- A test that asserts its own constant, or that passes because it never ran, is worse than no
  test. Do not remove or weaken a test to make a change land.

## What not to do

- Do not add a dependency without saying why. Everything shipped also has to be licensed,
  noticed and cross-compiled.
- Do not change behavior inside a refactor. Refactors preserve observable behavior or they are
  not refactors.
- Do not leave `TODO`, `FIXME`, commented-out code, or dead functions behind.
- Do not use `using namespace` in a header, ever, and not in a `.cpp` either.
- Do not print from library code. The application has one logging path, and diagnostics go
  through it.

## Before you open a pull request

- It builds with two jobs, and `ctest` passes: `python3 -B -m unittest discover -s tests` and the
  installer suite as well.
- You ran it against a real file or directory, not only a fixture.
- The change is explained in your own words, and you can answer questions about any line of it.
- The notices, the licenses and the legal files are untouched.
- Read [AI usage](../AI-USAGE.md): a human reviews everything before it goes public, and
  untested or obviously machine-generated dumps are closed without discussion.
