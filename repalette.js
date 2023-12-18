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

function to_rgb(hex) {
  const result = /^#?([a-f\d]{2})([a-f\d]{2})([a-f\d]{2})$/i.exec(hex)
  return [
    parseInt(result[1], 16),
    parseInt(result[2], 16),
    parseInt(result[3], 16),
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
  colorInp.addEventListener('change', e => color.style.background = e.target.value)

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

  for (const line of e.target.value.split('\n')) {
    if (/^#[0-9A-Fa-f]{6}$/i.test(line)) addColor(line)
  }
})

fetch('https://raw.githubusercontent.com/Gogh-Co/Gogh/master/data/themes.json').then(async response => {
  const data = await response.json()

  const paletteSelector = document.querySelector('#palette-select')
  paletteSelector.querySelector('option').textContent = 'Select palette'

  const themes = new Array(data.length)
  for (let i = 0; i < data.length; ++i) {
    const {name} = data[i]

    const palette = new Set()
    for (const [k, v] of Object.entries(data[i])) if (k.startsWith("color_")) palette.add(v)

    themes[i] = { name, palette }

    const option = document.createElement('option')
    option.value = i
    option.textContent = name

    paletteSelector.appendChild(option)
  }

  paletteSelector.addEventListener('change', e => {
    if (e.value == "-1") return

    for (const color of document.querySelectorAll('.color')) color.remove()

    for (const color of themes[e.target.value].palette) addColor(color)
  })
}).catch(e => {
  const paletteSelector = document.querySelector('#palette-select')
  const option = paletteSelector.querySelector('option')
  option.textContent = 'ERROR: Failed to fetch colors from Gosh'
  throw e
})

WebAssembly.instantiateStreaming(fetch("./repalette.wasm")).then(wasm => {
  const { exports } = wasm.instance

  const ditherSelect = document.querySelector('#dither-select')

  processButton.addEventListener('click', () => {
    ctx.putImageData(originalImageData, 0, 0)

    exports.palette_clear()
    for (const color of document.querySelectorAll('.color-input')) {
      const [r, g, b] = to_rgb(color.value)
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
