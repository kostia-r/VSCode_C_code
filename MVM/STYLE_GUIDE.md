# Mophun VM Style Guide

This guide adapts the project documents `2.+Naming+conventions.docx` and
`3.+Styling.docx` to the Mophun VM component.

## Component Identity

- Human-readable component name: `MophunVM`.
- C symbol prefix: `MVM`.
- Source/header file prefix: `MVM_`.
- New public symbols must use the `MVM_` prefix.

## Naming

### Functions

Function names follow:

```text
MVM_<ReturnType><PascalCaseName>
MVM_L<ReturnType><PascalCaseName>   // private/static functions
MVM_cbk_<ReturnType><PascalCaseName> // callbacks
```

Return type prefixes:

- `vid` for `void`
- `b` for `bool`
- `u8`, `u16`, `u32`, `u64` for unsigned integers
- `s8`, `s16`, `s32`, `s64` for signed integers
- `udt` for user-defined types or platform/library types such as `size_t`
- `pudt` for user-defined pointers when the distinction is useful

Examples:

```c
MVM_bInit(...)
MVM_vidFree(...)
MVM_udtGetStorageSize(...)
MVM_Lu32ReadOpcode(...)
```

### Variables

Variables use a type prefix and PascalCase name:

```text
u32StepCount
bIsReady
udtVm
pudtStorage
```

Function parameters start with `par_`:

```text
par_pudtVm
par_u32MaxSteps
par_pu8Data_out
```

File-scope variables use `MVM_L` before the type prefix:

```text
MVM_Lu32TraceMask
```

Global variables, if any are unavoidable, use `MVM_` before the type prefix.

### Types

Struct types follow:

```text
MVM_tstr<Name>
```

Enum types follow:

```text
MVM_tenu<Name>
```

Enum values include the enum name or a clear abbreviated prefix.

### Macros

Macros use uppercase names, begin with `MVM_`, and wrap values in parentheses.
Numeric macro values start at column 65. Unsigned numeric literals use an
explicit uppercase `U` suffix.

```c
#define MVM_U32_MAX_STREAMS                                     (16U)
```

### Files

Files begin with `MVM_` and use PascalCase words after the prefix:

```text
MVM_Core.c
MVM_PipExec.c
MVM_RuntimeStreams.c
MVM_Trace.h
```

## Formatting

- Use spaces, never tabs.
- Use 2 spaces per indentation level.
- Maximum line length: 120 characters.
- Braces go on the next line.
- Always use braces, even for single-line statements.
- `if`, `else if`, `else`, `for`, `while`, `switch`, and `case` bodies must
  always be enclosed in `{}`.
- `case` and `default` labels are indented 2 spaces deeper than their
  containing `switch`.
- Use blank lines to separate logical blocks; see `Blank Lines` below.
- Use one variable declaration per line.
- Keep operators surrounded by spaces, except unary operators.
- Each file ends with a newline.

### Blank Lines

- Do not insert a blank line immediately after an opening `{` before the first
  statement.
- Do not insert a blank line between a control statement or `case`/`default`
  label and its opening `{`.
- Do not insert a blank line between a closing `}` and its paired `else` or
  `else if`.
- Insert a blank line after a completed nested block or `if`/`else` chain before
  subsequent non-control continuation statements.
- Insert a blank line between adjacent `typedef` declarations.
- Intentional fall-through `case` labels may be adjacent without blank lines.
- Keep a blank line between completed `case` bodies.

### Closing Comments

Use explicit closing comments for high-level and control-flow blocks:

```c
} /* End of MVM_bInit */
} /* End of case OP_CALLL */
} /* End of switch */
} /* End of loop */
```

- Function definitions must close with `/* End of <FunctionName> */`.
- `switch` blocks must close with `/* End of switch */`.
- Loop blocks must close with `/* End of loop */`.
- `case` bodies must close with `/* End of case <label> */` or
  `/* End of default */`.
- Intentional fall-through label groups may keep multiple `case` labels before a
  single shared `{}` block. The closing comment names the last label in the
  group.

## File Structure

New or heavily rewritten `.c` files must use these sections in this order:

```text
FILE DESCRIPTION
INCLUDES
LOCAL MACROS
LOCAL FUNCTION MACROS
LOCAL DATA TYPES AND STRUCTURES
LOCAL DATA PROTOTYPES
LOCAL CONSTANT DATA
GLOBAL DATA PROTOTYPES
GLOBAL CONSTANT DATA
LOCAL FUNCTIONS PROTOTYPES
GLOBAL FUNCTIONS
LOCAL FUNCTIONS
END OF FILE
```

Empty sections are allowed. Keep the section banner in place so future additions
land in the expected location.

Header files must use these sections in this order:

```text
FILE DESCRIPTION
Header file guard
INCLUDES
GLOBAL MACROS
GLOBAL FUNCTION MACROS
GLOBAL DATA TYPES AND STRUCTURES
GLOBAL DATA PROTOTYPES
GLOBAL CONSTANT DATA
GLOBAL FUNCTIONS PROTOTYPES
GLOBAL INLINE FUNCTIONS      // only when static inline definitions are needed
END of header file guard
END OF FILE
```

Use the same visual banner style for every section:

```c
/**********************************************************************************************************************
 *  SECTION NAME
 *********************************************************************************************************************/
```

Existing files may be migrated incrementally to avoid mixing mechanical style
changes with behavioral changes.

### Function Placement

- Public/global function implementations live in `GLOBAL FUNCTIONS`.
- `static` function implementations live only in `LOCAL FUNCTIONS`, at the end
  of the `.c` file.
- Every `static` function in a `.c` file must have a prototype in
  `LOCAL FUNCTIONS PROTOTYPES`.
- Each `static` prototype must have a short Doxygen-style `@brief` comment:

```c
/**
 * @brief Checks whether the entry point is valid.
 */
static bool MVM_LbValidateEntryPoint(uint32_t par_u32Address);
```

- Keep the long function header comment above the implementation itself. The
  prototype gets only the short `@brief`; the implementation gets the full
  function description block.

### Header Inline Functions

`static inline` functions in headers are treated as local helper functions:

- Put their prototypes in `GLOBAL FUNCTIONS PROTOTYPES`.
- Put their definitions in `GLOBAL INLINE FUNCTIONS`.
- Add a short `@brief` above both the prototype and the inline definition.
- Prefer keeping inline helpers in internal headers. Public headers should avoid
  inline definitions unless the API needs them.

### Include And Macro Sections

- `#include` directives live only in `INCLUDES`.
- Object-like macros live in `LOCAL MACROS` or `GLOBAL MACROS`.
- Function-like macros live in `LOCAL FUNCTION MACROS` or
  `GLOBAL FUNCTION MACROS`.
- Keep include and macro lists compact; do not insert a blank line after every
  directive unless grouping improves readability.

## Migration Policy

- First migrate public headers and source file names.
- Then migrate public APIs.
- Then migrate internal functions module by module.
- Avoid changing behavior during style-only passes.
- Build and smoke test after each migration step.
