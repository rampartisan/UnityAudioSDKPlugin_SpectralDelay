#include "AudioPluginUtil.h"
#include <vector>
#include <string>


namespace SpecDelay
{
    
    
#if UNITY_WIN
#define ABA_API __declspec(dllexport)
#else
#define ABA_API
#endif
    
    std::vector<float> guiArray = {0,0.5,1,0.5,0};
    std::vector<float> delArray = {0};
    
    // Interop functions
    
    typedef void(*DebugCallback) (const char *str);
    DebugCallback gDebugCallback;
    
    extern "C" ABA_API void RegisterDebugCallback(DebugCallback callback)
    {
        if (callback)
        {
            gDebugCallback = callback;
        }
    }
    
    void DebugInUnity(std::string message)
    {
        if (gDebugCallback)
        {
            gDebugCallback(message.c_str());
        }
    }
    
    extern "C" ABA_API void getArray(long* len, float **data){
        *len = guiArray.size();
        auto size = (*len)*sizeof(float);
        *data = static_cast<float*>(malloc(size));
        memcpy(*data, guiArray.data(), size);
    }
    
    extern "C" ABA_API void setArray(float gArrayIn[], int gLen, float dArrayIn[], int dLen)
    {
        guiArray.assign(gArrayIn,gArrayIn+gLen);
        delArray.assign(dArrayIn, dArrayIn+dLen);

    }
    
    
    // End Interop
    
     enum Param
    {
        P_WindowSize,
        P_MaxDel,
        P_Debug,
        P_ShowSpectrum,
        
        P_NUM
    };
    
    struct EffectData
    {
        struct Data
        {
            FFTAnalyzer analyzer;
            float p[P_NUM];

        };
        union
        {
            Data data;
            unsigned char pad[(sizeof(Data) + 15) & ~15]; // This entire structure must be a multiple of 16 bytes (and and instance 16 byte aligned) for PS3 SPU DMA requirements
        };
    };

#if !UNITY_SPU
    int InternalRegisterEffectDefinition(UnityAudioEffectDefinition& definition)
    {
        int numparams = P_NUM;
        definition.paramdefs = new UnityAudioParameterDefinition[numparams];
        RegisterParameter(definition, "FFT Window Size", "Hz", 16.0f, 4096.0f, 2048.0f, 1.0f, 1.0f, P_WindowSize, "Size of window for FFT");
          RegisterParameter(definition, "Maximum Delay", "Windows", 1.0f, 100.0f, 40.0f, 1.0f, 1.0f, P_MaxDel, "Maximum delay time (top of the GUI)");
        RegisterParameter(definition, "Show Spectrum", "", 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, P_ShowSpectrum, "Show Spectrum (< 0.5 off)");
              RegisterParameter(definition, "DEBUG", "", 0.0f, 10000.0f, 1.0f, 1.0f, 1.0f, P_Debug ,"various debuggin shite");
        return numparams;
    }

    UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK CreateCallback(UnityAudioEffectState* state)
    {
        EffectData* effectdata = new EffectData;
        memset(effectdata, 0, sizeof(EffectData));
#if !UNITY_PS3
        effectdata->data.analyzer.spectrumSize = 4096;
#endif
        InitParametersFromDefinitions(InternalRegisterEffectDefinition, effectdata->data.p);
        state->effectdata = effectdata;

        return UNITY_AUDIODSP_OK;
    }

    UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK ReleaseCallback(UnityAudioEffectState* state)
    {
        EffectData::Data* data = &state->GetEffectData<EffectData>()->data;
#if !UNITY_PS3
        data->analyzer.Cleanup();
#endif
        delete data;
        return UNITY_AUDIODSP_OK;
    }


    UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK SetFloatParameterCallback(UnityAudioEffectState* state, int index, float value)
    {
        EffectData::Data* data = &state->GetEffectData<EffectData>()->data;
        if (index >= P_NUM)
            return UNITY_AUDIODSP_ERR_UNSUPPORTED;
        data->p[index] = value;
        return UNITY_AUDIODSP_OK;
    }

    UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK GetFloatParameterCallback(UnityAudioEffectState* state, int index, float* value, char *valuestr)
    {
        EffectData::Data* data = &state->GetEffectData<EffectData>()->data;
        if (value != NULL)
            *value = data->p[index];
        if (valuestr != NULL)
            valuestr[0] = 0;
        return UNITY_AUDIODSP_OK;
    }

    int UNITY_AUDIODSP_CALLBACK GetFloatBufferCallback(UnityAudioEffectState* state, const char* name, float* buffer, int numsamples)
    {
#if !UNITY_PS3
        EffectData::Data* data = &state->GetEffectData<EffectData>()->data;
        if (strcmp(name, "InputSpec") == 0)
            data->analyzer.ReadBuffer(buffer, numsamples, true);
        else if (strcmp(name, "OutputSpec") == 0)
            data->analyzer.ReadBuffer(buffer, numsamples, false);
        else
#endif
        memset(buffer, 0, sizeof(float) * numsamples);
        return UNITY_AUDIODSP_OK;
    }
#endif
    
#if !UNITY_PS3 || UNITY_SPU

#if UNITY_SPU
    EffectData  g_EffectData __attribute__((aligned(16)));
#endif


    UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK ProcessCallback(UnityAudioEffectState* state, float* inbuffer, float* outbuffer, unsigned int length, int inchannels, int outchannels)
    {
        EffectData::Data* data = &state->GetEffectData<EffectData>()->data;
  
#if UNITY_SPU
        UNITY_PS3_CELLDMA_GET(&g_EffectData, state->effectdata, sizeof(g_EffectData));
        data = &g_EffectData.data;
#endif

        const float sr = (float)state->samplerate;
#if !UNITY_PS3 && !UNITY_SPU
        float specDecay = powf(10.0f, 0.05f * -10.0f * length / sr);
        bool calcSpectrum = (data->p[P_ShowSpectrum] >= 0.5f);
        if (calcSpectrum)
            data->analyzer.AnalyzeInput(inbuffer, inchannels, length, specDecay);
#endif
        for (unsigned int n = 0; n < length; n++)
        {
            for (int i = 0; i < outchannels; i++)
            {
                outbuffer[n * outchannels + i] = inbuffer[n * outchannels + i]*delArray[0];
            }
        }

#if !UNITY_PS3 && !UNITY_SPU
        if (calcSpectrum)
            data->analyzer.AnalyzeOutput(outbuffer, outchannels, length, specDecay);
#endif
        
#if UNITY_SPU
        UNITY_PS3_CELLDMA_PUT(&g_EffectData, state->effectdata, sizeof(g_EffectData));
#endif
        
        return UNITY_AUDIODSP_OK;
    }
    
#endif
    

}
