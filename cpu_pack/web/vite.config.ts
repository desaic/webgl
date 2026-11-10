import { defineConfig, Plugin } from "vite";
import fs from "node:fs";
import path from "node:path";

// Local file access for the viewer. Only paths under ROOT are served.
// Override with: MESHES_ROOT=D:/data npm run dev
const ROOT = path.resolve(process.env.MESHES_ROOT || "F:/meshes");

function inRoot(p: string): boolean {
  const rel = path.relative(ROOT, p);
  return rel === "" || (!rel.startsWith("..") && !path.isAbsolute(rel));
}

function fwd(p: string): string {
  return p.replace(/\\/g, "/");
}

function fsApi(): Plugin {
  const handler = (req: any, res: any, next: any) => {
    const url = new URL(req.url, "http://x");
    if (!url.pathname.startsWith("/api/")) return next();
    const fail = (code: number, msg: string) => {
      res.statusCode = code;
      res.end(msg);
    };
    const p = path.resolve(url.searchParams.get("path") || ROOT);
    if (!inRoot(p)) return fail(403, "outside root " + fwd(ROOT));
    try {
      if (url.pathname === "/api/list") {
        const entries = fs.readdirSync(p, { withFileTypes: true }).map((d) => {
          let size = 0;
          let mtime = 0;
          try {
            const st = fs.statSync(path.join(p, d.name));
            size = st.size;
            mtime = st.mtimeMs;
          } catch {}
          return { name: d.name, dir: d.isDirectory(), size, mtime };
        });
        const up = path.dirname(p);
        const parent = p !== ROOT && inRoot(up) ? fwd(up) : null;
        res.setHeader("Content-Type", "application/json");
        res.end(JSON.stringify({ path: fwd(p), root: fwd(ROOT), parent, entries }));
      } else if (url.pathname === "/api/file") {
        res.setHeader("Content-Type", "application/octet-stream");
        res.setHeader("Cache-Control", "no-cache");
        fs.createReadStream(p)
          .on("error", () => fail(404, "not found"))
          .pipe(res);
      } else if (url.pathname === "/api/mtime") {
        res.setHeader("Content-Type", "text/plain");
        res.end(String(fs.statSync(p).mtimeMs));
      } else {
        fail(404, "unknown api");
      }
    } catch (e: any) {
      fail(404, String(e.message || e));
    }
  };
  return {
    name: "fs-api",
    configureServer(s) {
      s.middlewares.use(handler);
    },
    configurePreviewServer(s) {
      s.middlewares.use(handler);
    },
  };
}

export default defineConfig({
  plugins: [fsApi()],
  server: { port: Number(process.env.PORT) || 9007, host: "127.0.0.1" },
  base: "",
  build: { target: "es2020" },
});
