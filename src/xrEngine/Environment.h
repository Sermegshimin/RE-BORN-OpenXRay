#pragma once

#include "Include/xrRender/FactoryPtr.h"
#include "Include/xrRender/EnvironmentRender.h"
#include "xrCore/_vector3d.h"
#include "xrCore/_quaternion.h"
#include "xrCommon/xr_vector.h"
#include "xrCommon/xr_map.h"
#include "xrSound/Sound.h"
#include "editor_base.h"

// refs
class ENGINE_API IRender_Visual;
class ENGINE_API CInifile;
class ENGINE_API CEnvironment;

// refs - effects
class ENGINE_API CEnvironment;
class ENGINE_API CLensFlare;
class ENGINE_API CEffect_Rain;
class ENGINE_API CEffect_Thunderbolt;

class ENGINE_API CPerlinNoise1D;

struct SThunderboltDesc;
struct SThunderboltCollection;
class CLensFlareDescriptor;

#define DAY_LENGTH 86400.f

// t-defs
class ENGINE_API CEnvModifier
{
public:
    Fvector3 position;
    float radius;
    float power;

    float far_plane;
    Fvector3 fog_color;
    float fog_density;
    Fvector3 ambient;
    Fvector3 sky_color;
    Fvector3 hemi_color;
    Flags16 use_flags;

    void load(IReader* fs, u32 version);
    float sum(CEnvModifier& _another, Fvector3& view);
};

class ENGINE_API CEnvAmbient
{
public:
    struct SEffect
    {
        u32 life_time;
        ref_sound sound;
        shared_str particles;
        Fvector offset;
        float wind_gust_factor;
        float wind_blast_in_time;
        float wind_blast_out_time;
        float wind_blast_strength;
        Fvector wind_blast_direction;
    };
    using EffectVec = xr_vector<SEffect*>;

    struct ENGINE_API SSndChannel
    {
        shared_str m_load_section;
        Fvector2 m_sound_dist;
        Ivector4 m_sound_period;

        void load(const CInifile& config, pcstr sect, pcstr sectionToReadFrom = nullptr);

        [[nodiscard]]
        ref_sound& get_rnd_sound() { return m_sounds[Random.randI(m_sounds.size())]; }

        [[nodiscard]]
        u32 get_rnd_sound_time() const
        {
            return (m_sound_period.z < m_sound_period.w) ? Random.randI(m_sound_period.z, m_sound_period.w) : 0;
        }

        [[nodiscard]]
        u32 get_rnd_sound_first_time() const
        {
            return (m_sound_period.x < m_sound_period.y) ? Random.randI(m_sound_period.x, m_sound_period.y) : 0;
        }

        [[nodiscard]]
        float get_rnd_sound_dist() const
        {
            return (m_sound_dist.x < m_sound_dist.y) ? Random.randF(m_sound_dist.x, m_sound_dist.y) : 0;
        }

        [[nodiscard]]
        auto& sounds() { return m_sounds; }

    protected:
        xr_vector<ref_sound> m_sounds;
    };
    using SSndChannelVec = xr_vector<SSndChannel*>;

protected:
    shared_str m_load_section;

    EffectVec m_effects;
    Ivector2 m_effect_period;

    SSndChannelVec m_sound_channels;
    shared_str m_ambients_config_filename;

public:
    const shared_str& name() { return m_load_section; }
    const shared_str& get_ambients_config_filename() { return m_ambients_config_filename; }
    virtual void load(const CInifile& ambients_config, const CInifile& sound_channels_config,
        const CInifile& effects_config, const shared_str& section);
    SEffect* get_rnd_effect() { return effects().empty() ? 0 : effects()[Random.randI(effects().size())]; }
    u32 get_rnd_effect_time() { return Random.randI(m_effect_period.x, m_effect_period.y); }
    virtual SEffect* create_effect(const CInifile& config, pcstr id);
    virtual SSndChannel* create_sound_channel(const CInifile& config, pcstr id, pcstr sectionToReadFrom = nullptr);
    virtual ~CEnvAmbient();
    void destroy();
    virtual EffectVec& effects() { return m_effects; }
    virtual SSndChannelVec& get_snd_channels() { return m_sound_channels; }
};

class ENGINE_API CEnvDescriptor
{
public:
    bool dont_save; // oh

    float exec_time;
    float exec_time_loaded;

    shared_str sky_texture_name;
    shared_str sky_texture_env_name;
    shared_str clouds_texture_name;

    FactoryPtr<IEnvDescriptorRender> m_pDescriptor;

    Fvector4 clouds_color;
    float clouds_rotation;
    Fvector3 sky_color;
    float sky_rotation;

    float far_plane;

    Fvector3 fog_color;
    float fog_density;
    float fog_distance;

    float rain_density;
    Fvector3 rain_color;

    float bolt_period;
    float bolt_duration;

    float wind_velocity;
    float wind_direction;

    Fvector3 ambient;
    Fvector4 hemi_color; // w = R2 correction
    Fvector3 sun_color;
    Fvector3 sun_dir;
    float sun_azimuth; // for dynamic sun dir
    bool use_dynamic_sun_dir;

    float m_fSunShaftsIntensity;
    float m_fWaterIntensity;

    // SkyLoader: trees wave
    float m_fTreeAmplitude { 0.005f };
    float m_fTreeSpeed     { 1.00f };
    float m_fTreeRotation  { 10.0f };
    Fvector3 m_fTreeWave   { 0.1f, 0.01f, 0.11f };

    CLensFlareDescriptor* lens_flare;
    SThunderboltCollection* thunderbolt;

    CEnvAmbient* env_ambient;

    CEnvDescriptor(shared_str const& identifier);

