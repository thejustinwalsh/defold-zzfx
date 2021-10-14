#define LIB_NAME "ZzFX"
#define MODULE_NAME "zzfx_ext"
#define DLIB_LOG_DOMAIN "ZZFX"

#include <random>
#include <dmsdk/sdk.h>

// TODO: Allow configuration of buffer count
static const int BUFFER_COUNT = 32;
static const int BUFFER_INSTANCE_COUNT = 32;

// ZzFX Data
namespace
{
    // Globals
    int sampleRate = 44100;
    float volume = 0.3f;

    // BufferUserData info
    struct BufferUserData {
        int index;
        dmArray<int16_t> *buffer;
        dmSound::HSoundData soundData;
    };

    // Audio Buffers
    dmArray<int16_t> sampleBuffers[BUFFER_COUNT];
    dmArray<int> freeBuffers;

    // Instance and buffer management
    dmArray<BufferUserData> bufferReturnQueue;
    dmArray<dmSound::HSoundInstance> bufferInstances[BUFFER_COUNT];

    // Get an available audio buffer
    auto getBuffer = []() {
        if (freeBuffers.Empty()) return BufferUserData{-1, nullptr, nullptr};

        // Get a free buffer
        int index = freeBuffers[freeBuffers.Size() - 1];
        freeBuffers.Pop();
        
        // Return a usable buffer and the free index to return to
        return BufferUserData{index, &sampleBuffers[index], nullptr};
    };
}

// Utility functions
namespace Util {
    // Random Number
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dis(.0, 1.0);
    auto random = []() { return dis(gen); };

    // WAV HEADER -- Deprecate once we have a raw buffer time available
    const int WAV_HEADER = 23;
    auto wavHeader = [](dmArray<int16_t> &buffer, int length, int channels = 1) {
        buffer[ 0] = 0x4952; // RI
        buffer[ 1] = 0x4646; // FF
        buffer[ 2] = (2 * length + 15) & 0x0000ffff; // RIFF size
        buffer[ 3] = ((2 * length + 15) & 0xffff0000) >> 16; // RIFF size
        buffer[ 4] = 0x4157; // WA
        buffer[ 5] = 0x4556; // VE
        buffer[ 6] = 0x6d66; // fm
        buffer[ 7] = 0x2074; // t
        buffer[ 8] = 0x0012; // fmt chunksize: 18
        buffer[ 9] = 0x0000; //
        buffer[10] = 0x0001; // format tag : 1 
        buffer[11] = channels; // channelCount
        buffer[12] = sampleRate & 0x0000ffff; // sample per sec
        buffer[13] = (sampleRate & 0xffff0000) >> 16; // sample per sec
        buffer[14] = (2 * channels * sampleRate) & 0x0000ffff; // byte per sec
        buffer[15] = ((2 * channels * sampleRate) & 0xffff0000) >> 16; // byte per sec
        buffer[16] = 0x0004; // block align
        buffer[17] = 0x0010; // bit per sample
        buffer[18] = 0x0000; // cb size
        buffer[19] = 0x6164; // da
        buffer[20] = 0x6174; // ta
        buffer[21] = (2 * length) & 0x0000ffff; // data size[byte]
        buffer[22] = ((2 * length) & 0xffff0000) >> 16; // data size[byte]
    };
}

// isSample - check for sample userdata
BufferUserData* isSample(lua_State* L, int index) {
    void* pUserData = luaL_checkudata(L, index, MODULE_NAME"_sample");
    luaL_argcheck(L, pUserData != NULL, index, "sample expected");

    return (BufferUserData*)pUserData;
}

// __gc method
static int returnSample(lua_State *L)
{
    DM_LUA_STACK_CHECK(L, 0);

    BufferUserData *bufferUserData = isSample(L, 1);
    if (bufferUserData) bufferReturnQueue.Push(*bufferUserData);

    return 0;
}

static int setSampleRate(lua_State* L) {
    DM_LUA_STACK_CHECK(L, 1);

    if (lua_isnumber(L, 1)) sampleRate = luaL_checkinteger(L, 1);
    lua_pushinteger(L, sampleRate);

    return 1;
}

