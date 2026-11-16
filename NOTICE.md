# Third-party notices

Verbatim is licensed under the [GNU Affero General Public License v3.0](LICENSE). It includes or builds on the
following third-party software and fonts, each under its own licence.

| Component | Used for | Licence |
|---|---|---|
| [JUCE](https://github.com/juce-framework/JUCE) 8.0.17 | plugin framework, DSP classes (reverb, chorus, phaser, compressor, oversampling), WebView editor | AGPL-3.0 (JUCE is dual-licensed; Verbatim uses it under the AGPL) |
| [VST3 SDK](https://github.com/steinbergmedia/vst3sdk) (through JUCE) | VST3 plugin interface | as distributed with JUCE (GPL-3.0 option) |
| [xxHash](https://github.com/Cyan4973/xxHash) 0.8.4 | XXH64 for seeds and chain hashes | BSD-2-Clause |
| [Microsoft WebView2 SDK](https://www.nuget.org/packages/Microsoft.Web.WebView2) 1.0.3485.44 | the editor's web view (statically linked loader) | BSD-3-Clause |
| [React](https://react.dev) 18.3.1 | editor UI | MIT |
| [framer-motion](https://github.com/motiondivision/motion) 11.18.2 | editor motion | MIT |
| [Phosphor Icons](https://phosphoricons.com) 2.1.7 | icons | MIT |
| [Courier Prime](https://github.com/quoteunquoteapps/CourierPrime) | typed text | SIL Open Font License 1.1 |
| [Archivo](https://github.com/Omnibus-Type/Archivo) | interface text | SIL Open Font License 1.1 |
| [IBM Plex Mono](https://github.com/IBM/plex) | postmark and values | SIL Open Font License 1.1 |

Test-only dependencies (not shipped): [Catch2](https://github.com/catchorg/Catch2) (BSL-1.0),
[Playwright](https://playwright.dev) (Apache-2.0), [axe-core](https://github.com/dequelabs/axe-core) (MPL-2.0),
[Vite](https://vitejs.dev) and [TypeScript](https://www.typescriptlang.org) (MIT, Apache-2.0).
