# C++17 Language Rules & RAII

## Memory & Resource Management
- **Strict RAII:** All resources (memory, files, sockets, locks) must be managed by object lifetimes. Raw `new` and `delete` expressions are strictly **forbidden**.
- **Smart Pointers:** Use `std::unique_ptr` by default for exclusive ownership. Use `std::shared_ptr` only when shared ownership is explicitly required.
- **Factory Functions:** Use `std::make_unique<T>()` and `std::make_shared<T>()`. Never pass a raw pointer to a smart pointer constructor.
- **No Raw Pointers for Ownership:** Raw pointers (`T*`) may only be used as non-owning, non-nullable observers. Otherwise, use `std::reference_wrapper`.

## Types and Initialization
- Avoid magic numbers and strings by extracting recurring or meaningful values into descriptive constants (`constexpr`) or enums. Keep self-explanatory, one-off values inline to avoid clutter. If a value comes from a spec, use a constant regardless.
- **No Uninitialized Variables:** Every variable must be initialized immediately upon declaration. Use braced initialization `{}` where possible to prevent narrowing conversions.
- **Fixed-Width Types:** Use types from `<cstdint>` (e.g., `std::int32_t`, `std::uint64_t`) instead of basic types (`int`, `long`) to ensure predictable cross-platform sizing.
- **`auto` Usage:** Use `auto` only when the concrete type is explicitly obvious from the right-hand side (e.g., `auto ptr = std::make_unique<Target>();`) or when dealing with iterators/lambdas.

## Modern C++17 Features
- **Compile-Time Verification:** Use `constexpr` for all functions and constants that can be evaluated at compile time. Use `static_assert` to enforce invariant checks at compile time.
- **Structured Bindings:** Use structured bindings `auto [x, y] = ...` for readable tuple/pair unpacks.
- **Attributes:** Use C++17 standard attributes to communicate intent to the compiler:
  - `[[nodiscard]]` on functions where ignoring the return value is a logical error or safety risk.
  - `[[fallthrough]]` in switch cases where falling through is intended.
  - `[[maybe_unused]]` for variables that might not be used depending on build configurations.
- **Safe Alternatives:** Use `std::string_view` for non-owning read-only string parameters. Use `std::optional` instead of magic values (like `-1` or `nullptr`) to indicate missing data.