    void load(CEnvironment& environment, const CInifile& config, pcstr section = nullptr);
    void save(CInifile& config, pcstr section = nullptr) const;
    void copy(const CEnvDescriptor& src)
    {
        float tm0 = exec_time;
        float tm1 = exec_time_loaded;
        *this = src;
        exec_time = tm0;
        exec_time_loaded = tm1;
    }

    void ed_show_params(const CEnvironment& env); // ImGui editor

    void on_device_create();
    void on_device_destroy();

    shared_str m_identifier;
};

class ENGINE_API CEnvDescriptorMixer : public CEnvDescriptor
{
public:
    float weight;
    float modif_power;
    float fog_near;
    float fog_far;
    Fvector4 env_color;

    bool soc_style;

public:
    CEnvDescriptorMixer();

    virtual void lerp(CEnvironment& parent, CEnvDescriptor& A, CEnvDescriptor& B,
        float f, CEnvModifier& M, float m_power);

    static std::pair<Fvector3, float> calculate_dynamic_sun_dir(float fGameTime, float azimuth);

    void ed_show_params(const CEnvironment& env); // ImGui editor
};

class ENGINE_API CEnvironment : public xray::editor::ide_tool
{
    friend class dxEnvironmentRender;
    struct str_pred
    {
        bool operator()(const shared_str& x, const shared_str& y) const { return xr_strcmp(x, y) < 0; }
    };

public:
    struct EnvVec : xr_vector<CEnvDescriptor*>
    {
        bool soc_style{};
    };
    using EnvsMap = xr_map<shared_str, EnvVec, str_pred>;
    using EnvAmbVec = xr_vector<CEnvAmbient*>;

private:
    // clouds
    xr_vector<Fvector> CloudsVerts;
    xr_vector<u16> CloudsIndices;

    float NormalizeTime(float tm);
    float TimeDiff(float prev, float cur);
    float TimeWeight(float val, float min_t, float max_t);
    void SelectEnvs(EnvVec* envs, CEnvDescriptor*& e0, CEnvDescriptor*& e1, float tm);
    void SelectEnv(EnvVec* envs, CEnvDescriptor*& e, float tm);

public:
    static bool sort_env_pred(const CEnvDescriptor* x, const CEnvDescriptor* y) { return x->exec_time < y->exec_time; }
    static bool sort_env_etl_pred(const CEnvDescriptor* x, const CEnvDescriptor* y)
    {
        return x->exec_time_loaded < y->exec_time_loaded;
    }

protected:
    CPerlinNoise1D* PerlinNoise1D;

    float fGameTime{};

public:
    FactoryPtr<IEnvironmentRender> m_pRender;

    float wind_strength_factor{};
    float wind_gust_factor{};

    float wetness_factor{};

    // wind blast params
    float wind_blast_strength{};
    Fvector wind_blast_direction{};
    Fquaternion wind_blast_start_time;
    Fquaternion wind_blast_stop_time;
    float wind_blast_strength_start_value{};
    float wind_blast_strength_stop_value{};
    Fquaternion wind_blast_current;

    // Environments
    CEnvDescriptorMixer CurrentEnv;
    CEnvDescriptor* Current[2]{};

    bool bWFX{};
    float wfx_time;
    CEnvDescriptor* WFX_end_desc[2];

    EnvVec* CurrentWeather{};
    shared_str CurrentWeatherName;
    shared_str CurrentCycleName;

    EnvsMap WeatherCycles;
    EnvsMap WeatherFXs;
    xr_vector<CEnvModifier> Modifiers;
    EnvAmbVec Ambients;

    CEffect_Rain* eff_Rain{};
    CLensFlare* eff_LensFlare{};
    CEffect_Thunderbolt* eff_Thunderbolt{};

    float fTimeFactor;

    void SelectEnvs(float gt);

    void UpdateAmbient();
    virtual CEnvAmbient* AppendEnvAmb(const shared_str& sect, CInifile const* pIni = nullptr);

    void Invalidate();

public:
    CEnvironment();

    virtual ~CEnvironment();

    virtual void load();
    virtual void unload();
    void save() const;

    void mods_load();
    void mods_unload();

    void OnFrame();
    void on_tool_frame() override;
    void lerp();

    void RenderSky();
    void RenderClouds();
    void RenderFlares();
    void RenderLast();

    bool SetWeatherFX(shared_str name);
    bool StartWeatherFXFromTime(shared_str name, float time);
    bool IsWFXPlaying() { return bWFX; }
    void StopWFX();

    void SetWeather(shared_str name, bool forced = false);
    shared_str GetWeather() { return CurrentWeatherName; }
    void ChangeGameTime(float game_time);
    void SetGameTime(float game_time, float time_factor);

    void OnDeviceCreate();
    void OnDeviceDestroy();

    float GetGameTime() { return fGameTime; }
    void GetGameTime(u32& hours, u32& minutes, u32& seconds) const
    {
        SplitTime(fGameTime, hours, minutes, seconds);
    }

    void SplitTime(float time, u32& hours, u32& minutes, u32& seconds) const;

    // editor-related
    void ED_Reload();

    CInifile* m_ambients_config{};
    CInifile* m_sound_channels_config{};
    CInifile* m_effects_config{};

protected:
    virtual CEnvDescriptor* create_descriptor(shared_str const& identifier, CInifile const* config, pcstr section = nullptr);
    virtual void load_weathers();
    virtual void load_weather_effects();

    void load_level_specific_ambients();

    void save_weathers(CInifile* environment_config = nullptr) const;
    void save_weather_effects(CInifile* environment_config = nullptr) const;

private:
    pcstr tool_name() const override { return "Weather Editor"; }
};

ENGINE_API extern Flags32 psEnvFlags;
ENGINE_API extern float psVisDistance;
ENGINE_API extern float SunshaftsIntensity;
