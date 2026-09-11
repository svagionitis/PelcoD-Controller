---
name: doxygen-docs
description: >-
  Use this skill when documenting C++ code, classes, structs, methods, or free functions with Doxygen templates.
---

# Doxygen Documentation Requirements

Every class, struct, interface, method, and free function must be fully documented using Doxygen structural tags before generation. Use the `///` triple-slash comment style.

## Function Template

```cpp
/// @brief Brief description of what the function does.
/// @details Detailed explanation of the logic, pre-conditions, and post-conditions.
/// @param[in] param_name Description of input parameter.
/// @param[out] param_name Description of output parameter (if passed by reference).
/// @return Description of the return value.
/// @note Special design notes or thread-safety guarantees.
/// @see RelatedClass::RelatedMethod
/// @retval VALUE_NAME Description of specific return values (especially for enums/optionals).
```

## Class Template

```cpp
/// @class ClassName
/// @brief Brief description of the class's responsibility.
/// @details Detailed design invariants and thread-safety warnings.
```
