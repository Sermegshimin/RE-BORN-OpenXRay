#pragma once
#include "xrUICore/Static/UILanimController.h"
#include "xrUICore/Static/UIStaticItem.h"
#include "xrUICore/Windows/UIWindow.h"
#include "xrUICore/Lines/UILines.h"

class CUIFrameWindow;
class CLAItem;
class CUIXml;

struct lanim_cont
{
    CLAItem* m_lanim;
    float m_lanim_start_time;
    float m_lanim_delay_time;
    Flags8 m_lanimFlags;
    void set_defaults();
};

struct lanim_cont_xf : public lanim_cont
{
    Fvector2 m_origSize;
    void set_defaults();
};

class XRUICORE_API CUIStatic : public CUIWindow, public ITextureOwner, public CUILightAnimColorConrollerImpl
{
    friend class CUIXmlInitBase;

private:
    typedef CUIWindow inherited;
    lanim_cont_xf m_lanim_xform;
    void EnableHeading_int(bool b) { m_bHeading = b; }

public:
    CUIStatic(pcstr window_name);
    ~CUIStatic() override;

    virtual void Draw();
    virtual void Update();
    virtual void OnFocusLost();

    virtual pcstr GetText() const { return const_cast<CUIStatic*>(this)->TextItemControl()->GetText(); }
    virtual void SetText(pcstr txt) { TextItemControl()->SetText(txt); }
    virtual void SetTextST(pcstr txt) { TextItemControl()->SetTextST(txt); }
    void SetFont(CGameFont* F) override { TextItemControl()->SetFont(F); }
    CGameFont* GetFont() override { return TextItemControl()->GetFont(); }

    u32 GetTextColor() { return TextItemControl()->GetTextColor(); }
    void SetTextColor(u32 color) { TextItemControl()->SetTextColor(color); }

    void SetTextComplexMode(bool mode = true) { TextItemControl()->SetTextComplexMode(mode); }
    void SetTextAlignment(ETextAlignment al) { TextItemControl()->SetTextAlignment(al); }
    void SetVTextAlignment(EVTextAlignment al) { TextItemControl()->SetVTextAlignment(al); }
    void SetEllipsis(bool mode) { TextItemControl()->SetEllipsis(mode); }
    void SetCutWordsMode(bool mode) { TextItemControl()->SetCutWordsMode(mode); }
    void SetTextOffset(float x, float y)
    {
        TextItemControl()->m_TextOffset.x = x;
        TextItemControl()->m_TextOffset.y = y;
    }

    virtual void CreateShader(LPCSTR tex, LPCSTR sh = "hud" DELIMITER "default");
    ui_shader& GetShader() { return m_UIStaticItem.GetShader(); };
    virtual void SetTextureColor(u32 color) { m_UIStaticItem.SetTextureColor(color); }
    virtual u32 GetTextureColor() const { return m_UIStaticItem.GetTextureColor(); }
    virtual void SetTextureRect(const Frect& r) { m_UIStaticItem.SetTextureRect(r); }
    virtual const Frect& GetTextureRect() const { return m_UIStaticItem.GetTextureRect(); }
    virtual bool InitTexture(pcstr texture, bool fatal = true);
    virtual bool InitTextureEx(pcstr texture, pcstr shader = "hud" DELIMITER "default", bool fatal = true);
    CUIStaticItem* GetStaticItem() { return &m_UIStaticItem; }
    void SetTextureRect_script(Frect* pr) { m_UIStaticItem.SetTextureRect(*pr); }
    const Frect* GetTextureRect_script() { return &m_UIStaticItem.GetTextureRect(); }
    void SetHeadingPivot(const Fvector2& p, const Fvector2& offset, bool fixedLT)
    {
        m_UIStaticItem.SetHeadingPivot(p, offset, fixedLT);
    }
    void ResetHeadingPivot() { m_UIStaticItem.ResetHeadingPivot(); }
    virtual void SetTextureOffset(float x, float y) { m_TextureOffset.set(x, y); }
    Fvector2 GetTextureOffeset() const { return m_TextureOffset; }
    void TextureOn() { m_bTextureEnable = true; }
    void TextureOff() { m_bTextureEnable = false; }
    // own
    void SetXformLightAnim(LPCSTR lanim, bool bCyclic);
    void ResetXformAnimation();

    virtual void DrawTexture();
    virtual void DrawText();

    void AdjustHeightToText();
    void AdjustWidthToText();

    void SetShader(const ui_shader& sh);
    CUIStaticItem& GetUIStaticItem() { return m_UIStaticItem; }
    void SetStretchTexture(bool stretch_texture) { m_bStretchTexture = stretch_texture; }
    bool GetStretchTexture() { return m_bStretchTexture; }
    void SetHeading(float f) { m_fHeading = f; };
    float GetHeading() { return m_fHeading; }
    bool Heading() { return m_bHeading; }
    void EnableHeading(bool b) { m_bHeading = b; }
    void SetConstHeading(bool b) { m_bConstHeading = b; };
    bool GetConstHeading() { return m_bConstHeading; }
    virtual void ColorAnimationSetTextureColor(u32 color, bool only_alpha);
    virtual void ColorAnimationSetTextColor(u32 color, bool only_alpha);

    pcstr GetDebugType() override { return "CUIStatic"; }
    void FillDebugInfo() override;

protected:
    CUILines* m_pTextControl{};

    bool m_bStretchTexture{};
    bool m_bTextureEnable{ true };
    CUIStaticItem m_UIStaticItem;

    bool m_bHeading{};
    bool m_bConstHeading{};
    float m_fHeading{};

    Fvector2 m_TextureOffset;

public:
    CUILines* TextItemControl();
    shared_str m_stat_hint_text;
};
