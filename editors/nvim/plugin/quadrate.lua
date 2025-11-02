-- Quadrate Language Server Protocol and Tree-sitter configuration for Neovim
-- Requires: nvim-lspconfig
-- Optional: nvim-treesitter (for better syntax highlighting)

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

-- Define quadlsp configuration
if not configs.quadlsp then
  configs.quadlsp = {
    default_config = {
      cmd = {'quadlsp'},
      filetypes = {'quadrate'},
      root_dir = function(fname)
        return lspconfig.util.find_git_ancestor(fname)
               or lspconfig.util.path.dirname(fname)
      end,
      settings = {},
      init_options = {},
    },
  }
end

-- Auto-setup when opening .qd files
vim.api.nvim_create_autocmd('FileType', {
  pattern = 'quadrate',
  callback = function()
    -- Only setup if not already attached
    local clients = vim.lsp.get_active_clients({bufnr = 0})
    for _, client in ipairs(clients) do
      if client.name == 'quadlsp' then
        return -- Already attached
      end
    end

    -- Setup LSP
    lspconfig.quadlsp.setup({
      on_attach = function(client, bufnr)
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

        vim.notify('Quadrate LSP attached', vim.log.levels.INFO)
      end,
      capabilities = vim.lsp.protocol.make_client_capabilities(),
      flags = {
        debounce_text_changes = 150,
      },
    })
  end,
})
