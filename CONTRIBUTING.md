# Contributing

Start with [Developer setup](docs/DEVELOPMENT.md), [Build](docs/BUILD.md) and
[Architecture](docs/ARCHITECTURE.md). Then read the two documents that decide how your work is
judged: the [C++ guidelines](docs/CPP-GUIDELINES.md) and the [AI usage policy](AI-USAGE.md).

The application is free software under the [GNU GPL version 3 or later](LICENSE); read it
before proposing terms or copying independently owned code. The third-party
notices are separate and stay where they are.

## Changes

- Work on a focused branch, and say which user-visible behavior changes.
- Write the failing test first, then fix it, then run the changed subsystem's tests and the
  integration contracts. Passing tests are not headset acceptance.
- Use C++20, C ABI and GLSL. Preserve public formats and observable behavior.
- Follow the [C++ guidelines](docs/CPP-GUIDELINES.md): naming, comments only where the code
  cannot explain itself, small enough to be one thing, errors thrown and reported instead of
  swallowed.
- Use the Python standard library for tooling. A new dependency needs approval first, because
  everything shipped also has to be licensed, noticed and cross-compiled.
- Keep a file to one cohesive thing (SOLID) and split it when it becomes two, not to hit a
  line count.
- Preserve files, releases, media, logs and recovery evidence. No automatic cleanup.
- Never hard-code credentials, private device access or personal SDK paths.
- Build Release with at most two jobs for routine verification.

## Issues and pull requests

Use the templates. They ask for the problem, the scope, the evidence, the exact commands you
ran and their real output, and what you did not test. Say whether you tested the working tree
or the exact shipped artifact, and only claim controller or headset acceptance if you actually
exercised it. Mobile stays out of release-readiness claims, and nobody removes a third-party
notice or changes the application license casually.

A human reviews everything before it goes public. Say which tool you used, if any; untested
dumps and obvious machine-generated text are closed without discussion, as
[AI usage](AI-USAGE.md) explains.

Ordinary GitHub CI builds the dependency-light components and runs the installer and
packaging suites. The full SDK and device requirements are described separately.

## The version

The version lives in one place, the `project(...)` line in `CMakeLists.txt`, and it is set by the
maintainer. Do not change it in a pull request; it is bumped as part of a release, by hand, so that a
release is a decision rather than a side effect of merging.
