document.querySelector('.theme-btn')?.addEventListener('click', () => {
	const theme = document.documentElement.getAttribute('data-theme')
	const next = theme === 'light' ? 'dark' : 'light'
	document.documentElement.setAttribute('data-theme', next)
	localStorage.setItem('repalette:theme', next)
})
