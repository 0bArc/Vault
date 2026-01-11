# Vault Data Language (VDL)

A lightweight, intent-first data language for describing secure, scoped storage. Files use the `.vau` extension.

## Core principles
- Safety is implicit: anything stored is considered sealed; encryption is automatic and not user-managed.
- Scope is explicit: everything happens inside a `vault` block; nothing inside can mutate outside data.
- State-first access: lookups only reveal state (`missing` / `present`), never raw values.
- Registries provide namespacing within a vault; they do not exist outside it.

## Syntax (concise grammar)
```
program        ::= (vault_block)*
vault_block    ::= vault_header NEWLINE indent statement* 'secure'
vault_header   ::= 'vault' ['?'] IDENT
statement      ::= registry_stmt
                 | if_stmt
                 | mutation_stmt
                 | note_stmt
registry_stmt  ::= 'registry' IDENT
if_stmt        ::= 'if' state target NEWLINE indent statement*
state          ::= 'missing' | 'present'
target         ::= (IDENT '->')? STRING      # optional registry qualifier, else current registry
mutation_stmt  ::= ('store' | 'replace') target '=' value_expr
value_expr     ::= STRING | IDENT '(' ')'    # literal or built-in generator call
note_stmt      ::= 'note' STRING             # for logging intent without leaking data
STRING         ::= quoted string ("...")
IDENT          ::= /[A-Za-z_][A-Za-z0-9_]*/
```
- Indentation defines block structure (2 spaces recommended). Tabs are not allowed.
- `vault?` is optional; it runs only if the vault exists and suppresses absence errors.
- `secure` seals the vault; no structural changes may follow inside that block.

## Semantics
- `vault name`: required vault; create if absent. `vault? name`: optional; skip block if absent.
- `registry r`: select registry `r` within the current vault (creates if absent before `secure`).
- `target` resolution uses `registry -> "key"` or just `-> "key"` (uses current registry).
- `if missing target`: executes body when key is absent. `if present target`: executes body when key exists.
- `store target = value`: write only when key is absent (fails if present). `replace target = value`: overwrite regardless.
- `value_expr`: either a literal string or a zero-arg built-in (e.g., `generate()` for opaque tokens).
- `note`: emit intent to logs; never exposes stored values.
- `secure`: seals and encrypts; after sealing no further `registry`, `store`, or `replace` are allowed in that vault block.

## Built-in value providers
- `generate()`: produce a fresh, opaque token.
- `now()`: capture an ISO-8601 timestamp.

## Example (`example.vau`)
```
vault? cache
  registry session
  if missing session -> "token"
    store session -> "token" = generate()
    note "issued token"
  if present session -> "token"
    note "token already present"
  secure
```

## Error model
- Structural errors (bad indentation, unknown directives, missing `secure`) are fatal.
- Illegal actions (mutation after `secure`, mutation outside a vault) are rejected.
- Optional vaults suppress absence errors; required vaults create when missing.
- Lookup outside an active registry is an error unless a registry qualifier is provided.

## Intent of the prototype
This spec is intentionally small so you can iterate quickly. The prototype compiler/interpreter will:
- Parse a `.vau` file into vault blocks and statements.
- Simulate secure storage in-memory (opaque values prefixed `ENC[`…`]`).
- Enforce the safety rules above and log actions so you can validate intent.
