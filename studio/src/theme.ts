const THEME_ATTR = 'data-theme'
const THEME_KEY = 'repalette:theme'

function applyTheme(theme: 'light' | 'dark'): void {
	document.documentElement.setAttribute(THEME_ATTR, theme)
}

const lightQuery = globalThis.matchMedia('(prefers-color-scheme: light)')
lightQuery.addEventListener('change', () => {
	const systemTheme = lightQuery.matches ? 'light' : 'dark'
	const theme = localStorage.getItem(THEME_KEY) ?? systemTheme
	applyTheme(theme === 'light' ? 'light' : 'dark')
})

document.querySelector('.theme-btn')?.addEventListener('click', () => {
	const theme = document.documentElement.getAttribute(THEME_ATTR)
	const nextTheme = theme === 'light' ? 'dark' : 'light'
	applyTheme(nextTheme)

	// Persists user changes
	localStorage.setItem(THEME_KEY, nextTheme)
})

addEventListener('storage', (e) => {
	if (e.key !== THEME_KEY) return
	applyTheme(e.newValue === 'light' ? 'light' : 'dark')
})
