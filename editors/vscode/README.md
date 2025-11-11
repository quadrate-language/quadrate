# Quadrate VSCode Extension

Language support for Quadrate in Visual Studio Code.

## Features

- **Syntax Highlighting**: Full syntax highlighting for `.qd` files
- **Language Server**: Auto-completion, diagnostics, and formatting via LSP
- **Smart Editing**: Bracket matching, auto-closing pairs, comment toggling

## Prerequisites

1. **Install quadlsp**: The Quadrate Language Server must be installed and in your PATH

```bash
# From quadrate repo root
make install
```

This installs `quadlsp` to `/usr/local/bin/quadlsp` (or `$PREFIX/bin/quadlsp`).

2. **Verify Installation**:

```bash
which quadlsp
# Should output: /usr/local/bin/quadlsp (or similar)
```

## Installation

### Method 1: Development Installation (Recommended for Testing)

1. **Install Dependencies**:

```bash
cd editors/vscode
npm install
```

2. **Compile TypeScript**:

```bash
npm run compile
```

3. **Install Extension in VSCode**:

```bash
# Create symlink to VSCode extensions directory
ln -s "$(pwd)" ~/.vscode/extensions/quadrate-0.1.0

# Or for VSCodium
ln -s "$(pwd)" ~/.vscode-oss/extensions/quadrate-0.1.0
```

4. **Reload VSCode**: Press `Ctrl+Shift+P` → "Developer: Reload Window"

### Method 2: Package and Install

```bash
cd editors/vscode
npm install
npm run compile

# Install vsce (VSCode Extension Manager)
npm install -g @vscode/vsce

# Package the extension
vsce package

# Install the .vsix file
code --install-extension quadrate-0.1.0.vsix
```

## Testing the Extension

### 1. Create a Test File

Create a file named `test.qd`:

```quadrate
// Test file for Quadrate LSP
fn fibonacci( n:int -- result:int ) {
    n 0 eq if {
        0
    } else {
        n 1 eq if {
            1
        } else {
            n 1 sub fibonacci
            n 2 sub fibonacci
            add
        }
    }
}

fn main() {
    10 fibonacci print
}
```

### 2. Verify Syntax Highlighting

Open `test.qd` in VSCode. You should see:
- Keywords (`fn`, `if`, `else`) highlighted in purple/blue
- Built-in functions (`add`, `sub`, `eq`, `print`) highlighted differently
- Comments in green/gray
- Numbers highlighted
- Strings highlighted

### 3. Test Auto-Completion

1. Open `test.qd`
2. Start typing on a new line: `ad`
3. Press `Ctrl+Space`
4. You should see completion suggestions including `add`

Expected completions:
- `add` - Add two numbers
- `abs` - Absolute value
- `and` - Logical AND
- etc.

### 4. Test Diagnostics

1. Create an error by typing invalid syntax:

```quadrate
fn broken() {
    undefined_function
}
```

2. Save the file
3. You should see red squiggly lines under the error
4. Hover over the error to see the diagnostic message: "Undefined function 'undefined_function'"

### 5. Test Formatting

1. Open a `.qd` file
2. Press `Shift+Alt+F` (or right-click → "Format Document")
3. The document should be formatted (currently a stub, so may not change much)

### 6. Verify LSP Connection

Check the Output panel:
1. Press `Ctrl+Shift+U` (View → Output)
2. Select "Quadrate Language Server" from the dropdown
3. You should see LSP communication logs (if tracing is enabled)

### 7. Enable Detailed Logging

Add to your VSCode `settings.json`:

```json
{
  "quadrate.lsp.trace": "verbose"
}
```

This will show all LSP messages in the Output panel.

## Configuration

The extension supports these settings (File → Preferences → Settings → search "quadrate"):

- **`quadrate.lsp.path`**: Path to quadlsp executable (default: `quadlsp`)
- **`quadrate.lsp.trace`**: LSP trace level - `off`, `messages`, or `verbose` (default: `off`)

### Custom quadlsp Path

If `quadlsp` is not in your PATH, configure the full path:

```json
{
  "quadrate.lsp.path": "/path/to/quadlsp"
}
```

## Troubleshooting

### LSP Not Working

**Problem**: No auto-completion or diagnostics

**Solutions**:

1. **Check quadlsp is installed**:
   ```bash
   which quadlsp
   quadlsp --version  # Will hang - that's expected, press Ctrl+C
   ```

2. **Check VSCode Output**:
   - View → Output → Select "Quadrate Language Server"
   - Look for error messages

3. **Enable verbose logging**:
   ```json
   {
     "quadrate.lsp.trace": "verbose"
   }
   ```

4. **Restart VSCode**:
   - Press `Ctrl+Shift+P`
   - Type "Developer: Reload Window"

5. **Check file is recognized**:
   - Open a `.qd` file
   - Look at bottom-right corner - should say "Quadrate"
   - If not, click the language indicator and select "Quadrate"

### Syntax Highlighting Not Working

**Problem**: `.qd` files show as plain text

**Solutions**:

1. **Manually set language**:
   - Click language indicator in bottom-right
   - Type "Quadrate" and press Enter

2. **Check file extension**:
   - File must end with `.qd`

3. **Reinstall extension**:
   ```bash
   cd editors/vscode
   npm run compile
   # Re-link or reinstall
   ```

### Extension Not Loading

**Problem**: Extension doesn't appear in Extensions list

**Solutions**:

1. **Check symlink is correct**:
   ```bash
   ls -la ~/.vscode/extensions/ | grep quadrate
   ```

2. **Check for errors**:
   - Help → Toggle Developer Tools → Console
   - Look for extension loading errors

3. **Verify package.json**:
   ```bash
   cd editors/vscode
   cat package.json | grep -E "name|version"
   ```

## Development

### Watch Mode

For active development, use watch mode to auto-recompile on changes:

```bash
cd editors/vscode
npm run watch
```

Keep this running while editing `src/extension.ts`.

### Testing Changes

After making changes:

1. Recompile: `npm run compile` (or use watch mode)
2. Reload VSCode: `Ctrl+Shift+P` → "Developer: Reload Window"
3. Test the feature

### Debugging the Extension

1. Open `editors/vscode` in VSCode
2. Press `F5` to launch Extension Development Host
3. A new VSCode window opens with the extension loaded
4. Open a `.qd` file to test
5. Use Debug Console to see logs

## Testing Checklist

Before submitting changes, verify:

- [ ] Syntax highlighting works for keywords, comments, strings, numbers
- [ ] Auto-completion shows built-in functions
- [ ] Diagnostics show for syntax errors
- [ ] Format document command exists (even if stub)
- [ ] File type auto-detection works for `.qd` files
- [ ] Extension loads without errors in Developer Tools console
- [ ] Settings are configurable in VSCode settings UI
- [ ] Works with both absolute and relative paths to quadlsp

## Files Structure

```
editors/vscode/
├── package.json              # Extension manifest
├── tsconfig.json            # TypeScript configuration
├── language-configuration.json  # Bracket matching, etc.
├── src/
│   └── extension.ts         # LSP client implementation
├── syntaxes/
│   └── quadrate.tmLanguage.json  # Syntax highlighting
└── README.md                # This file
```

## Contributing

Issues and patches welcome at: ~klahr/quadrate@lists.sr.ht

## License

Same as Quadrate project.
