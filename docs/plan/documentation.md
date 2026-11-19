# Documentation plan

Tracking issue: [h4tr3d/avcpp#67 "Useful documentation"](https://github.com/h4tr3d/avcpp/issues/67).

Status: planned, not started. Baseline measured on 2026-11-18 against `master` (`v3.1.0`).

This document is the plan of record for the documentation workstream. Update it in place as
milestones close; do not create a second plan file.

## 1. Goal

Make the library usable without reading its source:

- a published site with a modern look, light/dark themes, unified search;
- English and Russian for everything hand-written;
- a complete API reference, generated from source comments;
- every example documented and reachable from the guides;
- a beginner path: install → decode → encode → filter → hardware.

Non-goals, deliberately out of scope:

- translating API reference comments into Russian (see decision D2);
- replacing Doxygen with another C++ parser;
- an archived build for every release tag (see D3);
- changing the public API of the library in the course of documentation work;
- PDF/LaTeX output: `GENERATE_LATEX` is dropped, not maintained.

## 2. Baseline

Measured with `doxygen` 1.18.0, XML output, `EXTRACT_ALL = NO` (the tracked `Doxyfile` sets
`EXTRACT_ALL = YES`, which suppresses every "not documented" warning and therefore hides the
regression). Reproduce with the recipe in Appendix A.

| metric | value |
| --- | --- |
| warnings, tracked settings (`EXTRACT_ALL = YES`) | 20 |
| warnings, `EXTRACT_ALL = NO` | 1022 |
| `Member ... is not documented` | 897 |
| `Compound ... is not documented` | 36 |
| `return type of member ... is not documented` | 36 |
| `parameters of member ... are not documented` | 35 |
| `no matching / no uniquely matching class member` | 10 |
| stale `@param` / `@return` on `void` | 7 |
| public members of `namespace av` | 975 |
| public members with any documentation | 154 (15.8%) |
| public members with no documentation at all | 821 (84%) |
| public classes/structs with a brief | 45 / 82 (55%) |
| namespaces with a brief | 1 / 17 (`av` itself has none) |
| headers with a `@file` block | 0 / 28 |
| `@defgroup` / `@ingroup` usage | none |
| examples listed in `Doxyfile` `INPUT` | 0 of 21 |

Worst offenders by undocumented public members:

| header | public | documented | zero docs |
| --- | --- | --- | --- |
| `hwcontext.h` | 156 | 5 | 151 |
| `codeccontext.h` | 163 | 39 | 123 |
| `frame.h` | 79 | 9 | 70 |
| `dictionary.h` | 87 | 28 | 59 |
| `subtitle.h` | 62 | 12 | 50 |
| `packet.h` | 46 | 1 | 45 |
| `linkedlistutils.h` | 41 | 2 | 39 |
| `rational.h` | 34 | 0 | 34 |
| `avutils.h` | 33 | 7 | 26 |
| `ffmpeg.h` | 24 | 0 | 24 |

Thirteen headers have no documented member at all: `rational.h`, `ffmpeg.h`, `sampleformat.h`,
`codecparameters.h`, `videorescaler.h`, `codec.h`, `rect.h`, `pixelformat.h` and five more.
`buffer.h` is the only well-covered header (29/31).

Published site today: stock Doxygen theme, `PROJECT_NUMBER = 2.0` while the library is 3.1.0,
`README.md` as main page, `doc/html` force-pushed to `gh-pages` on every push to `master`.
CI uses Ubuntu's doxygen 1.9.1; local work uses 1.18.0.

## 3. Decisions

| id | decision | rejected alternatives |
| --- | --- | --- |
| D1 | Infrastructures and the beginner guide come first; API coverage is a rolling second track, sliced per header | API-first, all tracks in parallel |
| D2 | Hand-written content is bilingual; the API reference is generated once in English, plus a hand-written Russian API navigator that links into it | per-language comment blocks (`\~english`/`\~russian`), two full translated references |
| D3 | `latest` plus an archive per minor for 3.x and one archived slice for 2.x; no archive per tag | latest only; per-tag archive |
| D4 | Authored sources are tracked under `docs/`; `doc/` stays git-ignored build output | sources under `doc/`; docs in a separate repository |
| D5 | English is the source of truth, Russian is a mirror; drift is visible in `docs/translations.yaml` and reported by CI as a warning, never a failure | hard CI gate on prose staleness; Russian-first |
| D6 | This file is the plan of record, tracked in the repository | local `PLAN.md`; issue-only plan |
| D7 | Doxygen only: `doxygen-awesome-css` for the look, Markdown `@page` files for guides, two language builds | MkDocs Material + Doxygen subtree; MkDocs + moxygen; Sphinx + Breathe |
| D8 | Sidebar-only theme, dark-mode toggle, custom chrome (language switcher, version selector, GitHub link) injected by `docs/theme/site-nav.js`, minimal forked `HTML_FOOTER` | forking `HTML_HEADER`; base theme without custom chrome |
| D9 | `/avcpp/{version}/` is the English site (flat, as doxygen emits it: `classav_1_1X.html`, `guide_*.html`), `/avcpp/{version}/ru/` holds Russian guides only | `/avcpp/{lang}/{version}/`; `/avcpp/{version}/{lang}/`; a hand-built `api/` and `guide/` split |
| D10 | Pin doxygen 1.18.x from the official release tarball; `EXTRACT_ALL = NO`; warning **ratchet** against a checked-in per-file baseline; docs build on PRs, deploy on `master` and on tags into `gh-pages` | `WARN_AS_ERROR = YES` today; keep `EXTRACT_ALL = YES`; `actions/deploy-pages` |
| D11 | All 21 samples get `@example`; about six are quoted in guides via `@snippet`; each sample gains a header comment | `@example` only; separate Markdown copies of the code |
| D12 | `docs/translations.yaml` declares only the page pairs and how complete each is; staleness is derived from git by comparing the last commit on each side | front matter in the page; `@htmlonly` metadata alias; a hand-maintained `source_sha` (it is stale the moment the pair is committed) |
| D13 | First-line summary is the brief (`JAVADOC_AUTOBRIEF` is already `YES` in the tracked config); `@` tags; `///` for one-line docs, `/** ... */` when tags are present | `@brief` everywhere; all `///`; all block comments |
| D14 | Authoritative `doxygen` build is a CMake target behind `AVCPP_ENABLE_DOCS` (default OFF) | shell script only; CI only |

Facts behind D7 and D10, checked on 2026-11-18:

- `doxygen-awesome-css` v2.4.2 (2026-03) supports doxygen **1.9.1–1.18.0**, requires
  `HTML_COLORSTYLE = LIGHT`, works with `GENERATE_TREEVIEW` sidebar mode, ships a dark-mode toggle.
- `mkdocs-static-i18n` 1.3.1 and `breathe` 5.1.0 are alive, but `doxybook2` is archived (2020) and
  Exhale is documented as suitable for small/medium projects. Neither weakness justifies moving
  this project's reference out of Doxygen.
- Doxygen strips HTML comments from Markdown pages and renders YAML front matter as visible text
  (verified with 1.18.0), so page-level translation metadata is not viable — hence D12.

## 4. Repository layout

```
Doxyfile                     # English build, tracked
docs/
  plan/documentation.md      # this file
  ru.doxyfile                # includes Doxyfile, overrides INPUT/OUTPUT_LANGUAGE/HTML_OUTPUT
  build.sh                   # builds <out>/<major.minor>/, <out>/<major.minor>/ru/, latest/
  check-warnings.sh          # ratchet against docs/doxygen-baseline.txt (--update regenerates)
  check-translations.sh      # reports pairs whose English side moved ahead
  site-index.sh              # regenerates the version chooser and 404.html for a site tree
  translations.yaml          # en/ru pairing and staleness metadata
  doxygen-baseline.txt       # accepted warning counts per file
  theme/                     # pinned doxygen-awesome css/js, custom.css, footer.html,
                             # site-nav.js, 404.html, LICENSE.doxygen-awesome-css
  img/                       # shared diagrams (svg preferred, sources kept)
  en/
    index.md                 # @mainpage: what, why, three-line install, where to go
    building.md              # full build matrix, moved out of the landing page
    guide/*.md               # getting_started, decode, encode, remux, filters, subtitles,
                             # errors, hw_codecs
    api-map.md               # "which type do I need for which job"
  ru/
    index.md                 # @mainpage for the Russian build
    api-nav.md               # Russian navigator over the English reference
    guide/*.md               # mirrors of the English guides
example/api2-samples/*.cpp   # M2, M4: @example + header comment per sample
doc/                         # generated output of a bare `doxygen`, git-ignored
build/docs/site/             # generated site of docs/build.sh, git-ignored
```

`docs/` used to be excluded through `.git/info/exclude`. M0 replaced that blanket exclusion with
narrow entries for the still-private material; `docs/hw-codecs.ru.md`, its HTML and the
`docs/hw-codecs/` diagrams leave the exclusion when M1 moves them into
`docs/ru/guide/hw_codecs.md` and `docs/img/`.

## 5. Doxyfile changes (M0)

Included here so the config is reviewable before it is written.

- `PROJECT_NUMBER`: `$(AVCPP_VERSION)`, injected from `VERSION.txt`; never a literal again (D3).
- `GENERATE_LATEX = NO`, `GENERATE_MAN = NO`, `GENERATE_RTF = NO`.
- `GENERATE_XML = YES` only in the CI coverage job, not in the published build.
- `EXTRACT_ALL = NO`, `WARN_IF_UNDOCUMENTED = YES`, `WARN_AS_ERROR = NO`, `WARN_LOGFILE` set (D10).
- `INPUT = src docs/en` for English; `docs/ru` only for Russian (D7, D9).
- `FILE_PATTERNS = *.h *.md`: implementation files are not parsed. They carry no public
  documentation, and their out-of-line definitions produced duplicate members that silently
  diverged from the declarations (doxygen reported them as "no matching class member"). The cost
  is the "Definition at line ..." links, which are not worth the noise.
- `PREDEFINED`: a full modern build (all optional components, C++20, FFmpeg 7.x versions). Without
  it, every `#if AVCPP_HAS_*` block is dropped, because those macros live in the generated
  `avconfig.h`, which the documentation build does not have. This was silently hiding the whole
  avformat, filter, hardware and subtitle layers from the published site.
- `FILE_PATTERNS += *.md`, `MARKDOWN_SUPPORT = YES` (already on), `JAVADOC_AUTOBRIEF = YES`
  (already on, D13).
- `USE_MDFILE_AS_MAINPAGE` cleared: `docs/en/index.md` carries `@mainpage`, because the landing
  page and the build instructions are now separate documents.
- `IMAGE_PATH = docs/img`, `EXAMPLE_PATH = example/api2-samples` (D11).
- Theme (D8): `HTML_EXTRA_STYLESHEET = docs/theme/doxygen-awesome.css`,
  `docs/theme/doxygen-awesome-sidebar-only.css`,
  `docs/theme/doxygen-awesome-sidebar-only-darkmode-toggle.css`, `docs/theme/custom.css`;
  `HTML_COLORSTYLE = LIGHT`, `GENERATE_TREEVIEW = YES`, `DISABLE_INDEX = NO`,
  `FULL_SIDEBAR = NO`. `HTML_EXTRA_FILES` only *copies* the theme scripts into the output, so
  `HTML_FOOTER = docs/theme/footer.html` — a copy of doxygen's footer plus five `<script>` tags —
  is what actually loads the dark-mode toggle, the paragraph links, the copy button and
  `site-nav.js`. The header template is not forked: it carries more version-specific tokens than
  the footer, and doxygen-awesome-css advises against changing the generated structure.
- `DOT_*` tuning: `DOT_GRAPH_MAX_NODES` lowered to 40, `MAX_DOT_GRAPH_DEPTH = 2`; the hardware
  and codec hierarchies are wide and today produce unreadable graphs.
- Drop the 13 obsolete tags reported by doxygen 1.18 to keep the warning log honest.
- Russian build: `docs/ru.doxyfile` starts with `@INCLUDE = ../Doxyfile` and overrides
  `INPUT`, `OUTPUT_LANGUAGE = Russian`, `OUTPUT_DIRECTORY`, `HTML_OUTPUT`, `PROJECT_NUMBER`.

## 6. Reference structure and writing rules

Groups (`@defgroup` / `@ingroup`) replace the flat list of 28 headers:

| group id | scope |
| --- | --- |
| `core` | errors, log, time/timestamp, rational, buffers, dictionary, string utils |
| `avformat` | format and IO contexts, streams, packets, time bases |
| `codec` | codecs, codec contexts, codec parameters, encoder/decoder helpers |
| `frame` | frames, subtitle frames, pixel and sample formats, video rescaler, audio resampler |
| `filter` | filter graph, buffer source/sink |
| `hw` | hardware devices, hardware frames, hardware codec contexts |
| `subtitle` | subtitle payloads, ASS rectangles, subtitle codec paths |

Every header gets a `@file` block (`@brief`-free, first line only) and an `@ingroup`.

Definition of done for a public member:

1. first line is a complete one-sentence summary (becomes the brief via `JAVADOC_AUTOBRIEF`);
2. every parameter documented with `@param`, direction and units where applicable;
3. `@return` when the function returns something;
4. `@note` on ownership, lifetime and thread-safety wherever a caller can get it wrong — this is
   the dominant source of user bugs in this library (`CodecContext2`, `hwFramesContext`,
   zero-copy frames, borrowed AVIO buffers);
5. `@since <version>` mandatory for API added from 3.2 on, optional for backfill;
6. `@deprecated` mirroring the `[[deprecated]]` attribute, so the reference and the compiler agree.

Comment form (D13): `///` for one-line docs and for `/// @{` group markers; `/** ... */` as soon as
`@param`, `@return` or `@note` is present. English only.

Pre-existing defects to fix while touching the files (they are part of M0 acceptance):

- 7 stale `@param`/`@return` (`avutils.h:597` wrong name, `dictionary.h:343` extra `flags`,
  `dictionary.h:465` `@return` on `void`, five "following parameter(s)" reports);
- 10 comments that silently fail to attach because the declaration writes `const class Codec &`
  while the definition writes `av::Codec` — normalize the elaborated types;
- the bogus `<close>:1` location for `CodecContext2::close`, caused by the `[[deprecated]]`
  attribute; document it the way doxygen can attach (spell out the method, avoid the attribute in
  the comment's scope).

## 7. Guides

Each guide exists as `docs/en/guide/<name>.md` and `docs/ru/guide/<name>.md`, joined to navigation
with `@page`/`@subpage` and `@tableofcontents`, named with a stable page id (`guide_decode`) so
anchors survive retitling.

| page id | content | en | ru |
| --- | --- | --- | --- |
| `index` | landing: what avcpp is, the pitch, install in three lines, links | yes | yes (partial) |
| `building` | the build matrix, the integration recipes, the docs targets | yes | no |
| `getting_started` | first program: open a file, decode it, print frames | yes | yes |
| `decode` | packets → frames, the send/receive contract, flushing, timestamps | yes | yes |
| `encode` | encoder setup, time bases, draining, `writeTrailer()` | yes | yes |
| `remux` | copying streams without re-encoding, what cannot be remuxed | yes | yes |
| `filters` | graph construction, buffer source/sink, pushing and pulling | yes | yes |
| `subtitles` | the legacy codec path, rect kinds, encoder requirements | yes | yes |
| `errors` | `OptionalErrorCode`, categories, the full list of frequent codes | yes | yes |
| `hw_codecs` | hardware decoding, encoding, zero copy, platform limits | yes | yes |
| `api_map` / `api_nav` | "which type for which job" | yes | yes (navigator over the English reference) |

## 8. Examples

For each of the 21 files in `example/api2-samples/`:

1. header comment: what it does, input/output, how to run it, which library parts it shows;
2. `@example` block so doxygen emits a page with the full source;
3. `@snippet` markers around the 10–20 lines that a guide quotes (named markers such as
   `//! [decode_loop]`).

Guides quote snippets; they never copy code. The six samples listed in §7 get snippet markers in
M2, the remaining ones in M4.

## 9. Publishing and CI

Workflow `docs.yml` is rewritten:

1. **Build job** (pull requests and `master`): pinned doxygen 1.18.x from the official release
   tarball, cached by version; graphviz from apt; `cmake -DAVCPP_ENABLE_DOCS=ON` then the `docs`
   target, so CI and contributors run the same command (D10, D14).
2. **Ratchet job**: `docs/check-warnings.sh` builds the reference without HTML output, aggregates
   warnings per file and compares them with `docs/doxygen-baseline.txt` (`<path> <TAB> <count>`).
   It fails when a file produces more warnings than the baseline and never when it produces fewer;
   `--update` regenerates the file and must be committed in the same PR. A per-file count is
   coarser than a line-by-line diff, but it keeps the baseline at ~40 lines instead of ~1500 and
   matches the "one header per PR" workflow.
3. **Deploy job** (`master` and tags): English build into `gh-pages/{major.minor}/`, Russian build
   into `gh-pages/{major.minor}/ru/`, `latest` refreshed by copying the published directory, root
   `index.html` regenerated by `docs/site-index.sh` from the directories that actually exist, and
   `404.html` copied from `docs/theme/`. Patching a release (3.1.1) refreshes the `3.1` directory.
4. Old version directories are never deleted by automation; removing a version is a manual commit
   on `gh-pages`.

GitHub Pages cannot issue server-side redirects, so `404.html` rewrites the pre-3.1 flat URLs by
rule: `<base>/<page>.html` becomes `<base>/latest/<page>.html`. A rule rather than a table means
old links keep working even for pages that no longer exist, and there is no inventory to maintain.
It is best-effort: crawlers and clients that ignore `location.replace()` land on the 404 page.

## 10. Milestones

Estimates assume one maintainer working part-time; they are planning numbers, not commitments.

### M0 — infrastructure (1–2 days) — done

- [x] rewrite `Doxyfile` per §5; add `docs/ru.doxyfile`
- [x] vendored `doxygen-awesome-css` 2.4.2 into `docs/theme/`, with `custom.css`, `footer.html`
      (theme scripts plus `site-nav.js`) and `site-nav.js` (language switcher, version selector,
      GitHub link)
- [x] `@defgroup` skeleton for the seven groups, `@file` + `@ingroup` on all 34 headers
- [x] `docs/en/index.md` (landing), `docs/en/building.md`, `docs/ru/index.md` (Russian stub)
- [x] `docs/translations.yaml` seed and `docs/doxygen-baseline.txt` (1499 warnings, 33 files)
- [x] CMake: `AVCPP_ENABLE_DOCS` option, `docs`, `docs-check` and `docs-serve` targets
- [x] rewritten `.github/workflows/docs.yml` per §9
- [x] narrowed `.git/info/exclude` entries for `docs/`
- [x] fixed the stale `@param`/`@return` (guessValue, Dictionary::operator=, parseString,
      FormatContext::initOutput/openOutput) and the misattached comments; the ten "no matching
      class member" warnings disappeared with the switch to header-only parsing
- [x] documented the members that `[[deprecated("...")]]` made doxygen invent at bogus
      `<name>:1` locations (`close`, `addStream`, `currentDts`)
- [x] restored the avformat, filter, hardware and subtitle layers with `PREDEFINED`: the
      published site had been missing them entirely

Acceptance: `cmake -DAVCPP_ENABLE_DOCS=ON && cmake --build build --target docs` produces the site
locally; the PR CI job fails on a deliberately introduced undocumented member and passes without
it; the landing page and the group tree are visible; the version shown is the one in `VERSION.txt`.

### M1 — first real article (2–3 days) — done

- [x] translated the hardware brief to `docs/en/guide/hw_codecs.md`, moved the Russian original to
      `docs/ru/guide/hw_codecs.md`, diagrams (`svg` plus their `dot`/`puml` sources) to
      `docs/img/hw-*.{svg,dot,puml}` and referenced them with `@image html`
- [x] registered the pair in `docs/translations.yaml` as `complete`
- [x] linked it from both landing pages and from the `@ref hw` group page

Outcome: the same page id (`guide_hw_codecs`) serves both builds, so the URL is
`/<version>/guide_hw_codecs.html` in English and `/<version>/ru/guide_hw_codecs.html` in Russian,
and each language page links to the group page of its own build.

Acceptance: both language builds show the article with diagrams; the RU page links into the
English `hw` group without dead links; the translation checker reports the pair as current.

### M2 — beginner guides (about a week) — done

- [x] `getting_started`, `decode`, `encode`, `remux`, `filters`, `subtitles`, `errors` in en and ru
- [x] snippet markers in six samples (`send_receive_*`, `encoder_setup`/`encode_loop`,
      `remux_streams`/`remux_loop`, `filter_graph`/`filter_pump`, `subtitle_decode`,
      `legacy_decode_loop`)
- [x] `api_map` (en) and `api_nav` (ru)
- [x] `docs/check-translations.sh` plus the `docs-translations` target and a CI step

Outcome: every guide exists in both languages, every code block is a `@snippet` from
`example/api2-samples/`, and the Russian pages link to the English reference by relative URL —
`@ref av::...` cannot resolve in the Russian build, which has no API pages.

Acceptance: a reader with FFmpeg installed but no avcpp experience can build and run the
`getting_started` program by following the page; every guide has a Russian counterpart listed as
current, or is explicitly listed as untranslated in `translations.yaml`.

### M3 — API coverage (3–4 weeks, rolling PRs) — in progress

Baseline: 1499 warnings at the start of M3, now **258**.

Done:

- `hwcontext.h` — 162 warnings to 0. Every public member, field, free function and iterator typedef
  is documented, with `@param`/`@return` and notes on ownership and lifetime.
- `codeccontext.h` — 305 warnings to 0. `CodecContext2`, `GenericCodecContext`,
  `CodecContextBase`, the video/audio/subtitle variants and their protected plumbing. The
  send/receive contract, hardware configuration and the subtitle legacy path are described where
  they are declared, not only in the guide.
- `ffmpeg.h` — 126 warnings to 0. `FFWrapperPtr`, `FFWrapperRef`, `PixSampleFmtWrapper`, the
  `_log` helpers and the eight `RAW_*`/`IF_*` macros. Documenting the templates once covered 21
  instantiations each, which is why the count collapsed.
- `rational.h` — 30 warnings to 0. Rational arithmetic that compares by value rather than by
  representation, the preserved zero denominator, rescaling, and the streaming operators.
- `timestamp.h` — 28 warnings to 0. The fixed-point model (value times time base), the chrono
  constructors including the precision-preserving integral path, comparison and arithmetic that
  align time bases through av_compare_ts, and toDuration().
- `format.h` — 33 warnings to 0. The shared Format<T> interface (name, flags, codec support),
  InputFormat and OutputFormat lookups by name/URL/MIME, the default codec ids a muxer falls back
  to, and the const/non-const alias that keeps one code path compiling against both FFmpeg header
  generations.
- `channellayout.h` — 35 warnings to 0. The view/owner split: a view shares the FFmpeg
  descriptor and is enough for queries, while ChannelLayout owns the description, duplicates it on
  copy and releases it on destruction, with release() for the hand-over case.
- `dictionary.h` — 58 warnings to 0. The dictionary as an FFmpeg linked list (so key lookup is
  O(n) while index access is O(1)), ownership and the takeOwning flag, both access interfaces
  (by index and by key), iteration with the two iterator flavours, entry value assignment, the
  raw-pointer escape hatches without which av_dict_* cannot be called, and DictionaryArray with
  its raw array handling.
- `packet.h` — 62 warnings to 0. The packet model (one stream, timestamps, duration, flags),
  every constructor: copy, reference, deep copy, wrap with and without ownership, the span
  variants, the data replacement methods, side data access, reference counting, makeRef/clone and
  the assignment operators. The padding requirement is stated where buffers are created: the
  zeroed AV_INPUT_BUFFER_PADDING_SIZE bytes after the payload are part of the contract, because
  the ASS muxer reads payloads as C strings.
- `subtitle.h` — 63 warnings to 0. `SubtitleRect` (payload kinds, geometry, text/ASS storage,
  bitmap and palette planes) and `SubtitleFrame` (move-only ownership without reference counting,
  display interval relative to pts, stream bookkeeping, rectangle access, and the fromText/fromAss
  builders that produce what the encoders accept).
- `formatcontext.h` — 74 warnings to 0. The container lifecycle (open, probe, close), the
  three ways to add a stream, seeking (five overloads, including the raw FFmpeg one), container
  metadata and event flags, the twelve openInput and twelve openOutput overloads, header and
  trailer writing, packet flushing, and the raw-frame paths that some muxers accept.
- `avutils.h` — 86 warnings to 0. Allocation helpers with the FFmpeg allocator, the deleters
  (`EmptyDeleter`, `v1::AvDeleter`, `v2::SmartDeleter`), scope guards, the C++20 sentinel and
  array-view machinery with its iterator, `EqualComparator`, `array_to_container`, the hex dump
  helpers and the constants `NoPts`/`TimeBase`.
- `frame.h` — 121 warnings to 0. `FrameCommon` (reference counting, timestamps, planes, side
  data), `Frame<T>` (CRTP assignment), `VideoFrame` (system vs device memory, clone, transfer,
  mapping, buffer copies) and `AudioSamples` (planar and packed layouts). Coverage of the side
  data API includes the wrap semantics of the three `addSideData` overloads.

Next, by current weight:

| header | warnings |
| --- | --- |
| `linkedlistutils.h` | 41 |
| `stream.h` | 26 |
| `audioresampler.h` | 22 |

The order changed since the first estimate: `codeccontext.h` was the largest, and `ffmpeg.h` and
`avutils.h` are now bigger than `frame.h` because their members were counted per template
instantiation before.

Notes from the first header:

- Iterator boilerplate (`iterator_category`, `value_type`, `difference_type`, `pointer`,
  `reference`) has to be documented too: doxygen reports each typedef, and hiding them with `@cond`
  would contradict "every public member is documented".
- Deleted copy operations cannot carry `@param`: their parameter is unnamed, so doxygen reports the
  documented name as missing. A brief without `@param` is the correct form.
- The `[[deprecated]]` attributes need a doc block above the attribute, otherwise doxygen invents a
  member at a bogus `<name>:1` location (this is how `close`, `addStream` and `currentDts` were
  fixed in M0).
- `@internal` is a trap: it ends the public description and swallows every following `@param` and
  `@return`, so the member is reported as undocumented. Mark internal helpers with prose, or keep
  them out of the reference with `EXCLUDE_SYMBOLS`.
- The compatibility shims in `av::codec_context::audio` and `av::codec_context::internal` are now
  excluded from the reference: they exist to bridge FFmpeg API generations and are not API.
- Protected plumbing of `CodecContext2` is documented rather than hidden: doxygen has no switch to
  omit protected members, `@cond` would leave real code undocumented, and the helpers are what a
  subclass author reads.
- doxygen 1.18 misnames the non-alphabetic bucket of the member index (`functions__.html` linked,
  `functions_~.html` written). `docs/build.sh` rewrites those links after the build; remove the
  workaround when the pinned release contains the fix.

Rules: one header per PR, baseline shrinks in the same PR, §6 definition of done applies, no
behavioural changes in the same commit.

Acceptance: `WARN_IF_UNDOCUMENTED` reports zero undocumented public members across `src/avcpp`;
`docs/doxygen-baseline.txt` is empty of member warnings; `EXTRACT_ALL = NO` stays.

### M4 — examples and polish (3–5 days)

- [ ] header comment and `@example` for all 21 samples, snippet markers for the remaining 15
- [ ] cross-references from every group page to the relevant guide
- [ ] verify the `404.html` rewrite rule against the URLs the old site published
- [ ] light/dark and mobile pass over the published site

Acceptance: every sample appears under "Examples"; every guide links a sample; the old flat URLs
resolve; the site is usable on a phone.

## 11. Risks

| risk | mitigation |
| --- | --- |
| Two doxygen runs diverge (EN/RU configs drift) | `docs/ru.doxyfile` includes the root file and overrides only INPUT/OUTPUT/LANGUAGE; CI builds both on every PR |
| Markdown pages produce duplicate auto-generated file pages and render front matter as text | metadata lives in `translations.yaml` (D12); M0 checks the page tree for duplicate pages |
| Moving pages breaks existing links | `404.html` rewrite rule; stable page ids; old version directories never deleted |
| Ratchet baseline becomes a graveyard | baseline may only shrink; a PR that adds a warning must add documentation or an explicit, reviewed exception |
| Translation drift | `translations.yaml` reports pairs whose English source changed after `source_sha`; warning first, gate later if it proves noisy |
| `doxygen-awesome-css` breaks on a future doxygen | version pinned in-repo; upgrade is a separate PR that reruns the visual check |
| Theming effort outstrips content | M0 timeboxed to 1–2 days; content milestones never wait for visual polish |
| doxygen 1.18 output differs from Ubuntu's 1.9.1 | CI pins the same version contributors use; the tarball is cached, so no apt surprise |

## 12. Deferred

- custom domain and its effect on existing URLs;
- analytics and social preview cards;
- a search synonym list for FFmpeg terminology;
- generated comparison tables (feature matrix per wrapper type);
- translating `api_map` into Russian (currently only `api_nav`).

## Appendix A — reproducing the baseline

```sh
cp Doxyfile /tmp/Doxyfile.baseline
# in the copy: OUTPUT_DIRECTORY=/tmp/doxybase, GENERATE_HTML=NO, GENERATE_LATEX=NO,
# GENERATE_XML=YES, EXTRACT_ALL=NO, QUIET=YES, WARN_LOGFILE=/tmp/doxybase/warnings.log
doxygen /tmp/Doxyfile.baseline
grep -c 'is not documented' /tmp/doxybase/warnings.log
```

Coverage percentages: parse `/tmp/doxybase/xml/*.xml`, keep `prot="public"` members, count those
whose `<briefdescription>` is empty.
