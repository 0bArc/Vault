# Vault

Secure, intent-first vault data language and toolchain. Vault lets you describe sealed storage with a small DSL and ship it as authenticated archives.

![status: experimental](https://img.shields.io/badge/status-experimental-orange)
![lang: c++17](https://img.shields.io/badge/lang-C%2B%2B17-1f425f)
![vs code](https://img.shields.io/badge/editor-VS%20Code-007ACC)

## What it is
- **Vault DSL (VDL):** minimal language for scoped, sealed data. Everything happens inside a `vault` block; registries namespace keys; `store`/`replace` mutate; `secure` seals.
- **vaultc:** C++17 compiler/interpreter that builds sealed `.svau` archives from `.vau` scripts and can read/query them.
- **vault:** slim wrapper so you can run `vault file.vau` directly using the same entrypoint.
- **vaultdepend:** helper that prints `depends` lines from an archive.
- **VS Code extension:** syntax coloring and snippets for `.vau`, `.svau`, and `.vsc` files.

## Example
```vault
vault? cache
   registry session
   if missing session -> "token"
      store session -> "token" = generate()
      note "issued token"
   if present session -> "token"
      replace session -> "refreshed_at" = now()
      note "token refreshed"
   secure
```

## Running it
1) Configure and build (CMake 3.16+, C++17 toolchain):
```sh
cmake -S . -B build
cmake --build build
```
2) Provide secrets in `.vault/var.vc` next to your sources:
```
MASTER_KEY=...
TOKEN=...
```
3) Compile a script to a sealed archive:
```sh
build/vaultc src/examples/depends_test.vau --out build/depends_test.svau
```
4) Inspect or query:
```sh
build/vaultc build/depends_test.svau --hide-mac
build/vaultc src/examples/secret.vsc --load build/depends_test.svau
```

## VS Code
Package the language extension locally:
```sh
cd vscode-extension
npx @vscode/vsce package
code --install-extension *.vsix
```

## Notes
- Archives are HMAC-checked with your token/master key; mismatches fail fast.
- Optional vaults can be materialized with runtime flags; experimental surface may change.

