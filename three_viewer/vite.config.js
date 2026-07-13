import { defineConfig } from "vite";

export default defineConfig({
  server: {
    open: true,
    port: 9001,
  },
  build: {
    target: "esnext",
  },
});
