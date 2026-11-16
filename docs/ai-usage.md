# AI Usage Declaration (log)

Required by the assignment: specify how AI tools were used (design, implementation, report writing) and the main prompts. Append an entry per use.

| Date | Member | Tool | Used for | Main prompt(s) |
|---|---|---|---|---|
| 2026-11-16 | Tan Jun Jie | Claude Code | Project scaffolding: CMake CLI skeleton (no algorithm implementation), CI, repo hygiene files | "Help with the initial set up of the GitHub repo / CMake CLI app; don't do any implementation" |
| 2026-11-16 | Tan Jun Jie | Claude Code | Makefile for Linux/WSL (vcpkg-based build, run, test, clean, rebuild) and `build.bat` for Windows (Visual Studio 2026 project, GUI-only app); diagnosing vcpkg and Visual Studio setup failures | "I'm going to build it on WSL, I want to run the make commands from the repo root", "make a bat file on windows that directly builds it into a gui exe", "the build.bat should build a Visual Studio project... use the latest Visual Studio version" |
| 2026-11-16 | Tan Jun Jie | Claude Code | Dear ImGui + GLFW GUI scaffolding (window, image loading, size sliders, Carve button) merged into the single `seamcarve` executable with the CLI; CMake options for GUI and GUI-only builds | "I want to use ImGui + GLFW for a user interface to hit the bonus mark requirement. Help me to set it up", "instead of building 2 different exe... build one" |