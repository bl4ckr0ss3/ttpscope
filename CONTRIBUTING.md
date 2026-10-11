# Contributing

Thank you for helping make TTPscope a reliable research tool.

1. Discuss substantial behavior changes in an issue first.
2. Keep the core in portable C17 and avoid unnecessary dependencies.
3. Add tests for parsers, data transformations, and regressions.
4. Run `cmake --build build` and `ctest --test-dir build --output-on-failure`.
5. Preserve ATT&CK citations and object provenance; do not silently rewrite
   source material.

Use focused commits and explain the research workflow improved by a change.