static int playSample(lua_State* L) {
    DM_LUA_STACK_CHECK(L, 0);

    BufferUserData * bufferUserData = isSample(L, 1);

    dmSound::HSoundInstance instance;
    auto result = dmSound::NewSoundInstance(bufferUserData->soundData, &instance);
    if (result != dmSound::RESULT_OK)
    {
        dmLogWarning("Failed to create sound instance: %d", result);
        lua_pushnil(L);
        return 1;
    }

    //dmSound::SetInstanceGroup(instance, dmHashString64(MODULE_NAME));
    dmSound::SetLooping(instance, false, 0);
    dmSound::SetParameter(instance, dmSound::PARAMETER_GAIN, Vectormath::Aos::Vector4(1, 1, 1, 1));
    dmSound::SetParameter(instance, dmSound::PARAMETER_PAN, Vectormath::Aos::Vector4(0, 0, 0, 0));
    dmSound::SetParameter(instance, dmSound::PARAMETER_SPEED, Vectormath::Aos::Vector4(1, 1, 1, 1));

    // Play sound
    result = dmSound::Play(instance);
    if (result != dmSound::RESULT_OK)
    {
        dmLogWarning("Failed to play sound instance: %d", result);
        lua_pushnil(L);
        return 1;
    }

    // Store instances for lookup on update
    auto &instances = bufferInstances[bufferUserData->index];
    if (instances.Full()) instances.SetCapacity(instances.Capacity() + BUFFER_INSTANCE_COUNT);
    instances.Push(instance);

    return 0;
}

