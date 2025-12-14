# Contributing

Thanks for helping improve VHS-Decode! This guide highlights a few key pointers to get you started.

## Development Basics
- Use feature branches and clear commit messages.
- Keep changes focused and small; avoid unrelated edits.
- Run relevant tests locally before opening a PR.

## GPU Acceleration Guidance
- Follow the project’s GPU implementation standards in [.github/copilot-instructions.md](.github/copilot-instructions.md).
- Requirements include test-first development, CPU fallback, precision validation, memory management, and continuous benchmarking.
- New GPU functions must include unit, integration, regression, and performance tests.

## Testing
- Prefer `pytest`; add targeted unit tests near the code you change.
- If you add GPU code, mark tests appropriately (e.g., `@pytest.mark.gpu`) and include performance checks.

## Opening Pull Requests
- Describe the change, rationale, and any trade-offs.
- Link to related issues and mention any follow-ups needed.
- Include a brief summary of test coverage and performance results if applicable.

## Code of Conduct
- Be respectful and collaborative. Report issues constructively.

## Questions
- See the [Wiki](https://github.com/oyvindln/vhs-decode/wiki) for detailed documentation.
- Open a discussion/issue if something is unclear.