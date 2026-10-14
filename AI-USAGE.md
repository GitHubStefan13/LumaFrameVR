# AI usage

AI tools are welcome in this project, and this project itself is written with AI
assistance. There is no point pretending otherwise, and no penalty for saying so.

What that does not change is who is responsible.

## The rule

Everything that goes public here, whether it is an issue or a pull request, has been read by a
human before it is posted. Not skimmed, not pasted from a chat window. Read, understood and
answered for. If a maintainer asks why a line is there, the person who opened the pull request
has to know the answer without asking a model.

Concretely, before you open a pull request:

- You built it and you ran the tests. Paste the commands and the real output.
- You ran the change against a real file or a real directory, not only against a fixture.
- You can explain every line you are submitting in your own words.
- You say which tool you used, if any. One line is enough: "written with <tool>, reviewed by me".
- If you removed or changed a test, you say why. Removing a failing test is not a fix.

## What gets closed without warning

- Obvious AI slop: padding, restated code, invented APIs or flags that do not exist, comments
  explaining what the next line already says, features nobody asked for, three paragraphs where
  one sentence would do.
- Untested or unreviewed code: a diff that was never built, tests that were never run, a
  description that is a promise rather than a result.
- Issue reports that are generated text with no reproduction, no version and no log.
- Anything whose author cannot answer questions about it.

No argument, no discussion, closed. Reopening with a real reproduction or a working, tested
change is fine and costs you nothing.

## Contributions and licensing

This project is free software under the GNU GPL version 3 or later (see [LICENSE](LICENSE)), and
what you send in comes under the same license, which is the same deal everyone else gets. You
keep the copyright on what you wrote. Read [CONTRIBUTING.md](CONTRIBUTING.md) and the
[C++ guidelines](docs/CPP-GUIDELINES.md) before you start, and use the issue and pull request
templates: they ask for exactly what a reviewer needs.