static int buildSample(lua_State* L) {
    DM_LUA_STACK_CHECK(L, 1);

    // Reserve an audio buffer
    auto bufferUserData = getBuffer();
    if (bufferUserData.index == -1)
    { 
        dmLogWarning("Out of ZzFX Audio Buffers!");
        lua_pushnil(L);
        return 1;
    }

    // Get arg count
    int argn = lua_gettop(L);
    
    // Parse params
    double loudness =        argn >=  1 ? luaL_checknumber(L,  1) : 1;
    double randomness =      argn >=  2 ? luaL_checknumber(L,  2) : .05;
    double frequency =       argn >=  3 ? luaL_checknumber(L,  3) : 220;
    double attack =          argn >=  4 ? luaL_checknumber(L,  4) : 0;
    double sustain =         argn >=  5 ? luaL_checknumber(L,  5) : 0;
    double release =         argn >=  6 ? luaL_checknumber(L,  6) : .1;
    double shape =           argn >=  7 ? luaL_checknumber(L,  7) : 0;
    double shapeCurve =      argn >=  8 ? luaL_checknumber(L,  8) : 1;
    double slide =           argn >=  9 ? luaL_checknumber(L,  9) : 0;
    double deltaSlide =      argn >= 10 ? luaL_checknumber(L, 10) : 0; 
    double pitchJump =       argn >= 11 ? luaL_checknumber(L, 11) : 0;
    double pitchJumpTime =   argn >= 12 ? luaL_checknumber(L, 12) : 0; 
    double repeatTime =      argn >= 13 ? luaL_checknumber(L, 13) : 0; 
    double noise =           argn >= 14 ? luaL_checknumber(L, 14) : 0;
    double modulation =      argn >= 15 ? luaL_checknumber(L, 15) : 0;
    double bitCrush =        argn >= 16 ? luaL_checknumber(L, 16) : 0;
    double delay =           argn >= 17 ? luaL_checknumber(L, 17) : 0;
    double sustainVolume =   argn >= 18 ? luaL_checknumber(L, 18) : 1;
    double decay =           argn >= 19 ? luaL_checknumber(L, 19) : 0;
    double tremolo =         argn >= 20 ? luaL_checknumber(L, 20) : 0;
    
    //dmLogInfo("zzfx(%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f)",
    //    loudness, randomness, frequency, attack, sustain, release, shape, shapeCurve, slide, deltaSlide, pitchJump, pitchJumpTime, repeatTime, noise, modulation, bitCrush, delay, sustainVolume, decay, tremolo
    //);

    // Init parameters
    double PI2 = M_PI * 2;
    auto sign = [](double v) { return v > 0 ? 1 : -1; };
    double startSlide = slide *= 500 * PI2 / sampleRate / sampleRate;
    double startFrequency = frequency *= 
        (1 + randomness * 2 * Util::random() - randomness) * PI2 / sampleRate;
    double t = 0, tm = 0, s = 0, f = 0;
    int length = 0, i = 0, j = 1, r = 0, c = 0;

    // scale by sample rate
    attack = attack * sampleRate + 9; // minimum attack to prevent pop
    decay *= sampleRate;
    sustain *= sampleRate;
    release *= sampleRate;
    delay *= sampleRate;
    deltaSlide *= 500 * PI2 / pow(sampleRate, 3.0);
    modulation *= PI2 / sampleRate;
    pitchJump *= PI2 / sampleRate;
    pitchJumpTime *= sampleRate;
    repeatTime = floor(repeatTime * sampleRate);

    // Initialize audio buffer
    static dmArray<double> buffer;
    length = (int)(attack + decay + sustain + release + delay);
    if (length > buffer.Capacity()) buffer.SetCapacity(length);
    buffer.SetSize(length);

    // generate waveform
    for(;i < length; buffer[i++] = s)
    {
        auto bc = (int)(bitCrush*100); ++c;                   // bit crush
        if (bc != 0 ? c%bc == 0 : true)
        { 
            s = shape != 0 ? shape>1? shape>2? shape>3?     // wave shape
                sin(pow(fmod(t,PI2),3.0)) :                 // 4 noise
                fmax(fmin(tan(t),1.0),-1.0):                // 3 tan
                1-fmod(fmod(2*t/PI2,2)+2,2):                // 2 saw
                1-4*fabs(roundf(t/PI2)-t/PI2):              // 1 triangle
                sin(t);                                     // 0 sin

            s = (repeatTime != 0 ?
                    1 - tremolo + tremolo * sin(PI2*i/repeatTime) // tremolo
                    : 1) *
                sign(s) * pow(fabs(s), shapeCurve) *        // curve 0=square, 2=pointy
                volume * loudness * (                       // envelope
                i < attack ? i/attack :                     // attack
                i < attack + decay ?                        // decay
                1-((i-attack)/decay)*(1-sustainVolume) :    // decay falloff
                i < attack  + decay + sustain ?             // sustain
                sustainVolume :                             // sustain volume
                i < length - delay ?                        // release
                (length - i - delay)/release *              // release falloff
                sustainVolume :                             // release volume
                0);                                         // post release

            s = delay != 0 ? s/2.0 + (((int)delay) > i ? 0 :       // delay
                (i<length-(int)delay? 1.0 : (length-i)/delay) *      // release delay 
                buffer[i-(int)delay] / 2.0) : s;                    // sample delay
        }

        f = (frequency += slide += deltaSlide) *            // frequency
            cos(modulation*tm++);                           // modulation
        t += f - f*noise*(1 - fmod((sin(i)+1)*1e9, 2));     // noise

        if (j != 0 && ++j > pitchJumpTime)       // pitch jump
        {
            frequency += pitchJump;         // apply pitch jump
            startFrequency += pitchJump;    // also apply to start
            j = 0;                          // stop pitch jump time
        }

        if (repeatTime != 0 && (++r % (int32_t)repeatTime) == 0) // repeat
        {
            frequency = startFrequency;     // reset frequency
            slide = startSlide;             // reset slide
            j = j > 0 ? j : 1;              // reset pitch jump time
        }
    }

    // Encode the WAV and create the Sound Data object
    auto& target = *bufferUserData.buffer;
    const int targetLength = length + Util::WAV_HEADER;
    if (targetLength > target.Capacity()) target.SetCapacity(targetLength);
    target.SetSize(targetLength);
    Util::wavHeader(target, length);
    
    for (int i = 0; i < length; ++i) target[Util::WAV_HEADER + i] = buffer[i] >= 1.0 ? (1<<15) - 1 : (int16_t)(buffer[i] * (1<<15));

    char name[32] = {0};
    sprintf(name, "zzfx-%d", bufferUserData.index);
    auto result = dmSound::NewSoundData(
        target.Begin(), 
        target.Size() * sizeof(int16_t),
        dmSound::SoundDataType::SOUND_DATA_TYPE_WAV, 
        &bufferUserData.soundData,
        dmHashString64(name)
    );

    if (result != dmSound::RESULT_OK)
    {
        dmLogError("Failed to create sound data: %s, %d", name, result);
        lua_pushnil(L);
        return 1;
    }
    
    // Return lua userdata to track samples
    *(BufferUserData*)lua_newuserdata(L, sizeof(BufferUserData)) = bufferUserData;
    if (luaL_newmetatable(L, MODULE_NAME"_sample")) {        
        lua_pushcfunction(L, returnSample); lua_setfield(L, -2, "__gc");
        lua_pushvalue(L, -1); lua_setfield(L, -2, "__index");
    }
    lua_setmetatable(L, -2);

    return 1;
}

