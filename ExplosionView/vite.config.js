import { defineConfig } from "vite";
import { resolve } from 'path';
//use tailwind because gemini loves it lol lol lol
import tailwindcss from "@tailwindcss/vite";

export default defineConfig({
  server: {
    port: 9002,
  },
  base: "",
  build: {
    rollupOptions: {
      input: {
        main: resolve(__dirname, "index.html"),
        help: resolve(__dirname, "help.html"),
      },
      output: {
        entryFileNames: "[name].js",
        // For asynchronously loaded chunks
        chunkFileNames: "[name]-c.js",
        // For other assets (images, CSS, etc.)
        assetFileNames: "assets/[name]-[extname]",
      },
    },
  },
  plugins: [tailwindcss()],
});
