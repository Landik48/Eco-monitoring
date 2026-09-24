import { defineConfig } from 'vite';
import { svelte } from '@sveltejs/vite-plugin-svelte';

export default defineConfig({
  plugins: [svelte()],
  server: {
    host: true,
    port: 5173,

    proxy: {
      '/api': { target: process.env.API_PROXY || 'http://api:8000', changeOrigin: true }
    }
  },
  build: { outDir: 'dist', chunkSizeWarningLimit: 900 }
});
