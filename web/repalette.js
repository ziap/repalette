const canvas = document.querySelector('canvas')
const ctx = canvas.getContext('2d', { willReadFrequently: true })
const processButton = document.querySelector('#process-image')
const downloadButton = document.querySelector('#download-image')

const downloader = document.createElement('a')
downloader.download = 'output'
downloadButton.addEventListener('click', () => downloader.click())

function updateDownloadURL() {
  canvas.toBlob(blob => {
    if (downloader.href) URL.revokeObjectURL(downloader.href)
    const blobURL = URL.createObjectURL(blob)
    downloader.href = blobURL
    downloadButton.disabled = false
  })
}

let originalImageData = null
function displayImage(image) {
  canvas.width = image.width
  canvas.height = image.height

  canvas.style.aspectRatio = image.width + '/' + image.height
  ctx.drawImage(image, 0, 0)

  originalImageData = ctx.getImageData(0, 0, canvas.width, canvas.height)

  processButton.disabled = false
  downloadButton.disabled = true
  updateDownloadURL()
}

const hexChars = new Set("0123456789abcdefABCDEF")
function isHex(str) {
  if (str.length != 7) return false
  if (str[0] != '#') return false

  for (let i = 1; i < str.length; ++i) {
    if (!hexChars.has(str[i])) return false
  }

  return true
}

function Hex2RGB(hex) {
  return [
    parseInt(hex.slice(1, 3), 16),
    parseInt(hex.slice(3, 5), 16),
    parseInt(hex.slice(5, 7), 16),
  ]
}

ctx.font = 'lighter 84px sans-serif'
ctx.textAlign = 'center'
ctx.fillStyle = '#ccc'
ctx.fillText('Paste image or click to upload', canvas.width / 2, canvas.height / 2)

document.querySelector('#file-input').addEventListener('input', e => {
  const reader = new FileReader()
  reader.addEventListener('load', event => {
    const img = new Image()
    img.addEventListener('load', () => displayImage(img))
    img.src = event.target.result.toString()
  })
  reader.readAsDataURL(e.target.files[0])
})

document.addEventListener('paste', e => {
  for (const item of e.clipboardData.items) {
    if (item.kind != 'file') continue
    const reader = new FileReader()
    const file = item.getAsFile()
    reader.addEventListener('load', event => {
      const img = new Image()
      img.addEventListener('load', () => displayImage(img))
      img.src = event.target.result

    })
    reader.readAsDataURL(file)
    break
  }
})

const paletteContainer = document.querySelector('#palette')
const paletteAdd = document.querySelector('#color-new')

function addColor(hex) {
  const color = document.createElement('label')
  color.classList.add('color')
  color.style.background = hex;

  const colorInp = document.createElement('input')
  colorInp.classList.add('color-input')
  colorInp.type = 'color'
  colorInp.value = hex
  colorInp.addEventListener('change', () => color.style.background = colorInp.value)

  const colorRemove = document.createElement('button')
  colorRemove.textContent = "\u00D7"
  colorRemove.classList.add('color-remove')
  colorRemove.addEventListener('click', () => color.remove())

  color.appendChild(colorInp)
  color.appendChild(colorRemove)
  paletteContainer.insertBefore(color, paletteAdd)
}

paletteAdd.addEventListener('click', () => addColor('#000000'))

const paletteExport = document.querySelector('#palette-export')

paletteExport.addEventListener('focus', e => {
  e.target.value = ''
  for (const color of document.querySelectorAll('.color-input')) {
    e.target.value += color.value + '\n'
  }
})

paletteExport.addEventListener('blur', e => {
  for (const color of document.querySelectorAll('.color')) color.remove()

  for (const line of e.target.value.split('\n')) if (isHex(line)) addColor(line)
})

const paletteSelector = document.querySelector('#palette-select')
const palettePlaceholder = paletteSelector.querySelector('option')
palettePlaceholder.textContent = 'Select palette'

// The bundled Gogh presets: one "name: rrggbb,rrggbb,..." line per palette.
fetch('./palettes.txt').then(async response => {
  if (!response.ok) throw new Error('HTTP ' + response.status)
  const text = await response.text()

  const themes = []
  for (const line of text.split('\n')) {
    const [l, r] = line.split(':')
    if (l === undefined || r === undefined) continue;

    const name = l.split('-')
      .map(word => word.charAt(0).toUpperCase() + word.slice(1))
      .join(' ')

    const colors = r.trimStart().split(',')
    const palette = new Array(colors.length)
    for (let i = 0; i < colors.length; ++i) {
      palette[i] = '#' +colors[i]
    }

    const option = document.createElement('option')
    option.value = themes.length
    option.textContent = name
    paletteSelector.appendChild(option)

    themes.push({ name, palette })
  }

  paletteSelector.addEventListener('change', e => {
    const theme = themes[e.target.value]
    if (!theme) return

    for (const color of document.querySelectorAll('.color')) color.remove()
    for (const color of theme.palette) addColor(color)
  })
}).catch(e => {
  palettePlaceholder.textContent = 'ERROR: Failed to fetch palettes'
  throw e
})

WebAssembly.instantiateStreaming(fetch("./repalette.wasm")).then(wasm => {
  const { exports } = wasm.instance

  const ditherSelect = document.querySelector('#dither-select')
  ditherSelect.querySelector('option').remove()

  const textDecoder = new TextDecoder()
  function cstr(ptr) {
    const buffer = new Uint8Array(exports.memory.buffer, ptr)
    let len = 0
    while (buffer[len] !== 0) len += 1;
    return textDecoder.decode(buffer.subarray(0, len))
  }

  for (let i = 0; i < exports.ditherer_count(); ++i) {
    const name = cstr(exports.ditherer_display(i))

    const option = document.createElement('option')
    option.value = i
    option.textContent = name
    option.selected = i === 1

    ditherSelect.appendChild(option)
  }

  processButton.addEventListener('click', () => {
    ctx.putImageData(originalImageData, 0, 0)

    const colors = Array.from(document.querySelectorAll('.color-input'))
    exports.palette_init(colors.length)

    for (const color of colors) {
      const [r, g, b] = Hex2RGB(color.value)
      exports.palette_add(r, g, b)
    }

    const { data } = ctx.getImageData(0, 0, canvas.width, canvas.height)

    const ptr = exports.get_pixels(canvas.width, canvas.height)
    const len = 4 * canvas.width * canvas.height
    const buf = new Uint8ClampedArray(exports.memory.buffer, ptr, len)
    const imgdata = new ImageData(buf, canvas.width)

    buf.set(data)
    exports.update_canvas(canvas.width, canvas.height, ditherSelect.value)
    ctx.putImageData(imgdata, 0, 0)

    downloadButton.disabled = true
    updateDownloadURL()
  })
})
