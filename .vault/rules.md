
This document defines the rules and guarantees of the Vault DSL. These rules are normative and must be preserved by all implementations.

## Vault and security

A vault represents an isolated data domain. A vault is not considered protected until the `.secure` directive is applied. Applying `.secure` seals the vault, activates encryption, and prevents further structural modification.

## Optional and required vaults

A vault declared without a suffix is required and is created if absent. A vault declared with the `?` suffix is optional and is accessed only if it exists. Optional vaults suppress errors related to non existence.

## Scope and isolation

All operations inside a vault are scoped to that vault. No operation may read from or write to data outside the active vault context.

## Registries

Registries are logical namespaces inside a vault. They exist only to organize entries and inherit all security guarantees of the enclosing vault.

## Arrow lookup

The arrow operator `->` performs directional lookup. It resolves a key inside a container. Arrow lookup never mutates state and never exposes raw values.

## State based access

Entries are evaluated only by state, such as `missing` or `present`. State evaluation does not reveal underlying data.

## Mutation rules

Mutation operations such as `store` and `replace` are permitted only inside a vault context. All mutation results in encrypted storage.

## Prohibited actions

The language does not permit direct value extraction, manual encryption, mutation outside a vault context, or exposure of internal storage mechanisms.

## Language intent

The Vault DSL exists to make safe data handling the default by enforcing structure, clarity, and intent driven logic rather than low level control.