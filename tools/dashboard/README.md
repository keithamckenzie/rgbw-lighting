# RGBW Lighting Dashboard

Desktop dashboard for managing, configuring, building, and uploading the rgbw-lighting project's apps to ESP32/ESP8266/AVR hardware.

## Prerequisites

1. **Node.js** (v18+)
2. **pnpm** (v10+)
3. **Rust** (latest stable)
4. **PlatformIO** installed in the project venv:
   ```bash
   # From repo root
   uv venv .venv
   uv pip install platformio --python .venv/bin/python
   ```

## Development

```bash
# Install dependencies
pnpm install

# Run in development mode (Tauri + Vite)
pnpm run tauri:dev
```

If you want Git hooks enabled, run:

```bash
pnpm run prepare
```

## Building

```bash
# Build for production
pnpm run tauri:build
```

The built application will be in `src-tauri/target/release/bundle/`.

## Technology Stack

- **Desktop**: Tauri 2.0 (Rust backend)
- **Frontend**: SvelteKit + TypeScript
- **Styling**: Tailwind CSS
- **Serial**: `serialport` crate

## Features

- App discovery and selection
- Dynamic configuration from config.h
- Pin validation with safety checks
- Build with streaming output
- Upload to hardware
- Serial monitor
- Configuration profiles

## Architecture

```
src/                    # SvelteKit frontend
  lib/
    components/         # Svelte components
    stores/             # State management
    types/              # TypeScript types
    validation/         # Pin validation
  routes/               # SvelteKit routes

src-tauri/              # Rust backend
  src/
    commands/           # Tauri commands
    utils/              # Utilities
```

## Troubleshooting

- If `pio` is not found, run the PlatformIO setup commands from the repo root:
  - `uv venv .venv`
  - `uv pip install platformio --python .venv/bin/python`
- If the app fails to load, re-run `pnpm install` and then `pnpm run tauri:dev`.
- For serial connection issues, click "Refresh ports" and confirm the device appears.

## Contributing

- Run `pnpm run check` and `pnpm run lint` before committing.
- Format changes with `pnpm run format`.
