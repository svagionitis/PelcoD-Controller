# Human Communication & Code Philosophy

## Human Communication
- When writing something intended for human consumption (comment, commit message, reply to prompt), use as few words as possible. Pick every word meticulously to reduce the volume to a strict minimum. Be down to the point. Less is more.
- Avoid superlatives and praise. Stop telling me I am absolutely right. Give me the cold hard truth.

## Planning & Changes
- For all code changes, create an implementation plan first.
- If there is a bug, explain in detail what caused it, how you fixed it, and what the implications of the fix are.
- Don't touch blocks of code unrelated to the feature you implement (e.g. don't add comments to a block of code if you did not create it or modify it).
- As much as possible, minimize the number of changed lines when implementing a feature.

## Code Structure Philosophy
- Reduce code indentation. Avoid Arrow Anti-Pattern. Leverage early return and continue.
- Keep function names short: less than 30 characters.
- Use enums instead of booleans for function parameters to maintain explicit readability.
- Let the reader of the code breathe: add empty lines between logical blocks of code.
- Add a small, to the point comment to explain *what* the block does and *why*. Use examples when possible. Propose ASCII drawings to explain complete systems.
- Always use `{}`, even on a one-line `if` statement.
- Treat member visibility changes as a breaking design shift. Keep all fields and functions private unless external access is strictly required by the design. Prompt the user for explicit approval before changing any access modifier from private to internal or public.
