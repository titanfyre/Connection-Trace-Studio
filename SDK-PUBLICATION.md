# SDK publication status

Current source layout and registry coverage. A GitHub download is distinct from publication on a central registry.

| Target | Status | Package / documentation |
| --- | --- | --- |
| CPP/vcpkg | published_custom_registry | connection-trace-studio |
| CPP/conan | published_custom_source_index | [connection-trace-studio](https://github.com/TitanCSA/sdk-packages/tree/089bc73095aa8eda89bacbe10d0b9e58394b0207/conan-index) [Source](packaging/native/) |
| CPP/nuget | paused_pending_owner_email_confirmation | Connection-Trace-Studio.Native |
| NET/nuget | built_verified_publication_on_hold_by_owner | ConnectionTraceStudio.SDK [Source](Connection-Trace-Studio-NET-nuget/) |
| Python/pypi | built_fixture_and_production_verified | connection-trace-studio [Source](Connection-Trace-Studio-Python-pypi/) |
| Python/conda-forge | custom_channel_install_verified_requires_conda_forge_review | [connection-trace-studio](https://raw.githubusercontent.com/TitanCSA/sdk-packages/089bc73095aa8eda89bacbe10d0b9e58394b0207/conda-channel) [Source](packaging/conda/) |
| Rust/cargo | published_install_verified | [connection-trace-studio](https://crates.io/crates/connection-trace-studio) [Source](Connection-Trace-Studio-Rust-cargo/) |
| JavaScript/npm | awaiting_browser_publication_authentication | [connection-trace-studio](https://www.npmjs.com/package/connection-trace-studio) [Source](Connection-Trace-Studio-JavaScript-npm/) |
| Java/maven | published_install_verified | [connection-trace-studio](https://central.sonatype.com/artifact/io.github.titancsa/connection-trace-studio/1.0.0) [Source](Connection-Trace-Studio-Java-maven/) |
| Java/jitpack | published_install_verified | [connection-trace-studio](https://jitpack.io/#titanfyre/Connection-Trace-Studio/java-sdk-v1.0.0) [Source](Connection-Trace-Studio-Java-maven/) |
| Go/modules | published_install_verified | [connection-trace-studio](https://pkg.go.dev/github.com/titanfyre/Connection-Trace-Studio/Connection-Trace-Studio-Go-modules@v1.0.0) [Source](Connection-Trace-Studio-Go-modules/) |
| Go/pkg-go-dev | indexed_verified | [Documentation](https://pkg.go.dev/github.com/titanfyre/Connection-Trace-Studio/Connection-Trace-Studio-Go-modules@v1.0.0) |
| PowerShell/gallery | published_install_verified | [ConnectionTraceStudio](https://www.powershellgallery.com/packages/ConnectionTraceStudio/1.0.0) [Source](Connection-Trace-Studio-PowerShell-gallery/) |
| Dart/pub-dev | built_fixture_and_production_verified | connection_trace_studio [Source](Connection-Trace-Studio-Dart-pub-dev/) |
| Ruby/rubygems | published_install_verified | [connection-trace-studio](https://rubygems.org/gems/connection-trace-studio) [Source](Connection-Trace-Studio-Ruby-rubygems/) |
| PHP/packagist | composer_source_and_tag_published | [titancsa/connection-trace-studio](https://packagist.org/packages/titancsa/connection-trace-studio) |

Native SDK source and CMake/Conan/vcpkg/NuGet recipes share [`packaging/native`](packaging/native/LAYOUT.md). Previous release archives and tags remain unchanged.

Application and driver prerequisites and original licensing remain those documented by each SDK. No driver installer is bundled with these source packages.
