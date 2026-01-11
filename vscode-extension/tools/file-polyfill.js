// Minimal File polyfill for Node 18 to satisfy undici/vsce requirements.
// Uses Blob from Node 18's built-in implementation.
if (typeof File === "undefined") {
  globalThis.File = class File extends Blob {
    constructor(parts = [], name = "", options = {}) {
      super(parts, options);
      this.name = String(name);
      this.lastModified = options.lastModified ?? Date.now();
    }
  };
}
