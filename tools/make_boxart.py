#!/usr/bin/env python3
"""Box art: 500x500 RGBA from two in-game renders (GOR_BOXART=1 scene, =2 title band). usage: make_boxart.py scene.bmp title.bmp out.png"""
import sys
from PIL import Image
scene=Image.open(sys.argv[1]).convert('RGB').resize((500,375),Image.LANCZOS)
band=Image.open(sys.argv[2]).convert('RGB')
# title band: the 640x480 title frame's text area (y 140..300), scaled the same way as the scene
band=band.crop((0,130,640,320)).resize((500,148),Image.LANCZOS)
c=Image.new('RGB',(500,500),band.getpixel((2,2)))
c.paste(band,(0,0)); c.paste(scene,(0,125))
# soften the seam: sky colours match, so nothing else is needed
c.convert('RGBA').save(sys.argv[3])
