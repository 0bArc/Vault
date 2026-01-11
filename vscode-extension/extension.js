const vscode = require("vscode");

/**
 * Provide lightweight keyword/snippet completions for the Vault DSL.
 */
function activate(context) {
  console.log("Vault DSL extension activated");
  const keywords = [
    { label: "vault", detail: "Define a vault block", insertText: "vault ${1:name}\n  registry ${2:registry}\n  store ${2} -> \"${3:key}\" = ${4:value}\n  secure" },
    { label: "vault?", detail: "Optional vault", insertText: "vault? ${1:name}" },
    { label: "registry", detail: "Start a registry block", insertText: "registry ${1:name}" },
    { label: "store", detail: "Store a value", insertText: "store ${1:registry} -> \"${2:key}\" = ${3:value}" },
    { label: "replace", detail: "Replace a value", insertText: "replace ${1:registry} -> \"${2:key}\" = ${3:value}" },
    { label: "if missing", detail: "Conditional missing", insertText: "if missing ${1:registry} -> \"${2:key}\"\n  ${3:store ${1} -> \"${2}\" = ${4:value}}" },
    { label: "if present", detail: "Conditional present", insertText: "if present ${1:registry} -> \"${2:key}\"\n  ${3:replace ${1} -> \"${2}\" = ${4:value}}" },
    { label: "note", detail: "Attach a note", insertText: "note \"${1:text}\"" },
    { label: "secure", detail: "Seal the vault", insertText: "secure" },
    { label: "optional", detail: "Mark vault optional", insertText: "optional" },
    { label: "required", detail: "Mark vault required", insertText: "required" },
    { label: "sealed", detail: "Mark vault sealed", insertText: "sealed true" },
    { label: "document", detail: "Document literal", insertText: "{ ${1:field}: \"${2:value}\" }" },
    { label: "log", detail: "Script logging", insertText: "log(${1:expr})" },
    { label: "for", detail: "Script loop", insertText: "for idx, doc in document:find::matching(\"${1:needle}\"):" },
  ];

  const provider = vscode.languages.registerCompletionItemProvider(
    { language: "vault" },
      const output = vscode.window.createOutputChannel("Vault");

      function getBinaryPath() {
        const cfg = vscode.workspace.getConfiguration("vault");
        const raw = cfg.get("binaryPath", "vault");
        if (!raw) return "vault";
        if (vscode.workspace.workspaceFolders && raw && !raw.match(/^([a-zA-Z]:\\|\\\\|\//)) {
          // resolve relative to workspace root
          const root = vscode.workspace.workspaceFolders[0].uri.fsPath;
          return require("path").join(root, raw);
        }
        return raw;
      }

      async function runVault(fileUri, extraArgs = []) {
        const bin = getBinaryPath();
        const args = [fileUri.fsPath, ...extraArgs];
        output.appendLine(`[vault] ${bin} ${args.join(" ")}`);
        const exec = require("child_process").execFile;
        return new Promise((resolve, reject) => {
          exec(bin, args, { cwd: vscode.workspace.workspaceFolders?.[0]?.uri.fsPath }, (err, stdout, stderr) => {
            if (stdout) output.appendLine(stdout);
            if (stderr) output.appendLine(stderr);
            if (err) reject(err); else resolve();
          });
        });
      }

      async function pickFile() {
        const picked = await vscode.window.showOpenDialog({
          canSelectMany: false,
          filters: { "Vault Files": ["vau", "svau", "vsc"] }
        });
        return picked?.[0];
      }

      async function handleRunCommand(uri) {
        try {
          const fileUri = uri ?? await pickFile();
          if (!fileUri) return;
          await runVault(fileUri);
          vscode.window.showInformationMessage(`Vault ran ${fileUri.fsPath}`);
        } catch (err) {
          vscode.window.showErrorMessage(`Vault run failed: ${err.message || err}`);
        }
      }

      function createPanel() {
        const panel = vscode.window.createWebviewPanel(
          "vaultRun",
          "Vault Runner",
          vscode.ViewColumn.Beside,
          { enableScripts: true, retainContextWhenHidden: true }
        );

        const html = `<!DOCTYPE html>
    <html lang="en">
    <head>
      <meta charset="UTF-8">
      <style>
        body { font-family: Segoe UI, sans-serif; margin: 0; padding: 16px; }
        header { display: flex; align-items: center; gap: 8px; margin-bottom: 12px; }
        button { padding: 6px 12px; border: 1px solid #888; background: #f5f5f5; cursor: pointer; }
        button:hover { background: #e9e9e9; }
        input { width: 100%; padding: 6px; box-sizing: border-box; }
        .row { display: flex; gap: 8px; margin-bottom: 8px; }
        .col { flex: 1; }
        pre { background: #111; color: #e6e6e6; padding: 10px; border-radius: 6px; min-height: 180px; overflow: auto; }
        .badge { font-size: 12px; padding: 2px 6px; border-radius: 10px; background: #007acc; color: white; }
      </style>
    </head>
    <body>
      <header>
        <span class="badge">Vault</span>
        <strong>Interactive Runner</strong>
      </header>
      <div class="row">
        <div class="col">
          <label>File path (.vau / .svau / .vsc)</label>
          <input id="file" placeholder="e.g. src/examples/depends_test.vau" />
        </div>
      </div>
      <div class="row">
        <div class="col"><button id="browse">Browse…</button></div>
        <div class="col"><button id="run">Run</button></div>
      </div>
      <pre id="log">Waiting…</pre>
      <script>
        const vscode = acquireVsCodeApi();
        document.getElementById('browse').onclick = () => {
          vscode.postMessage({ type: 'browse' });
        };
        document.getElementById('run').onclick = () => {
          vscode.postMessage({ type: 'run', file: document.getElementById('file').value });
        };
        window.addEventListener('message', event => {
          const msg = event.data;
          if (msg.type === 'setFile') {
            document.getElementById('file').value = msg.file;
          }
          if (msg.type === 'log') {
            document.getElementById('log').textContent = msg.text;
          }
        });
      </script>
    </body>
    </html>`;

        panel.webview.html = html;

        panel.webview.onDidReceiveMessage(async (msg) => {
          if (msg.type === "browse") {
            const picked = await pickFile();
            if (picked) panel.webview.postMessage({ type: "setFile", file: picked.fsPath });
          }
          if (msg.type === "run") {
            const filePath = msg.file?.trim();
            if (!filePath) {
              vscode.window.showWarningMessage("Select a file first.");
              return;
            }
            try {
              panel.webview.postMessage({ type: "log", text: "Running..." });
              const exec = require("child_process").execFile;
              const bin = getBinaryPath();
              exec(bin, [filePath], { cwd: vscode.workspace.workspaceFolders?.[0]?.uri.fsPath }, (err, stdout, stderr) => {
                const text = [stdout || "", stderr || "", err ? err.message : ""].filter(Boolean).join("\n").trim() || "Done.";
                panel.webview.postMessage({ type: "log", text });
                output.appendLine(text);
                if (err) vscode.window.showErrorMessage(`Vault run failed: ${err.message}`);
              });
            } catch (err) {
              vscode.window.showErrorMessage(`Vault run failed: ${err.message || err}`);
            }
          }
        });
      }

      const runCmd = vscode.commands.registerCommand("vault.run", handleRunCommand);
      const panelCmd = vscode.commands.registerCommand("vault.openPanel", () => createPanel());
    {
      provideCompletionItems(document, position) {
        const range = document.getWordRangeAtPosition(position);
        const current = range ? document.getText(range) : "";
        return keywords.map(k => {
          const item = new vscode.CompletionItem(k.label, vscode.CompletionItemKind.Snippet);
          item.detail = k.detail;
          item.insertText = new vscode.SnippetString(k.insertText);
          item.filterText = k.label;
          item.preselect = true;
          item.sortText = "!" + k.label; // float above word-based suggestions
          item.range = range;
          return item;
        });
      }
    },
    " ", "\n", "\t", ":", "-", ">", "{", "["
  );

  context.subscriptions.push(provider, runCmd, panelCmd, output);
}

function deactivate() {}

module.exports = { activate, deactivate };
