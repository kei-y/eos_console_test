// eos_console_test.cpp
//
#define NOMINMAX

#include "../credentials.h"

#define AUTH_CREDENTIALS_TOKEN "HOSTING"
#include "my_eos.h"

#pragma comment(lib, "EOSSDK-Win64-Shipping.lib")

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
    EOS::WaitSignal(eos);

    for (auto l : lobbies)
    {
        eos.LobbyLeave(l);
    }
    //eos.LobbyDestroy(lobby);

    std::cout << "Hello World!\n";

    eos.Finalize();

    return 0;
}
