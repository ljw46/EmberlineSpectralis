# Code signing policy

> **Status: not active yet.** Releases are currently unsigned. This policy takes effect once the project is approved by
> SignPath Foundation and signing is wired into the release build. Until then Windows may show a SmartScreen warning
> (More info, then Run anyway).

Free code signing for Windows builds of Emberline Spectralis is provided by [SignPath.io](https://signpath.io),
with the certificate issued by [SignPath Foundation](https://signpath.org).

## What gets signed

Only Windows binaries built from this repository by the public GitHub Actions workflow
(`.github/workflows/build.yml`), from a `v*` release tag:

* `Emberline Spectralis.vst3` (the plugin DLL inside the bundle)
* `Emberline Spectralis.exe` (the Standalone app)
* the Windows setup installer

Nothing is signed from a developer's own machine, and nothing outside this repository is signed with this certificate.
The unsigned build artifacts are produced by GitHub Actions, submitted to SignPath from that same workflow run, and the
signed files are published as GitHub release assets.

## Roles

| Role | Who |
|---|---|
| Committer / reviewer | [Liam Walsh](https://github.com/ljw46) |
| Release approver | [Liam Walsh](https://github.com/ljw46) |

The project has a single maintainer, who reviews changes and approves every signing request by hand. Contributions from
others are reviewed and merged by the maintainer before they can be part of a signed release. Accounts with signing
access use multi-factor authentication.

## Privacy

Emberline Spectralis does not collect data and makes no network connections of its own. It opens links (the manual, the
author credit, the support page) in your default browser only when you click them.

## Licence

The source code is [AGPL-3.0](LICENSE). Factory presets, wavetables and artwork are under the
[royalty-free content licence](LICENSE-CONTENT.md).

## Reporting a problem

If you think a signed build is wrong or malicious, [open an issue](https://github.com/ljw46/EmberlineSpectralis/issues)
and I will look at it straight away.
