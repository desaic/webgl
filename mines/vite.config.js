import { defineConfig } from "vite";
import { resolve } from 'path';
// tailwind because gemini loves it lol lol lol
import tailwindcss from "@tailwindcss/vite";

export default defineConfig({
  server: {
    port: 9005,
  },
  base: "",
  build: {
    rollupOptions: {
      input: {
        main: resolve(__dirname, "index.html")
      },
      output: {
        entryFileNames: "[name].js",
        // For asynchronously loaded chunks
        chunkFileNames: "[name]-c.js",
        // For other assets (images, CSS, etc.)
        assetFileNames: ({name}) => {
          if (/\.(gif|jpe?g|png|svg)$/.test(name ?? '')){
              return 'assets/images/[name][extname]';
          }
          
          if (/\.css$/.test(name ?? '')) {
              return 'assets/css/[name][extname]';   
          }
 
          // default value
          // ref: https://rollupjs.org/guide/en/#outputassetfilenames
          return 'assets/[name][extname]';
        },
      },
    },
  },
  plugins: [tailwindcss()],
});
