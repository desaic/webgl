import { defineConfig } from "vite";

export default defineConfig({
  server: {
    port: 9002,
  },
  base: '',
  build: {
    rollupOptions: {
      output: {
        entryFileNames: "[name].js", 
        // For asynchronously loaded chunks
        chunkFileNames: "[name]-.js", 
        // For other assets (images, CSS, etc.)
        assetFileNames: "assets/[name]-[extname]",
      },
    },
  },
});
