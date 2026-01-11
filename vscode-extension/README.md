# Vault DSL VS Code Support

Syntax highlighting and keyword/snippet completions for `.vau`, `.svau`, and `.vsc` files.

## Installing from source
1. Install the VS Code extension tooling: `npm install -g @vscode/vsce` (or run `npx @vscode/vsce package`).
2. From this folder (`vscode-extension`), run `vsce package` to produce a `.vsix` file.
3. Install the generated package with `code --install-extension <file>.vsix` (or via the VS Code Extensions view: "Install from VSIX...").

## Features
- Language registration for Vault scripts (`.vau`), sealed archives (`.svau`), and query scripts (`.vsc`).
- TextMate grammar with keywords, booleans, numbers, hex digests/HMACs, operators, and comments highlighted.
- Basic language configuration (comment token, brackets, auto-close pairs).
- Keyword/snippet completions for common Vault constructs (vault/registry/store/replace/document literals, script `for`/`log`).

## Notes
- No runtime activation code is required; this extension only contributes language metadata and syntax coloring.
- Update `package.json` publisher/name/version before publishing to a registry.
