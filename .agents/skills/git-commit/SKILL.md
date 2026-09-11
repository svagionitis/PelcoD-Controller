---
name: git-commit
description: >-
  Use this skill when composing, reviewing, or generating Git commit messages conforming to Conventional Commits 1.0.0.
---

# Git Commit Instructions

When writing a commit message, follow these 6 strict formatting rules alongside the **Conventional Commits 1.0.0** specification.

---

## General Git Formatting Rules

- **Rule 1:** Separate the subject line from the body with a single blank line.
- **Rule 2:** Limit the subject line to 50 characters (72 is the absolute hard limit).
- **Rule 3:** Do not end the subject line with a period.
- **Rule 4:** Use the imperative mood in the subject line (e.g., "fix bug," "add feature," not "fixed" or "adds"). Test formula: It must complete the sentence: "If applied, this commit will [your subject line here]".
- **Rule 5:** Wrap the body text manually at 72 characters to prevent Git formatting issues.
- **Rule 6:** Use the body to explain what and why vs. how. Assume the code explains the how; the message must explain the context and reasoning.

---

## Conventional Commits 1.0.0 Specification

Commit messages should adhere to the following structural layout:

```text
<type>[optional scope][!]: <description>

[optional body]

[optional footer(s)]
```

### 1. Types

Commits MUST be prefixed with a type designating the intent of the change:

- **`feat`**: Introduces a new feature or functionality to the codebase or application.
- **`fix`**: Patches a bug or regression in the codebase or application.
- **`docs`**: Documentation changes only (e.g., Markdown, docstrings, architectural guides).
- **`style`**: Code formatting, indentation, whitespace, or style changes with no logic alterations.
- **`refactor`**: Code restructuring that neither fixes a bug nor adds a feature.
- **`perf`**: Code modifications that improve execution performance or algorithmic efficiency.
- **`test`**: Adding missing tests, refactoring test suites, or updating unit benchmarks.
- **`build`**: Changes that affect the build system or external dependencies (e.g., CMake, package scripts).
- **`ci`**: Changes to continuous integration / continuous deployment configuration files and scripts.
- **`chore`**: Maintenance, tooling configuration, or miscellaneous housekeeping tasks.
- **`revert`**: Reverts a previous commit.

### 2. Scopes

- An optional scope MAY be added after the type in parentheses to specify the module or subsystem affected: `<type>(<scope>): <description>`
- Examples: `feat(gui):`, `fix(territory-fill):`, `refactor(core):`, `build(cmake):`
- The scope should be concise, lowercase, and noun-based.

### 3. Breaking Changes

- Breaking changes MUST be signaled by either:
  1. A `!` character placed immediately before the colon in the header:
     - Example: `feat(api)!: redesign state machine interface`
     - Example: `refactor!: remove legacy tile coordinate mapping`
  2. A footer entry beginning with `BREAKING CHANGE: ` followed by an explanation of the breaking change and necessary migration steps:
     ```text
     BREAKING CHANGE: The `step()` method signature now requires tick delta in microseconds.
     ```

### 4. Description

- The description immediately follows the colon and space (`: `).
- It provides a succinct summary of the change in the imperative mood.
- Do not end the description with a period (`.`).

### 5. Body

- If present, the body MUST begin one blank line after the header.
- Manually wrap all body lines at 72 characters.
- Explain the motivation, context, and impact of the change.

### 6. Footers

- If present, footers MUST begin one blank line after the body.
- Each footer consists of a word token, followed by either `:<space>` or `<space>#`, followed by a value:
  - Examples: `Reviewed-by: Stavros Vagionitis`, `Fixes: #123`, `Refs: #456`
- Multi-word footer tokens MUST use hyphens instead of spaces (e.g., `Co-authored-by:`), with the exception of `BREAKING CHANGE:`.

---

## Examples

### Feature with Scope
```text
feat(ptz): add hold-to-move directional controller

Implement continuous pan and tilt commands on mouse-press and stop
motion on mouse-release with configurable speed sliders.
```

### Bug Fix with Reference
```text
fix(frame): correct modulo-256 checksum boundary check

Ensure checksum calculation handles 8-bit overflow accurately across
bytes 2 through 6 in 7-byte standard frames.

Fixes: #12
```

### Breaking Change with Footer
```text
refactor(core)!: rename CameraStatus to DeviceStatus

Generalize status telemetry data structure to support pan/tilt heads,
gimbals, receiver/drivers, and motorized domes.

BREAKING CHANGE: `PelcoDDevice::getStatus()` now returns `DeviceStatus`.
```
