import './theme.ts'

function nonNull(): never {
	throw new Error('Non-null assertion failed')
}

const switcher = document.querySelector('#section-switcher')
const sections = document.querySelectorAll('.dock-section[data-sec]')

for (let i = 0; i < sections.length; ++i) {
	const section = sections[i]
	const sectionName = section.getAttribute('data-sec')
	if (sectionName === null) continue
	const sectionLabel = section.getAttribute('data-sec-label') ?? ''

	const elem = document.createElement('label')
	elem.classList.add('switcher-input')

	const input = document.createElement('input')
	input.name = 'current-section'
	input.type = 'radio'
	input.checked = i === 0
	input.value = sectionName
	input.hidden = true

	const label = document.createElement('div')
	const iconTemplate = section.querySelector('.section-icon') ?? nonNull()
	const icon = document.importNode(iconTemplate.content, true)
	const text = document.createElement('span')

	text.textContent = sectionLabel
	label.appendChild(icon)
	label.appendChild(text)

	elem.appendChild(input)
	elem.appendChild(label)
	switcher?.appendChild(elem)
}
