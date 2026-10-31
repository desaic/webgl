import { defineConfig } from "vite";

export default defineConfig({
  server: { port: 9006, host: true },
  // relative paths so the build works from any sub folder (GitHub Pages)
  base: "",
  worker: { format: "es" },
  build: { target: "es2020" },
});
