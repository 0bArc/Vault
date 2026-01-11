A vault is a data focused container whose sole purpose is to hold information safely and descriptively, where safety is implied by structure rather than by user managed controls. A vault defines an isolated domain of storage and no operation performed inside a vault may affect data outside of it.

A vault may be referenced before it is secured, but a vault is not considered closed, protected, or encrypted until the `.secure` directive is applied. Until a vault is secured, it may be declared and inspected for structure or intent, but it must not be treated as protected storage. Once `.secure` is applied, the vault becomes sealed, encryption is activated, and its structure may no longer be altered.

All data stored inside a secured vault is encrypted implicitly. The user never provides keys, selects algorithms, or manages encryption state. Encryption is an invariant of the language and cannot be disabled or bypassed.

A vault may be required or optional. A required vault is declared without a suffix and is created if it does not already exist. An optional vault is declared with the `?` suffix and is accessed only if it exists, without producing errors when it does not.

All operations inside a vault are scoped strictly to that vault. The vault acts as the boundary of authority, meaning that mutation, lookup, and evaluation are only valid within its context.

Within a vault, data may be organized using registries. A registry is a logical namespace that exists only inside a vault and inherits all of the vault’s security and lifecycle guarantees. A registry does not expose data and does not exist independently of the vault.

The arrow operator `->` expresses directional lookup and is used to resolve an entry within a container. An expression such as `registry -> "token"` means that the entry named `"token"` is being resolved inside the registry. The arrow operator never mutates data and never returns raw values; it is used only to evaluate presence or state.

Entries inside a vault are never accessed directly. Instead, they are evaluated by state. Common states include `missing` and `present`. State evaluation reveals nothing about the underlying value and exists only to guide intent driven logic.

Mutation operations such as `store` and `replace` are permitted only inside a vault context. These operations always result in encrypted storage and are considered safe by design. Replacing data does not imply exposure or risk, as previous state remains protected by the vault’s guarantees.

Optional access suppresses errors related to non existence. When a vault or entry is optional, its absence is treated as a valid state rather than a failure.

Direct extraction of raw values, manual encryption, mutation outside a vault context, or exposure of internal storage mechanics are not permitted by the language.

The intent of the Vault DSL is to remove fear from data handling by making safety implicit and intent explicit, ensuring that users describe what should exist or happen without needing to understand how protection is achieved.

Example usage demonstrating intent and flow:

```text
vault? cache
  if missing registry -> "token"
    store "token" = generate()
  secure
```
