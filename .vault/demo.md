# demo.md

This document demonstrates how the Vault DSL is used in practice. The examples focus on intent, readability, and safety rather than implementation details.

## Basic optional vault usage

```text
vault? cache
  if missing registry -> "token"
    store "token" = generate()
  secure
```

This example describes an optional cache vault. If the vault exists and the registry does not contain a token, a new token is generated and stored. The vault is then secured, which activates encryption and seals the vault.

## Required vault with guaranteed entry

```text
vault session
  if missing registry -> "user_id"
    store "user_id" = currentUser()
  secure
```

This example ensures that a session vault exists. If the user identifier is missing, it is stored securely. Once secured, the vault becomes encrypted and closed to structural changes.

## Lookup without mutation

```text
vault? cache
  if registry -> "token" present
    log "token exists"
```

This example demonstrates state evaluation without exposing or extracting any values. The arrow lookup is used only to reason about presence.

## Intent driven replacement

```text
vault preferences
  replace "theme" = "dark"
  secure
```

This example replaces a stored preference. Replacement is safe by design because all data is encrypted implicitly and never exposed.

