# Suyu core source snapshot

This directory contains the Suyu source snapshot used by Shireia Next's core
build. It is based on the public `suyu-emu/suyu-main` repository at commit
`347fcf91d95cd39cb5c3db50f1b6d827ad699311` (`mk8-recomp`), with the Suyu
`PROVENANCE.md`, root license, source headers, and license directory preserved.
Build identity sidecars (`GIT-COMMIT`, `GIT-REFSPEC`, and `GIT-TAG`) pin the
reported source revision without relying on Git metadata from the outer
Shireia repository.

## Shireia downstream source changes

The following six downstream changes are present in the source and recorded in
`shireia-core.patch`. Their original Suyu/yuzu copyright and SPDX notices remain
in place:

- `src/common/fs/fs_paths.h` and `src/common/fs/path_util.cpp`: use the Shireia
  data-directory name and migrate the legacy Suyu directory only when a target
  directory does not already exist.
- `src/video_core/renderer_opengl/present/fsr.cpp`: pass the number of `uvec4`
  values, rather than the byte count, to the OpenGL uniform call.
- `src/video_core/renderer_vulkan/present/layer.cpp`: wait for in-flight GPU
  resources before replacing antialiasing resources and order resize cleanup
  safely.
- `src/video_core/renderer_vulkan/vk_blit_screen.cpp` and `.h`: recreate
  presentation layers when the output extent changes.

These are Shireia integration patches on top of Suyu; they are not represented
as upstream Suyu changes. The patch file is retained as a reviewable record;
the source files in this directory already contain it, so build scripts do not
apply it a second time.

### Authorship and license scope of the downstream patch

Willlchrist, project lead, stated on 22/10/2026 that the new Shireia
contributions were developed under their direction with AI assistance, that no
other human coauthors or contractual restrictions are known, and that they
authorize GPL-3.0-or-later for original additions to the extent they hold the
necessary rights. This statement does not claim ownership of the surrounding
Suyu, yuzu or Torzu code, nor guarantee that every new line is original.

The six-file core patch has 78 added and 14 removed diff lines. Its additions comprise guarded data-directory migration, an OpenGL uniform element-count correction, Vulkan in-flight resource retirement, and presentation-layer recreation when output dimensions change. The edits modify inherited files and include adaptation of upstream control flow; line-level ownership cannot be established from the patch alone. Willlchrist's GPL-3.0-or-later authorization applies only to portions for which they hold sufficient rights.

The three-file publication patch has six added and 26 removed diff lines. It excludes embedded shared-font fallback sources, uses locally installed firmware archives, and removes Suyu frontend media/logo resource references. It is a packaging/runtime choice and does not grant rights over omitted assets.

The six file diffs were reviewed individually: `fs_paths.h` changes directory
labels; `path_util.cpp` adds guarded migration; `fsr.cpp` fixes the GL element
count; `present/layer.cpp` orders resource retirement; and `vk_blit_screen.cpp`
with its header tracks output extent for layer recreation. All changes are
small modifications inside existing upstream files. `git apply --reverse
--check --ignore-whitespace shireia-core.patch` succeeds against this snapshot,
showing that the patch corresponds to the included sources. Existing upstream
SPDX/copyright notices remain in each touched file. The additions are
downstream modifications under the applicable GPL terms; the surrounding
files and prior authorship remain upstream. No separate third-party snippet
was identified in this patch review, but this check is not proof of exclusive
authorship.

The local Suyu worktree also had unrelated Qt frontend, translation, document,
and branding changes. Those changes are not included because the Shireia core
build disables the Suyu Qt frontend. Two untracked graphics lifetime test
sources relevant to the Vulkan resource-lifetime patch are retained under
`tests/graphics_settings/`; the untracked Suyu-branded icon files were omitted.

## Vendored and separately obtained source

The Dynarmic submodule used by the core is included at commit
`19249e8cbfb6e0d4c062bd7c4a111f55f220c211`; its own license is preserved in
`externals/dynarmic/LICENSE.txt`. The Suyu build's remaining CMake-managed
dependencies are resolved from the version and SHA-512 records in
`cpmfile.json`. CMake obtains those source packages into the ignored local
cache on first build. They are not copied from a prebuilt Suyu build, and no
binary archives or object files are included here.

The Suyu source is GPL-3.0-or-later, with inherited GPL-2.0-or-later files;
Dynarmic's included license text is 0BSD (not MIT). Other downloaded dependency licenses remain
with their upstream packages and are summarized in the project's
`third_party/NOTICES.md`. The root GPL text and conditional license choice for
original Shireia contributions do not change the licenses of this vendored source.

