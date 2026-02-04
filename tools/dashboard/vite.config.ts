import { sveltekit } from '@sveltejs/kit/vite';
import { defineConfig } from 'vite';

export default defineConfig({
	plugins: [sveltekit()],
	// Tauri expects a fixed port
	server: {
		port: 5173,
		strictPort: true
	},
	// Prevent vite from obscuring Rust errors
	clearScreen: false,
	// Tauri expects HTML to be served from a relative path
	build: {
		target: 'esnext'
	}
});
