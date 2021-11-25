local M = {}

-- alias to zzfx: https://killedbyapixel.github.io/ZzFX/
function M.play(...)
  zzfx_ext.play_sample(zzfx_ext.build_sample(...))
end

-- set the samplerate, defaults to 44100
function M.samplerate(rate)
  zzfx_ext.samplerate(rate)
end

-- play a sample returnd from zzfx_ext.build_sample
function M.play_sample(sample)
  zzfx_ext.play_sample(sample)
end

-- build a sample from a zzfx param list
function M.build_sample(...)
  return zzfx_ext.build_sample(...)
end

return M