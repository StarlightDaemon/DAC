# Native static analysis

The production analysis target is `src/standalone.cpp`, which includes the DAC
platform and policy implementation. [VERIFICATION](VERIFICATION.md) records the
current analyzed source identities, compiler, outcome, and qualification limits.
Historical analysis results are not carried forward as a fresh pass.

This is compilation-time analysis only. It does not execute a DAC process, UI,
monitor control, hardware helper, or installation.

## Scope

The command uses the Release definitions and generated Fujin include directory,
C++23-capable MSVC language mode, `/MT /utf-8 /permissive- /EHsc /W4 /sdl
/guard:cf /O2`, and `/analyze /analyze:only`. It analyzes production code, not
`DAC_HARNESS`-only branches or the separate test programs.

Only MSVC/Windows SDK include directories from `INCLUDE` are designated external,
using `/external:env:INCLUDE /external:W0 /analyze:external-`. Project files and the
generated Fujin header remain inside the analysis scope. `/external:templates-`
keeps diagnostics for external templates instantiated by project code. No project
warning number is disabled and no project warning suppression was introduced.
Microsoft documents these switches in
[/analyze](https://learn.microsoft.com/en-us/cpp/build/reference/analyze-code-analysis?view=msvc-170)
and [/external](https://learn.microsoft.com/en-us/cpp/build/reference/external-external-headers-diagnostics).

## Findings resolved during development

The development review identified and resolved the following warnings:

| Diagnostic | Count | Resolution |
| --- | ---: | --- |
| C6262, large stack frames | 11 | Moved 32,768-character path buffers into local RAII vectors, retaining API capacities, length validation, and zero initialization. The largest initial frames were approximately 128 KiB in the options window procedure and hardware-helper validation. |
| C28251, entry-point annotation mismatch | 1 | Added the SDK's `_In_` / `_In_opt_` annotations to `wWinMain`. |
| C28020, histogram index precondition | 1 | Rewrote bucket selection as an explicitly bounded eight-element loop with an inclusive `UINT64_MAX` final boundary. Existing bucket boundaries and behavior remain identical. |
| C6326, constant comparison | 1 | Expressed the generated Fujin radius choice with `if constexpr`, matching its compile-time nature. |

The histogram warning was a conservative analyzer finding: the original loop
already constrained the index to 0–7 for an eight-element array. The constant
comparison was also intentional. Both now express those invariants directly;
neither required suppressing a diagnostic. No SDK-only diagnostic was carried
forward as a project defect.

Heap conversion retains the registry buffer's 65,536-byte capacity explicitly,
rather than incorrectly applying `sizeof` to the vector. That allocation occurs
before opening the registry key. The folder-dialog buffer is allocated before
acquiring the shell item. Other affected handles already have RAII ownership.

## Reproduce

From the repository root in PowerShell 7, after configuring the Release build:

```powershell
. ./tools/use-toolchain.ps1
New-Item -ItemType Directory -Force build/analysis | Out-Null
cl.exe /nologo /std:c++latest /MT /utf-8 /permissive- /EHsc /W4 /sdl /guard:cf `
  /O2 /DNDEBUG /DWIN32 /D_WINDOWS /DNOMINMAX /DUNICODE /D_UNICODE `
  /DWIN32_LEAN_AND_MEAN /DWINVER=0x0A00 /D_WIN32_WINNT=0x0A00 `
  /Ibuild/release /Iresources /external:env:INCLUDE /external:W0 `
  /external:templates- /analyze /analyze:only /analyze:external- `
  /analyze:logbuild/analysis/standalone.xml /FC /c `
  /Fobuild/analysis/standalone.obj src/standalone.cpp
```

An analysis result covers only the MSVC native analyzer's enabled checks for this
translation unit. It does not prove the absence of runtime races, device-driver
failures, or presentation defects; the separate automated and manual verification
records cover those concerns within their stated scope.
