////////////////////////////////////////////////////////////////////////////
//	Module 		: UIActorStateInfo.h
//	Created 	: 15.02.2008
//	Author		: Evgeniy Sokolov
//	Description : UI actor state window class
////////////////////////////////////////////////////////////////////////////

#ifndef UI_ACTOR_STATE_INFO_H_INCLUDED
#define UI_ACTOR_STATE_INFO_H_INCLUDED

#include "alife_space.h"
#include "xrUICore/Hint/UIHint.h"

class CUIProgressBar;
class CUIProgressShape;
class CUIStatic;
class CUIFrameWindow;
class CUIXml;
class UI_Arrow;
class CInventoryOwner;
class CActor;

class ui_actor_state_item;

class ui_actor_state_wnd final : public CUIWindow
{
private:
    typedef CUIWindow inherited;

    enum EStateType
    {
        stt_stamina = 0,
        stt_health,
        stt_bleeding,
        stt_radiation,
        stt_armor,
        stt_main,
        stt_fire,
        stt_radia,
        stt_acid,
        stt_psi,
        stt_wound,
        stt_fire_wound,
        stt_shock,
        stt_power,
        stt_count
    };
    ui_actor_state_item* m_state[stt_count]{};
    UIHint* m_hint_wnd{};

public:
    ui_actor_state_wnd() : CUIWindow(ui_actor_state_wnd::GetDebugType()) {}
    ~ui_actor_state_wnd() override;

    void init_from_xml(CUIXml& xml);
    void init_from_xml(CUIXml& xml, LPCSTR path);
    void UpdateActorInfo(CInventoryOwner* owner);
    void UpdateHitZone();

    virtual void Draw();
    virtual void Show(bool status);

    pcstr GetDebugType() override { return "ui_actor_state_wnd"; }

private:
    void update_round_states(EStateType stt_type, float initial, float max_power);
    void update_number(EStateType stt_type, float initial, float max_power);
};

class ui_actor_state_item final : public UIHintWindow
{
    typedef UIHintWindow inherited;

protected:
    CUIStatic* m_static{};
    CUIStatic* m_static2{};
    CUIStatic* m_static3{};
    CUIStatic* m_number{}; //DrSergei - numbers in addition to bars
    CUIProgressBar* m_progress{};
    CUIProgressShape* m_sensor{};
    UI_Arrow* m_arrow{};
    UI_Arrow* m_arrow_shadow{};
    float m_magnitude;

public:
    ui_actor_state_item();

    void init_from_xml(CUIXml& xml, LPCSTR path, bool critical = true);
    void init_from_xml_plain(CUIXml& xml, LPCSTR path);
    void init_from_xml_number_only(CUIXml& xml, pcstr path);

    bool set_text(float value); // 0..1
    bool set_number(float value); //DrSergei
    bool set_progress(float value); // 0..1
    bool set_progress_shape(float value); // 0..1
    int set_arrow(float value); // 0..1
    bool show_static(bool status, u8 number = 1);

    pcstr GetDebugType() override { return "ui_actor_state_item"; }
}; // class ui_actor_state_item

#endif // UI_ACTOR_STATE_INFO_H_INCLUDED
