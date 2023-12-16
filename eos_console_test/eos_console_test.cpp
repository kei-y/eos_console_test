// eos_console_test.cpp
//
#define NOMINMAX

#include "../credentials.h"

#define AUTH_CREDENTIALS_TOKEN "HOSTING"
#include "my_eos.h"

#pragma comment(lib, "EOSSDK-Win64-Shipping.lib")

void WaitSignal(EOS& eos)
{
    // Ctrl+Cされるまで適当に待つ
    auto close_handle = [](HANDLE h) { CloseHandle(h); };

    static eos::Handle<HANDLE> g_sleep;

    auto sigint_handler = [](DWORD control_type)
    {
        ReleaseSemaphore(g_sleep, 1, nullptr);
        return TRUE;
    };

    SetConsoleCtrlHandler(sigint_handler, TRUE);

    g_sleep.Initialize(CreateSemaphore(nullptr, 0, 1, nullptr), close_handle);

    puts("wait(break ctrl+c)");

    while (true)
    {
        if (WAIT_TIMEOUT != WaitForSingleObject(g_sleep, 100))
        {
            break;
        }
        EOS_Platform_Tick(eos.GetPlatform());
    }
}

int main(int argc, const char* argv[])
{
    EOS eos;

    // 初期化とプラットフォームハンドルを作成する
    eos.Initialize();
    {
        // 認証
        auto auth = eos.Authorize();
        eos.Connect(auth);
    }

    std::vector<std::shared_ptr<EOS::Lobby>> lobbies;

    // ロビーを数個作成する
    for (int i = 0; i < 1; i++)
    {
        auto lobby = eos.LobbyCreate();
        eos.LobbySetAttributes(lobby, i, 1);
        lobbies.push_back(lobby);
    }

    // Ctrl+Cされるまで適当に待つ
    WaitSignal(eos);

    for (auto l : lobbies)
    {
        eos.LobbyLeave(l);
    }
    //eos.LobbyDestroy(lobby);

    std::cout << "Hello World!\n";

    eos.Finalize();

    return 0;
}
