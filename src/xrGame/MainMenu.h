#pragma once

class CUIWindow;
class CUIDialogWnd;
class CUICursor;
class CUIMessageBoxEx;
class CGameSpy_HTTP;
class CGameSpy_Full;

class demo_info_loader;

#include "xrEngine/IInputReceiver.h"
#include "xrEngine/IGame_Persistent.h"
#include "UIDialogHolder.h"
#include "xrUICore/Callbacks/UIWndCallback.h"
#include "xrUICore/ui_base.h"
#include "DemoInfo.h"
#include "ai_space.h"

namespace gamespy_gp
{
class account_manager;
class login_manager;

} // namespace gamespy_gp
namespace gamespy_profile
{
class profile_store;
} // namespace gamespy_profile

struct Patch_Dawnload_Progress
{
    bool IsInProgress{};
    float Progress{};
    shared_str Status{ "" };
    shared_str FileName{ "" };

    [[nodiscard]] bool  GetInProgress() const { return IsInProgress; }
    [[nodiscard]] float GetProgress()   const { return Progress; }
    [[nodiscard]] pcstr GetStatus()     const { return Status.c_str(); }
    [[nodiscard]] pcstr GetFlieName()   const { return FileName.c_str(); }
};

class CMainMenu : public IMainMenu,
                  public IInputReceiver,
                  public pureRender,
                  public CDialogHolder,
                  public CUIWndCallback,
                  public CDeviceResetNotifier,
                  public CUIResetNotifier
{
    CUIDialogWnd* m_startDialog;

    enum
    {
        flRestoreConsole = (1 << 0),
        flRestorePause = (1 << 1),
        flRestorePauseStr = (1 << 2),
        flActive = (1 << 3),
        flNeedChangeCapture = (1 << 4),
        flRestoreCursor = (1 << 5),
        flGameSaveScreenshot = (1 << 6),
        flNeedVidRestart = (1 << 7),
        flNeedUIRestart = (1 << 8),
    };
    Flags16 m_Flags;
    string_path m_screenshot_name;
    u32 m_screenshotFrame;

    xr_vector<CUIWindow*> m_pp_draw_wnds;

    CGameSpy_Full* m_pGameSpyFull{};
    gamespy_gp::account_manager* m_account_mngr{};
    gamespy_gp::login_manager* m_login_mngr{};
    gamespy_profile::profile_store* m_profile_store{};

    demo_info_loader* m_demo_info_loader;

public:
    enum EErrorDlg
    {
        ErrInvalidPassword,
        ErrInvalidHost,
        ErrSessionFull,
        ErrServerReject,
        ErrCDKeyInUse,
        ErrCDKeyDisabled,
        ErrCDKeyInvalid,
        ErrDifferentVersion,
        ErrGSServiceFailed,
        ErrMasterServerConnectFailed,
        NoNewPatch,
        ConnectToMasterServer,
        SessionTerminate,
        LoadingError,
        DownloadMPMap,
        ErrMax,
        ErrNoError = ErrMax,
    };

    Patch_Dawnload_Progress m_sPDProgress;
    Patch_Dawnload_Progress* GetPatchProgress() { return &m_sPDProgress; }
    void CancelDownload();
    gamespy_gp::account_manager* GetAccountMngr() { return m_account_mngr; }
    gamespy_gp::login_manager* GetLoginMngr() { return m_login_mngr; }
    gamespy_profile::profile_store* GetProfileStore() { return m_profile_store; }

    CGameSpy_Full* GetGS() const { return m_pGameSpyFull; }

protected:
    EErrorDlg m_NeedErrDialog;
    u32 m_start_time;

    shared_str m_downloaded_mp_map_url;
    shared_str m_player_name;
    shared_str m_cdkey;

    xr_vector<CUIMessageBoxEx*> m_pMB_ErrDlgs;
    bool ReloadUI();

public:
    u32 m_deactivated_frame;
    void DestroyInternal(bool bForce) override;

    CMainMenu();
    virtual ~CMainMenu();

    void Activate(bool bActive) override;
    bool IsActive() const override;
    bool CanSkipSceneRendering() override;

    bool IgnorePause() override { return true; }

    void IR_OnActivate() override;
    void IR_OnDeactivate() override;

    void IR_OnMousePress(int btn) override;
    void IR_OnMouseRelease(int btn) override;
    void IR_OnMouseHold(int btn) override;
    void IR_OnMouseWheel(float x, float y) override;
    void IR_OnMouseMove(int x, int y) override;

    void IR_OnKeyboardPress(int dik) override;
    void IR_OnKeyboardRelease(int dik) override;
    void IR_OnKeyboardHold(int dik) override;

    void IR_OnTextInput(pcstr text) override;

    void IR_OnControllerPress(int dik, const ControllerAxisState& state) override;
    void IR_OnControllerRelease(int dik, const ControllerAxisState& state) override;
    void IR_OnControllerHold(int dik, const ControllerAxisState& state) override;

    bool OnRenderPPUI_query();
    void OnRenderPPUI_main();
    void OnRenderPPUI_PP();

    void OnRender() override;
    void OnFrame(void) override;

    bool UseIndicators() override { return false; }
    void OnDeviceCreate();

    void Screenshot(IRender::ScreenshotMode mode = IRender::SM_NORMAL, LPCSTR name = 0);
    void RegisterPPDraw(CUIWindow* w);
    void UnregisterPPDraw(CUIWindow* w);

    void SetErrorDialog(EErrorDlg ErrDlg);
    EErrorDlg GetErrorDialogType() const { return m_NeedErrDialog; }
    void CheckForErrorDlg();

    pcstr GetDebugType() override { return "CMainMenu"; }
    bool FillDebugTree(const CUIDebugState& debugState) override;

    void SwitchToMultiplayerMenu();

    void OnPatchCheck(bool success);

    void Show_DownloadMPMap(LPCSTR text, LPCSTR url);
    void OnDownloadMPMap_CopyURL(CUIWindow*, void*);
    void OnDownloadMPMap(CUIWindow*, void*);

    void OnSessionTerminate(LPCSTR reason);
    void OnLoadError(LPCSTR module);

    void Show_CTMS_Dialog();
    void Hide_CTMS_Dialog();

    void SetNeedVidRestart();
    void OnDeviceReset() override;
    void OnUIReset() override;

    LPCSTR GetGSVer();

    bool IsCDKeyIsValid();
    bool ValidateCDKey();

    LPCSTR GetPlayerName();
    LPCSTR GetCDKeyFromRegistry();

    demo_info const* GetDemoInfo(LPCSTR file_name);

    CEventNotifierCallback::CID m_script_reset_event_cid;
};

extern CMainMenu* MainMenu();
