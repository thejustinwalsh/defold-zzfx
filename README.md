# ZzFX Defold Extension
> Zuper Zmall Zound Zynth

### [ZzFX Sound Designer](https://killedbyapixel.github.io/ZzFX/)

Port of [ZzFX](https://killedbyapixel.github.io/ZzFX/) for the [Defold Game Engine](https://defold.com)

## Defold Setup
Open your game.project file and in the dependencies field under project add:
```
https://github.com/thejustinwalsh/defold-zzfx/archive/main.zip
```

Require the zzfx.api script and play sound effects like so:
```lua
local zzfx_api = require("zzfx.api")
local zzfx = zzfx_api.play

zzfx(1,.05,448,.01,.1,.3,3,.39,-0.5,0,0,0,0,0,.2,.1,.08,1,0,0)
```

### Caveats
  - Limited to 32 unique zzfx with 32 intances of each playing simualtaniously
    - This will be configurable in the future
  - The code `local zzfx = zzfx_api.play` is for compatibility with copy & paste from the [ZzFX Sound Designer](https://killedbyapixel.github.io/ZzFX/)
    - Be sure to uncheck the `Spread` checkbox as lua doesn't support the spread syntax.

## Attribution
- Port of soundboard and core [ZzFX](https://github.com/KilledByAPixel/ZzFX) by @KilledByAPixel
- Emoji graphics from [twemoji](https://github.com/twitter/twemoji) by @twitter
