import { assertClass } from './utils.ts'

const container = assertClass(
	HTMLElement,
	document.querySelector('.dock-sections'),
)

const switcher = assertClass(
	HTMLElement,
	document.querySelector('#section-switcher'),
)

const sections = new Array<HTMLElement>()
for (const child of container.children) {
	if (child.hasAttribute('data-sec')) {
		sections.push(assertClass(HTMLElement, child))
	}
}

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
	input.value = sectionName
	input.hidden = true

	if (i === 0) {
		input.checked = true
		section.classList.add('active')
	}

	input.addEventListener('change', () => {
		container.style.transform = `translateX(${i * -100}%)`

		for (const sec of sections) {
			sec.classList.remove('active')
		}

		section.classList.add('active')
	})

	const label = document.createElement('div')
	const iconTemplate = assertClass(
		HTMLTemplateElement,
		section.querySelector('.section-icon'),
	)
	const icon = document.importNode(iconTemplate.content, true)
	const text = document.createElement('span')

	text.textContent = sectionLabel
	label.appendChild(icon)
	label.appendChild(text)

	elem.appendChild(input)
	elem.appendChild(label)
	switcher?.appendChild(elem)
}
