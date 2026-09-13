import path from 'path'
import tailwindcss from '@tailwindcss/vite'
import react, { reactCompilerPreset } from '@vitejs/plugin-react'
import babel from '@rolldown/plugin-babel'
import { defineConfig } from 'vite'

const rawBase = process.env.BASE_PATH || './'
const base = rawBase === './' ? './' : (rawBase.endsWith('/') ? rawBase : `${rawBase}/`)

export default defineConfig({
  base,
  plugins: [
    tailwindcss(),
    react(),
    babel({ presets: [reactCompilerPreset()] })
  ],
  server: {
    fs: {
      allow: ['..'],
    },
  },
  resolve: {
    alias: {
      '@': path.resolve(import.meta.dirname, './src'),
      '@root': path.resolve(import.meta.dirname, '..'),
      '@docs': path.resolve(import.meta.dirname, '../docs'),
    },
  },
})

