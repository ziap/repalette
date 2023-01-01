const fileInput = document.querySelector('#file-input')
const processButton = document.querySelector('#process-image')
const paletteContainer = document.querySelector('#palette')
const paletteAdd = document.querySelector('#color-new')
const canvas = document.querySelector('canvas')
const ctx = canvas.getContext('2d')

function displayImage(image) {
  canvas.width = image.width
  canvas.height = image.height

  canvas.style.aspectRatio = image.width + '/' + image.height
  ctx.drawImage(image, 0, 0)
  processButton.disabled = false
}

function to_rgb(hex) {
  const result = /^#?([a-f\d]{2})([a-f\d]{2})([a-f\d]{2})$/i.exec(hex)
  return [
    parseInt(result[1], 16),
    parseInt(result[2], 16),
    parseInt(result[3], 16),
  ]
}

canvas.width = 800
canvas.height = 600

ctx.font = 'lighter 32px sans-serif'
ctx.textAlign = 'center'
ctx.fillStyle = '#ccc'
ctx.fillText('Paste image or click to upload', canvas.width / 2, canvas.height / 2)

fileInput.addEventListener('input', e => {
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

function addColor(hex) {
  const color = document.createElement('label')
  color.classList.add('color')
  color.style.background = hex;

  const colorInp = document.createElement('input')
  colorInp.classList.add('color-input')
  colorInp.type = 'color'
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

const wasm = await WebAssembly.instantiateStreaming(fetch("./repalette.wasm"))
const { exports } = wasm.instance

processButton.addEventListener('click', () => {
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
  exports.update_canvas(canvas.width, canvas.height)
  ctx.putImageData(imgdata, 0, 0)
})
