// Helpers to build Vault DSL text from simple JS structures.

function renderValue(v) {
  if (v === null || v === undefined) return '""';
  if (typeof v === "string") return `"${v}"`;
  if (typeof v === "number" || typeof v === "boolean") return String(v);
  if (typeof v === "object") return JSON.stringify(v);
  return `"${String(v)}"`;
}

function renderOp(op, indent) {
  const pad = " ".repeat(indent);
  switch (op.type) {
    case "registry":
      return `${pad}registry ${op.name}`;
    case "store": {
      const target = op.registry ? `${op.registry} -> "${op.key}"` : `-> "${op.key}"`;
      return `${pad}store ${target} = ${renderValue(op.value)}`;
    }
    case "replace": {
      const target = op.registry ? `${op.registry} -> "${op.key}"` : `-> "${op.key}"`;
      return `${pad}replace ${target} = ${renderValue(op.value)}`;
    }
    case "ifMissing": {
      const target = op.registry ? `${op.registry} -> "${op.key}"` : `-> "${op.key}"`;
      const inner = (op.body || []).map(b => renderOp(b, indent + 2)).join("\n");
      return `${pad}if missing ${target}\n${inner ? inner : pad + "  note \"missing\""}`;
    }
    case "ifPresent": {
      const target = op.registry ? `${op.registry} -> "${op.key}"` : `-> "${op.key}"`;
      const inner = (op.body || []).map(b => renderOp(b, indent + 2)).join("\n");
      return `${pad}if present ${target}\n${inner ? inner : pad + "  note \"present\""}`;
    }
    case "note":
      return `${pad}note "${op.text || ""}"`;
    case "secure":
      return `${pad}secure`;
    default:
      throw new Error(`Unknown op type: ${op.type}`);
  }
}

function renderVault(vault) {
  const header = vault.optional ? `vault? ${vault.name}` : `vault ${vault.name}`;
  const body = (vault.ops || []).map(op => renderOp(op, 2)).join("\n");
  return body ? `${header}\n${body}${vault.secure === false ? "" : "\n  secure"}` : header;
}

function buildProgram(vaults) {
  return vaults.map(renderVault).join("\n\n");
}

module.exports = { buildProgram };
