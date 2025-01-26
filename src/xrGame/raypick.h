#pragma once

#include "script_game_object.h"
#include "script_game_object_impl.h"
#include "xrCDB/xr_collide_defs.h"

struct script_rq_result
{
    CScriptGameObject* O;
    float range;
    int element;

    // Material of tri of ray query result
    pcstr pMaterialName;
    u32 pMaterialFlags;

    // physics part
    float fPHFriction; 
    float fPHDamping; 
    float fPHSpring; 
    float fPHBounceStartVelocity; 
    float fPHBouncing; 
    float fFlotationFactor; 
    float fShootFactor; 
    float fShootFactorMP; 
    float fBounceDamageFactor; 
    float fInjuriousSpeed; 
    float fVisTransparencyFactor; 
    float fSndOcclusionFactor;
    float fDensityFactor;

    script_rq_result()
    {
        O = nullptr;
        range = 0;
        element = 0;
    };

    void set(collide::rq_result& R)
    {
        if (R.O)
        {
            IGameObject* go = smart_cast<IGameObject*>(R.O);
            if (go)
                O = go->lua_game_object();
        }
        range = R.range;
        element = R.element;

        // demonized: set material params of ray pick result
        auto pTri = Level().ObjectSpace.GetStaticTris() + R.element;
        auto pMaterial = GMLib.GetMaterialByIdx(pTri->material);
        auto pMaterialFlagsRQ = pMaterial->Flags;
  
        pMaterialFlags = pMaterialFlagsRQ.flags;
        pMaterialName = pMaterial->m_Name.c_str();

        fPHFriction = pMaterial->fPHFriction;
        fPHDamping = pMaterial->fPHDamping;
        fPHSpring = pMaterial->fPHSpring;
        fPHBounceStartVelocity = pMaterial->fPHBounceStartVelocity;
        fPHBouncing = pMaterial->fPHBouncing;
        fFlotationFactor = pMaterial->fFlotationFactor;
        fShootFactor = pMaterial->fShootFactor;
        fShootFactorMP = pMaterial->fShootFactorMP;
        fBounceDamageFactor = pMaterial->fBounceDamageFactor;
        fInjuriousSpeed = pMaterial->fInjuriousSpeed;
        fVisTransparencyFactor = pMaterial->fVisTransparencyFactor;
        fSndOcclusionFactor = pMaterial->fSndOcclusionFactor;
        fDensityFactor = pMaterial->fDensityFactor;
    };
};

// class for performing ray pick
struct CRayPick
{
    Fvector start_position;
    Fvector direction;
    float range;
    collide::rq_target flags;
    script_rq_result result;
    IGameObject* ignore;

    CRayPick();
    CRayPick(const Fvector& P, const Fvector& D, const float R, const collide::rq_target F, CScriptGameObject* I);

    void set_position(Fvector& P) { start_position = P; };
    void set_direction(Fvector& D) { direction = D; };
    void set_range(float R) { range = R; };
    void set_flags(collide::rq_target F) { flags = F; };
    void set_ignore_object(CScriptGameObject* I)
    {
        if (I) ignore = smart_cast<IGameObject*>(&(I->object()));
    }

    bool query();

    script_rq_result get_result() { return result; };
    CScriptGameObject* get_object() { return result.O; };
    float get_distance() { return result.range; };
    int get_element() { return result.element; };
};
