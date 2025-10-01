import { defineConfig } from "vite";
//use tailwind because gemini loves it lol lol lol
import tailwindcss from '@tailwindcss/vite'

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
        chunkFileNames: "[name]-c.js", 
        // For other assets (images, CSS, etc.)
        assetFileNames: "assets/[name]-[extname]",
      },
    },
  },
  plugins: [
    tailwindcss(),
  ],
});
