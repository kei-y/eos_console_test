// eos_console_test.cpp
//
#define NOMINMAX

#include "../credentials.h"

#define AUTH_CREDENTIALS_TOKEN "JOIN"
#include "../eos_console_test/my_eos.h"

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

    // 反映されるまで少し時間がかかるようなので適当に待機します
    EOS::WaitSignal(eos, 15 * 1000);

    // 一番最新のロビーへ参加する
    puts("search 1");
    {
        auto search = eos.LobbySearchCreate(5);

        EOS_Lobby_AttributeData attr;

        search->AddParameter(EOS::MakeAttribute(attr, "now", std::numeric_limits<int64_t>::max()),
                             EOS_EComparisonOp::EOS_CO_DISTANCE);
        eos.LobbySearchExecute(search);

        search->ResultDump();

        assert(0 < search->GetSearchResultCount());

        eos::Handle<EOS_HLobbyDetails> details_handle;
        search->GetDetail(0, details_handle);

        lobbies.push_back(eos.LobbyJoin(details_handle));
    }

    puts("wait");
    // Ctrl+Cされるまで適当に待つ
    EOS::WaitSignal(eos, 15 * 1000);

    for (auto l : lobbies)
    {
        eos.LobbyLeave(l);
    }
    //eos.LobbyDestroy(lobby);
    lobbies.clear();

    std::cout << "Hello World!\n";

    eos.Finalize();

    return 0;
}
