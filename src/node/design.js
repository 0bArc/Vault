const path = require("path");
const fs = require("fs");

const DEFAULT_BIN = "C:/Scripts/vault.exe";

function workspaceRoot() {
  return process.cwd();
}

function resolveVaultBin({ override, env } = {}) {
  const e = env || process.env;
  const candidate = override || e.VAULT_BIN || DEFAULT_BIN;
  if (path.isAbsolute(candidate)) return candidate;
  return path.join(workspaceRoot(), candidate);
}

function ensureDir(p) {
  fs.mkdirSync(p, { recursive: true });
}

module.exports = {
  DEFAULT_BIN,
  resolveVaultBin,
  workspaceRoot,
  ensureDir,
};
