#pragma once
#include "xrUICore/Windows/UIWindow.h"
#include "UIDialogHolder.h"

class CDialogHolder;

class CUIDialogWnd : public CUIWindow
{
private:
    typedef CUIWindow inherited;
    CDialogHolder* m_pParentHolder;

protected:
public:
    bool m_bWorkInPause;
    CUIDialogWnd(pcstr window_name);
    virtual ~CUIDialogWnd();

    virtual void Show(bool status);

    bool OnKeyboardAction(int dik, EUIMessages keyboard_action) override;
    bool OnControllerAction(int axis, const ControllerAxisState& state, EUIMessages controller_action) override;

    CDialogHolder* GetHolder() const { return m_pParentHolder; }
    void SetHolder(CDialogHolder* h) { m_pParentHolder = h; }

    virtual bool StopAnyMove() { return true; }
    virtual bool NeedCursor() const { return true; }
    virtual bool NeedCenterCursor() const { return true; }
    virtual bool WorkInPause() const { return m_bWorkInPause; }
    virtual bool Dispatch(int cmd, int param) { return true; }
    virtual void ShowOrHideDialog(bool bDoHideIndicators);
    virtual void ShowDialog(bool bDoHideIndicators);
    virtual void HideDialog();

    virtual bool IR_process();

    pcstr GetDebugType() override { return "CUIDialogWnd"; }
    void FillDebugInfo() override;
};