// Functions exposed to Lua
static const luaL_reg ModuleInterface[] =
{
    {"samplerate", setSampleRate},
    {"play_sample", playSample},
    {"build_sample", buildSample},
    {0, 0}
};

static void LuaInit(lua_State* L)
{
    int top = lua_gettop(L);

    luaL_register(L, MODULE_NAME, ModuleInterface);

    lua_pop(L, 1);
    assert(top == lua_gettop(L));
}

dmExtension::Result app_init(dmExtension::AppParams* params)
{
    return dmExtension::RESULT_OK;
}

dmExtension::Result app_final(dmExtension::AppParams* params)
{
    return dmExtension::RESULT_OK;
}

dmExtension::Result init(dmExtension::Params* params)
{
    // Init Lua
    LuaInit(params->m_L);
    dmLogInfo("Registered %s Extension", MODULE_NAME);
    
    // Set buffer capacity & initial size
    freeBuffers.SetCapacity(BUFFER_COUNT); 
    bufferReturnQueue.SetCapacity(BUFFER_COUNT);
    freeBuffers.SetSize(BUFFER_COUNT); 

    // Init buffers
    for (int i = 0; i < BUFFER_COUNT; i++)
    {
        sampleBuffers[i].SetCapacity(Util::WAV_HEADER + sampleRate * 15);
        bufferInstances[i].SetCapacity(BUFFER_INSTANCE_COUNT);
        freeBuffers[i] = i;
    };

    return dmExtension::RESULT_OK;
}

dmExtension::Result update(dmExtension::Params* params)
{
    // Delete the instance if it is done playing
    for (int i = 0; i < BUFFER_COUNT; i++)
    {
        auto &instances = bufferInstances[i];
        for (int j = instances.Size() - 1; j >= 0; j--)
        {
            auto &instance = instances[j];
            if (!dmSound::IsPlaying(instance)) {
                instances.EraseSwap(j);
                dmSound::DeleteSoundInstance(instance);
            }
        }
    }

    // Delete the sound data if there are no active instances left
    for (int i = bufferReturnQueue.Size() - 1; i >= 0; i--)
    {
        auto &bufferUserData = bufferReturnQueue[i];
        if (bufferInstances[bufferUserData.index].Size() == 0) {
            bufferReturnQueue.EraseSwap(i);
            dmSound::DeleteSoundData(bufferUserData.soundData);
            freeBuffers.Push(bufferUserData.index);            
        }
    }
    
    return dmExtension::RESULT_OK;
}

dmExtension::Result final(dmExtension::Params* params)
{
    return dmExtension::RESULT_OK;
}

DM_DECLARE_EXTENSION(ZzFX, LIB_NAME, app_init, app_final, init, update, 0, final)
