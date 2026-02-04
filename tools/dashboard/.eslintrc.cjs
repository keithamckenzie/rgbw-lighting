module.exports = {
	root: true,
	parser: 'svelte-eslint-parser',
	parserOptions: {
		parser: '@typescript-eslint/parser',
		ecmaVersion: 2022,
		sourceType: 'module',
		extraFileExtensions: ['.svelte']
	},
	extends: [
		'eslint:recommended',
		'plugin:@typescript-eslint/recommended',
		'plugin:svelte/recommended',
		'prettier'
	],
	env: {
		browser: true,
		node: true,
		es2022: true
	}
};
