#pragma once
#include "xrUICore/TabControl/UITabButton.h"

class CUIRadioButton final : public CUITabButton
{
    typedef CUITabButton inherited;

public:
    virtual void InitButton(Fvector2 pos, Fvector2 size);
    virtual bool InitTexture(pcstr texture, bool fatal = true);
    void SendMessage(CUIWindow* pWnd, s16 msg, void* pData = nullptr) override;
    bool OnMouseDown(int mouse_btn) override;
    pcstr GetDebugType() override { return "CUIRadioButton"; }
};
