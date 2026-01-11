// Lightweight Node wrapper around vault.exe
const { execFile } = require("child_process");
const path = require("path");
const { resolveVaultBin, workspaceRoot } = require("./design");

const VAULT_BIN = resolveVaultBin();

function runVault(args, options = {}) {
  return new Promise((resolve, reject) => {
    execFile(VAULT_BIN, args, { cwd: options.cwd || workspaceRoot() }, (err, stdout, stderr) => {
      if (err) {
        const error = new Error(stderr || err.message);
        error.code = err.code;
        return reject(error);
      }
      resolve({ stdout, stderr });
    });
  });
}

async function compileVau(inputVau, outputSvau, extraLoads = []) {
  const args = [inputVau, "--out", outputSvau, ...extraLoads.flatMap(d => ["--load", d])];
  return runVault(args);
}

async function inspectArchive(archivePath, { hideMac = false } = {}) {
  const args = [archivePath];
  if (hideMac) args.push("--hide-mac");
  return runVault(args);
}

async function main() {
  const [cmd, ...rest] = process.argv.slice(2);
  if (!cmd || ["-h", "--help"].includes(cmd)) {
    console.log("Usage:\n  node vault.js compile <in.vau> <out.svau> [--load dep1.svau ...]\n  node vault.js inspect <archive.svau> [--hide-mac]");
    return;
  }

  try {
    if (cmd === "compile") {
      if (rest.length < 2) throw new Error("compile requires <in.vau> <out.svau>");
      const [input, output, ...loadArgs] = rest;
      const loads = [];
      for (let i = 0; i < loadArgs.length; i++) {
        if (loadArgs[i] === "--load" && loadArgs[i + 1]) {
          loads.push(loadArgs[i + 1]);
          i++;
        }
      }
      await compileVau(input, output, loads);
      console.log(`Wrote ${output}`);
    } else if (cmd === "inspect") {
      if (rest.length < 1) throw new Error("inspect requires <archive.svau>");
      const archive = rest[0];
      const hideMac = rest.includes("--hide-mac");
      const { stdout } = await inspectArchive(archive, { hideMac });
      process.stdout.write(stdout);
    } else {
      throw new Error(`Unknown command: ${cmd}`);
    }
  } catch (err) {
    console.error("Error:", err.message || err);
    process.exitCode = 1;
  }
}

if (require.main === module) {
  main();
}

module.exports = { compileVau, inspectArchive, runVault };
