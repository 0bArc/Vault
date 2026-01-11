const fs = require("fs");
const path = require("path");
const { buildProgram } = require("./structure");
const { compileVau, inspectArchive } = require("./vault");
const { ensureDir, workspaceRoot } = require("./design");

async function writeVau(model, outPath) {
  const text = buildProgram(model);
  ensureDir(path.dirname(outPath));
  fs.writeFileSync(outPath, text, "utf8");
  return outPath;
}

async function buildArchive(model, { vauPath, svauPath, loads = [] } = {}) {
  const root = workspaceRoot();
  const vau = vauPath || path.join(root, "build", "model.vau");
  const svau = svauPath || path.join(root, "build", "model.svau");
  await writeVau(model, vau);
  await compileVau(vau, svau, loads);
  return { vau, svau };
}

async function buildAndInspect(model, options = {}) {
  const { svau } = await buildArchive(model, options);
  const { stdout } = await inspectArchive(svau, { hideMac: options.hideMac });
  return { svau, view: stdout };
}

module.exports = {
  writeVau,
  buildArchive,
  buildAndInspect,
};
