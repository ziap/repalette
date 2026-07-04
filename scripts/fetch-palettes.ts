const GOGH_URL =
	'https://raw.githubusercontent.com/Gogh-Co/Gogh/master/data/themes.json'
const OUTPUT_PATH = 'src/palettes.txt'

function isHex(value: unknown): value is string {
	if (typeof value !== 'string') return false
	if (value.length !== 7) return false
	if (value[0] !== '#') return false
	for (let i = 1; i < value.length; ++i) {
		const c = value[i]
		const ok = (c >= '0' && c <= '9') ||
			(c >= 'a' && c <= 'f') ||
			(c >= 'A' && c <= 'F')
		if (!ok) return false
	}
	return true
}

function normalizeName(name: string): string {
	return name
		.toLowerCase()
		.replace(/[^a-z0-9\s]/g, '')
		.trim()
		.replace(/\s+/g, '-')
}

function encodePalette(theme: Record<string, unknown>): string {
	const colors = new Set<string>()
	for (const value of Object.values(theme)) {
		if (isHex(value)) colors.add(value.slice(1).toLowerCase())
	}
	return Array.from(colors).join(',')
}

async function main(): Promise<void> {
	const response = await fetch(GOGH_URL)
	if (!response.ok) {
		throw new Error(`Failed to fetch Gogh themes: ${response.status}`)
	}

	const themes = await response.json() as Record<string, unknown>[]

	const file = await Deno.open(OUTPUT_PATH, {
		write: true,
		create: true,
		truncate: true,
	})
	const writer = file.writable.getWriter()
	const encoder = new TextEncoder()

	let count = 0
	for (const theme of themes) {
		const name = theme.name
		if (typeof name !== 'string') continue

		const colors = encodePalette(theme)
		if (colors === '') continue

		await writer.write(encoder.encode(`${normalizeName(name)}: ${colors}\n`))
		++count
	}

	await writer.close()
	console.log(`Wrote ${count} palettes to ${OUTPUT_PATH}`)
}

await main()
