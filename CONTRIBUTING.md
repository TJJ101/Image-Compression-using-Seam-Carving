# Contributing

- `main` is protected: work on a branch (`feat/<topic>`, e.g. `feat/greedy-seam`), open a PR, get one teammate review, then merge.
- Keep PRs small and tied to an assignment item. CI (Ubuntu + Windows) must be green.
- Each algorithm has its own file and a shared signature in `include/seamcarve/seam_finders.hpp`, so people rarely touch the same file. Discuss before changing a shared header.
- Do **not** use OpenCV's content-aware resizing functions. OpenCV is for I/O, matrices and (optionally) Sobel only.
- Follow the coding conventions below. Format with `clang-format` (config in `.clang-format`).
- Log every use of AI tools (what for + main prompts) in `docs/ai-usage.md` as you go — it is required in the report.
- Don't commit large images. Put small sample inputs in `data/input/`; `data/output/` is git-ignored.
- The project quiz is based on the submitted code: make sure you can explain everything you merge.

## Coding conventions

| What | Convention | Example |
|---|---|---|
| Functions | `PascalCase` | `ComputeEnergy`, `FindSeamDP` |
| Variables, parameters, members | `camelCase` | `targetWidth`, `carveOpts` |
| Enum values | `ALL_CAPS` with underscores (enum type names stay `PascalCase`) | `Algorithm::GRAPH_CUT` |
| Initialization | Curly braces, with a space inside, instead of `=` | `int count{ 0 };`, `const cv::Mat image{ cv::imread(path) };` |
| Namespaces | Everything inside `namespace seamcarve`, and the contents are indented | see any file in `include/seamcarve/` |
| Comments on functions | Doxygen blocks, with the `*` aligned under the `/` of `/**` | see below |

Doxygen comment style (put it on the declaration in the header):

```cpp
/**
* @brief Converts an Algorithm value into its command-line name.
* @param[in] algo	- Algorithm value.
* @return "dp", "greedy" or "graphcut".
*/
std::string ToString(Algorithm algo);
```

Use `@param[in] name<tab>- description` for each parameter, plus `@return` and `@throws` where they apply.

`.clang-format` already sets the namespace indentation and the `{ x }` brace spacing. It may move the
Doxygen `*` lines one space to the right, so check with `clang-format --dry-run <file>` before formatting
a file that has Doxygen comments.
