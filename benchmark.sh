#!/bin/sh

cmd="./repalette input.jpg output.png -p 3b4252,bf616a,a3be8c,ebcb8b,81a1c1,b48ead,88c0d0,e5e9f0,4c566a,8fbcbb,eceff4,d8dee9,2e3440"

hyperfine \
  "$cmd --dither none" \
  "$cmd --dither floyd-steinberg" \
  "$cmd --dither atkinson" \
  "$cmd --dither jjn" \
  "$cmd --dither burkes" \
  "$cmd --dither sierra" \
  "$cmd --dither sierra-lite"
