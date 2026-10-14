# Localisation

Every user-visible interface string lives in a language file under `lang/`, never in code.
The interface reads the catalogue at start-up and selects the language the user chose.

## Files

`cpp/i18n/lang/en.lang` is the reference catalogue. A language file is a plain text file:

```
# LumaFrameVR interface language file (Spanish).
code = es
name = Español

browser.folders = Carpetas
browser.items = {count} elementos
```

- `key = value`, one entry per line. `#` or `;` starts a full-line comment.
- `code` (short, lowercase, stable) and `name` (shown in the interface) are required.
- Keys are unchanged from the English file. Never rename a key in one file only.
- `{placeholder}` tokens are substituted at run time; keep them, and keep the same set.
- A malformed file (duplicate key, missing `name`, a line without `=`) is refused and the
  rest of the catalogue stays usable; nothing is half-applied.
- A key a language does not define falls back to English; a key no language defines shows
  the key itself, which makes the gap obvious in the interface.

## Adding a language

1. Copy `cpp/i18n/lang/en.lang` to `lang/<code>.lang`.
2. Set `code` and `name`, translate every value, keep every key and placeholder.
3. Run `ctest -R '^(catalogue_acceptance|hardcoded_ui_text_audit)$'`. The catalogue test
   fails if a key or placeholder is missing or a value is empty; the audit fails if any
   source file hardcodes interface text or uses a key no language file defines.
4. Add the file to the release payload so `lang/` in the package carries it.

No code change is needed: the prompt and the settings row list whatever `lang/*.lang`
files are present, in file-name order.

## Where the catalogue is read

- `LUMAFRAMEVR_LANG_DIR` when set, else `$LUMAFRAMEVR_PACKAGE_ROOT/lang`, else `lang/`
  beside the executable, else the development source directory.
- The first real start (a configured viewer state that does not exist yet) asks for a
  language once. Later starts reuse the stored choice; with storage disabled the interface
  keeps the default English instead of asking every time.
- The prompt is shown over the file browser, so a first start that opens a media file
  directly keeps English and offers the choice in Viewer advanced settings instead.

## Limits

The interface font ships ASCII plus a handful of symbols and maps other accented Latin
characters to their base letter, so `Español` renders as `Espanol`. Adding a real glyph
means extending `cpp/ui/font.cpp` (`font_glyph`) with the composed character.

Diagnostic and CLI surfaces (`--help`, `--probe`, stderr traces) stay in English: they are
developer-facing, not the rendered interface. Numbers, units and technical tokens (`GiB`,
`MiB`, `Mbit/s`, `fps`, `HW`, `SW`, `N/A`) are intentionally not translated.
