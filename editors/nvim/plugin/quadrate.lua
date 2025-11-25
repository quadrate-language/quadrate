-- Quadrate Language Server Protocol and Tree-sitter configuration for Neovim
-- Requires: nvim-lspconfig
-- Optional: nvim-treesitter (for better syntax highlighting)

-- Default configuration
local quadrate_config = {
  lint = {
    enabled = true,
    path = 'quadlint',
  },
  lsp = {
    path = 'quadlsp',
  },
}

-- Allow users to override configuration
-- Usage: require('quadrate').setup({ lint = { enabled = false } })
local M = {}

function M.setup(opts)
  quadrate_config = vim.tbl_deep_extend('force', quadrate_config, opts or {})
end

-- Export module for configuration
_G.quadrate = M

-- Configure tree-sitter parser (if nvim-treesitter is installed)
local treesitter_ok, treesitter_configs = pcall(require, 'nvim-treesitter.configs')
if treesitter_ok then
  local parser_config = require('nvim-treesitter.parsers').get_parser_configs()

  -- Define Quadrate parser
  parser_config.quadrate = {
    install_info = {
      url = vim.fn.expand('~/.config/nvim') .. '/tree-sitter-quadrate',
      files = {'src/parser.c'},
      branch = 'main',
      generate_requires_npm = false,
      requires_generate_from_grammar = true,
    },
    filetype = 'quadrate',
  }

  -- Enable tree-sitter highlighting for Quadrate
  vim.api.nvim_create_autocmd('FileType', {
    pattern = 'quadrate',
    callback = function()
      if vim.fn.exists(':TSBufEnable') > 0 then
        vim.cmd('TSBufEnable highlight')
      end
    end,
  })
end

-- Configure LSP
local lspconfig_ok, lspconfig = pcall(require, 'lspconfig')
if not lspconfig_ok then
  vim.notify('nvim-lspconfig is not installed. Install it to use Quadrate LSP.', vim.log.levels.WARN)
  return
end

local configs = require('lspconfig.configs')

-- Define quadlsp configuration (only if not already defined)
if not configs.quadlsp then
  configs.quadlsp = {
    default_config = {
      cmd = {quadrate_config.lsp.path},
      filetypes = {'quadrate'},
      root_dir = function(fname)
        return lspconfig.util.find_git_ancestor(fname)
               or lspconfig.util.path.dirname(fname)
      end,
      single_file_support = true,
      settings = {},
    },
  }
end

-- Default on_attach function with keybindings
local function on_attach(client, bufnr)
  -- Enable completion
  vim.api.nvim_buf_set_option(bufnr, 'omnifunc', 'v:lua.vim.lsp.omnifunc')

  -- Key mappings
  local opts = { noremap = true, silent = true, buffer = bufnr }

  -- LSP actions
  vim.keymap.set('n', 'gD', vim.lsp.buf.declaration, opts)
  vim.keymap.set('n', 'gd', vim.lsp.buf.definition, opts)
  vim.keymap.set('n', 'K', vim.lsp.buf.hover, opts)
  vim.keymap.set('n', 'gi', vim.lsp.buf.implementation, opts)
  vim.keymap.set('n', '<C-k>', vim.lsp.buf.signature_help, opts)
  vim.keymap.set('n', '<space>wa', vim.lsp.buf.add_workspace_folder, opts)
  vim.keymap.set('n', '<space>wr', vim.lsp.buf.remove_workspace_folder, opts)
  vim.keymap.set('n', '<space>wl', function()
    print(vim.inspect(vim.lsp.buf.list_workspace_folders()))
  end, opts)
  vim.keymap.set('n', '<space>D', vim.lsp.buf.type_definition, opts)
  vim.keymap.set('n', '<space>rn', vim.lsp.buf.rename, opts)
  vim.keymap.set('n', '<space>ca', vim.lsp.buf.code_action, opts)
  vim.keymap.set('n', 'gr', vim.lsp.buf.references, opts)
  vim.keymap.set('n', '<space>f', function()
    vim.lsp.buf.format({ async = true })
  end, opts)

  -- Diagnostics
  vim.keymap.set('n', '<space>e', vim.diagnostic.open_float, opts)
  vim.keymap.set('n', '[d', vim.diagnostic.goto_prev, opts)
  vim.keymap.set('n', ']d', vim.diagnostic.goto_next, opts)
  vim.keymap.set('n', '<space>q', vim.diagnostic.setloclist, opts)
end

-- Export on_attach for users who want to customize
M.on_attach = on_attach

-- Get LSP setup options
function M.get_lsp_opts(user_opts)
  local opts = {
    init_options = {
      lint = {
        enabled = quadrate_config.lint.enabled,
        path = quadrate_config.lint.path,
      },
    },
    on_attach = on_attach,
    capabilities = vim.lsp.protocol.make_client_capabilities(),
    flags = {
      debounce_text_changes = 150,
    },
  }
  if user_opts then
    opts = vim.tbl_deep_extend('force', opts, user_opts)
  end
  return opts
end

-- Setup LSP - call this from your config if you want auto-setup
-- Or use lspconfig.quadlsp.setup(require('quadrate').get_lsp_opts()) for manual setup
function M.setup_lsp(user_opts)
  if vim.g.quadrate_lsp_setup_done then
    return
  end
  vim.g.quadrate_lsp_setup_done = true
  lspconfig.quadlsp.setup(M.get_lsp_opts(user_opts))
end

-- Auto-setup LSP (can be disabled by setting vim.g.quadrate_no_auto_setup = true before loading)
if not vim.g.quadrate_no_auto_setup then
  M.setup_lsp()
end

return M
