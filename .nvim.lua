vim.api.nvim_create_autocmd("BufWritePost", {
	pattern = { "*.cc", "*.h" },
	callback = function()
		local file = vim.fn.expand("%:p")
		if not file:match("/utf8/") and not file:match("utf8%.h$") then
			vim.fn.system("clang-format -i " .. vim.fn.shellescape(file))
			vim.cmd("edit!")
		end
	end,
})
