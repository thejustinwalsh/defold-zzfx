local M = {}

-- alias to zzfx: https://killedbyapixel.github.io/ZzFX/
function M.play(...)
  zzfx_ext.play_sample(zzfx_ext.build_sample(...))
end

-- set the samplerate, defaults to 44100
function M.samplerate(...)
  return zzfx_ext.samplerate(...)
end

-- play a sample returnd from zzfx_ext.build_sample
function M.play_sample(...)
  zzfx_ext.play_sample(...)
end

-- build a sample from a zzfx param list
function M.build_sample(...)
  zzfx_ext.build_sample(...)
end

return M