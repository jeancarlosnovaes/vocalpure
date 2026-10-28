"""Render VocalPure's existing waveform mark into Windows packaging assets."""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

root=Path(__file__).resolve().parent.parent
assets=root/'assets'
assets.mkdir(exist_ok=True)
icon=Image.new('RGBA',(512,512),(0,0,0,0))
draw=ImageDraw.Draw(icon)
draw.rounded_rectangle((12,12,500,500),radius=110,fill='#0a151d',outline='#2e555f',width=7)
heights=[74,158,240,116,196,88,148]
for i,height in enumerate(heights):
    x=100+i*52
    draw.rounded_rectangle((x-10,256-height/2,x+10,256+height/2),radius=10,fill='#7de8da')
icon.save(assets/'vocalpure.ico',sizes=[(16,16),(20,20),(24,24),(32,32),(40,40),(48,48),(64,64),(128,128),(256,256)])
icon.resize((55,55),Image.Resampling.LANCZOS).convert('RGB').save(assets/'wizard-small.bmp')
sidebar=Image.new('RGB',(328,628))
pixels=sidebar.load()
for y in range(628):
    for x in range(328):
        t=y/627
        pixels[x,y]=(int(13+8*t),int(34+30*t),int(45+24*t))
d=ImageDraw.Draw(sidebar)
sidebar.paste(icon.resize((164,164),Image.Resampling.LANCZOS),(82,84),icon.resize((164,164),Image.Resampling.LANCZOS))
font=lambda n:ImageFont.truetype('C:/Windows/Fonts/segoeui.ttf',n)
d.text((164,292),'VocalPure',font=font(36),fill='#e8f5f6',anchor='mm')
d.text((164,338),'ÁUDIO NEURAL LOCAL',font=font(16),fill='#7de8da',anchor='mm')
d.line((55,398,273,398),fill='#426774',width=2)
d.text((164,451),'Aplicativo',font=font(22),fill='#b9d3dc',anchor='mm')
d.text((164,487),'VST3  /  CLAP',font=font(22),fill='#b9d3dc',anchor='mm')
sidebar.save(assets/'wizard-sidebar.bmp')
(assets/'vocalpure.svg').write_text('''<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 512 512"><rect x="12" y="12" width="488" height="488" rx="110" fill="#0a151d" stroke="#2e555f" stroke-width="7"/><g stroke="#7de8da" stroke-width="20" stroke-linecap="round"><path d="M100 219v74M152 177v158M204 136v240M256 198v116M308 158v196M360 212v88M412 182v148"/></g></svg>''',encoding='utf-8')
