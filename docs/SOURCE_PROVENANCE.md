# Public source provenance

The current update imports the production subset of the maintainer-supplied `PiJava-keyui6-gpuf5-save1-source.zip` (SHA256 `bb1895603d0ca5c85e94e8501fde79399cd435ba4ff077d3e7658a317a94d24e`). The accompanying identity identifies upstream commit `caf1063217bbe38a3644893a40e89dc91228a0cd`; that is an input identity, not a public Git ancestor.

The production C++/Java/shader sources and all 289 ROMFS resources are byte-preserved. The removed mapping-page phone bitmap is absent both from the imported source and from the supplied NRO. No replacement phone image is added. Runtime phone-style skins remain selectable and are distinct from the deleted editor illustration.

Public changes select source/build files and replace release prose, root source manifests and license summaries. The previous public GPL root license and original third-party notices are retained. The generated runtime API classes are necessary application inputs; they are not game/test classes.

Test classes, examples, benchmark/profile inputs, game data, logs, screenshots, raw test/build reports, private CI, SDK archives, saves and keys are excluded. The empty examples placeholder preserves the unchanged build tool's optional scan; the publication does not add a new test data directory. Private build history is not imported.

The current source and assets are described separately from the supplied binary release: the latter keeps its internal keyui5/save1 identifier, while KeyUI6 denotes its final mapping layout.