## Source-publication asset exclusions

The public Git index excludes the two Suyu Qt frontend MP3 files
(`dist/suyu_loading.mp3` and `dist/sounds/midnight_tokyo_lofi.mp3`),
`dist/readme/suyu__Logo-Pill.svg`, and the four Glyphicons font files under
`externals/dynarmic/externals/fmt/doc/_static/fonts/`. Their redistribution
terms are not established for this source package. They are not inputs to the
Shireia Windows core/frontend build. The Suyu Qt resource manifest no longer
lists the excluded media and logo. This is an intentional difference from the
pinned upstream source snapshot; do not describe the directory as byte-for-byte
identical to the upstream commit.

The three unreferenced Suyu documentation images `img/daily-builds.png`, `img/maximum-clocks.png` and `img/need to fix bugs.png` have no identified license/provenance notice and are not referenced by the Suyu source/documentation. A further 33 unreferenced files: 29 under `dist/icons/overlay_dark/` and four loose default-theme status icons are excluded from the Git index for the same reason; active QRC variants remain. They are excluded from the public Git index via `.gitignore`; local originals are retained. This exclusion is separate from `shireia-publication.patch` and does not modify upstream files.

The six embedded shared-font payloads and their wrapper sources/headers under
`src/core/file_sys/system_archive/` are also excluded from the public Git
index. `src/core/CMakeLists.txt` no longer compiles them, and
`system_archive.cpp` no longer synthesizes the five shared-font system
archives. The existing platform service first reads the corresponding data
archives from the user-installed firmware. This downstream choice requires
those archives for font-dependent content and removes fallback behavior when
the installed firmware is absent or incomplete. Shireia's setup already
requires validated user-provided firmware; this change does not distribute
firmware, keys, or font payloads.

## Public-source packaging patch

`shireia-publication.patch` is the reviewable patch at this directory's root. It records the public-source packaging changes relative to the staged Suyu snapshot: (1) removal of embedded shared-font wrapper sources from `src/core/CMakeLists.txt`; (2) use of user-installed system archives instead of synthesizing those font archives in `src/core/file_sys/system_archive/system_archive.cpp`; and (3) removal of Suyu Qt frontend media and the restricted logo resource from `src/suyu/suyu.qrc`. The corresponding media, logo and font source files are excluded from the public Git index.

The patch contains exactly those three paths. `shireia-core.patch` contains six different paths (`fs_paths.h`, `path_util.cpp`, `fsr.cpp`, Vulkan `present/layer.cpp`, `vk_blit_screen.cpp` and `vk_blit_screen.h`); there is no path overlap between the patches. Reverse-application checks passed against the current source snapshot for both patches. This verifies that each patch's changes are present in the worktree; it does not prove the provenance or licensing of every new line.

The publication patch changes downstream packaging and runtime behavior. Existing Suyu/yuzu copyright and SPDX headers remain in source files. Their licenses continue to govern inherited code; GPL-3.0-or-later applies to original Shireia contributions only to the extent Willlchrist holds the necessary rights. The patch does not claim ownership of upstream code or grant rights to excluded media, fonts or firmware.


## Additional public-snapshot changes outside the two core patches

The platform application icons in dist/shireia.bmp and dist/shireia.icns are generated from the Shireia butterfly logo in assets/brand/shireia-logo.png. The CLI resource path in src/suyu_cmd/CMakeLists.txt, macOS icon path in src/suyu/CMakeLists.txt, CFBundleIconFile in src/suyu/Info.plist, and the consumer comment in src/suyu_cmd/emu_window/emu_window_sdl2.h use the Shireia filenames. The original Suyu copyright/SPDX headers in those inherited source files are unchanged. dist/suyu.bmp and dist/suyu.icns are replaced in the public snapshot. Provenance limits and the pending independent mark review are documented in assets/brand/NOTICE.md and docs/REVISAO_IDENTIDADE_VISUAL.md.

docs/Build.md no longer embeds docs/img/creator-1.png; its Qt Creator instructions are text-only. The screenshot is excluded from the public Git index by explicit project-lead authorization. The package-policy tests now use tests/package_policy/fixtures/shireia-synthetic-detached-signature.asc instead of the Debian libxau detached signature. The synthetic v4 packet preserves syntax/parser, size/hash, manifest, malformed-payload, and collector test coverage, but is deliberately not cryptographically valid. Fixture provenance is recorded beside it. These changes are outside shireia-core.patch and shireia-publication.patch; neither patch was regenerated or altered.
