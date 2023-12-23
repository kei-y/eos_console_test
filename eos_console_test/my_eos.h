#pragma once

#include <malloc.h>
#include <cassert>
#include <string>
#include <windows.h>

#include <eos_sdk.h>
#include <eos_auth.h>
#include <eos_lobby.h>
#include <eos_p2p.h>
#include "eos_error.h"
#include "eos_handle.h"
#include "eos_account.h"
#include "my_eos_p2p.h"
#include "timeout.h"

#include <iostream>

class EOS
{
public:
    // 一定時間待機、Ctrl+Cで中断可能
    static void WaitSignal(EOS& eos, int timeout = 0)
    {
        auto old = std::chrono::system_clock::now();

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

        if (timeout)
        {
            puts(std::format("wait({}ms)(break ctrl+c)", timeout).c_str());
        }
        else
        {
            puts("wait(break ctrl+c)");
        }

        while (true)
        {
            if (WAIT_TIMEOUT != WaitForSingleObject(g_sleep, 20))
            {
                break;
            }

            if (timeout && IsTimeout(old, timeout))
            {
                break;
            }

            eos.Update();
        }

        SetConsoleCtrlHandler(nullptr, FALSE);

        g_sleep.Release();
    }

public:
    /// @brief 簡易の非同期完了待ちとデータの受け取りをするためのクラス
    template <typename T> class Async
    {
        T          m_storage = {}; // 受け渡し用のストレージ
        eos::Error m_error;        // 動作とエラー状態

        EOS& m_eos;

    public:
    public:
        Async(EOS& eos) : m_eos(eos) {}

        void              SetComplete(eos::Error e) { m_error = e; }
        const eos::Error& GetError() const { return m_error; }

        void SetStorage(eos::Error e, T storage)
        {
            m_error   = e;
            m_storage = storage;
        }
        const T& GetStorage() const { return m_storage; }

        // 動作完了待機
        void Wait()
        {
            while (true)
            {
                if (!m_eos.IsInitialized())
                {
                    // エラー
                    return;
                }

                // eosの内部進行を進めるために呼び出す必要がある、本来は１フレームに一度程度呼び出すだけでよい
                m_eos.Update();

                if (m_error.IsComplete())
                {
                    // 完了フラグが立てられたら抜ける
                    return;
                }
                ::Sleep(10);
            }
        }
        EOS& GetEOS() { return m_eos; }
    };
    /// @brief Asyncで空オブジェクトとして扱うダミークラス
    struct None
    {
    };

    /// @brief ロビーのIDを管理する
    class Lobby
    {
        EOS& m_eos;

        std::string m_id;

        EOS_NotificationId m_lobby_member_status_received = 0;
        P2P                m_p2p;

    public:
        void OnLobbyMemberStatusReceivedCallbackInfo(const EOS_Lobby_LobbyMemberStatusReceivedCallbackInfo& data)
        {
            switch (data.CurrentStatus)
            {
                case EOS_ELobbyMemberStatus::EOS_LMS_CLOSED:
                    puts("EOS_LMS_CLOSED");
                    break;
                case EOS_ELobbyMemberStatus::EOS_LMS_DISCONNECTED:
                    puts("EOS_LMS_DISCONNECTED");
                    m_p2p.OnLeft(data.TargetUserId);
                    break;
                case EOS_ELobbyMemberStatus::EOS_LMS_JOINED:
                    m_p2p.OnJoined(data.TargetUserId);
                    puts("EOS_LMS_JOINED");
                    break;
                case EOS_ELobbyMemberStatus::EOS_LMS_KICKED:
                    puts("EOS_LMS_KICKED");
                    break;
                case EOS_ELobbyMemberStatus::EOS_LMS_LEFT:
                    puts("EOS_LMS_LEFT");
                    m_p2p.OnLeft(data.TargetUserId);
                    break;
                case EOS_ELobbyMemberStatus::EOS_LMS_PROMOTED:
                    puts("EOS_LMS_PROMOTED");
                    break;
                default:
                    puts("error");
                    break;
            }
        }

    public:
        Lobby(EOS& eos, EOS_LobbyId id)
            : m_eos(eos), m_id(id), m_p2p(m_eos.m_local_user_id, EOS_Platform_GetP2PInterface(m_eos.GetPlatform()), id)
        {
            auto lobby = EOS_Platform_GetLobbyInterface(m_eos.GetPlatform());

            // メンバーの状態が変化した際に呼び出される
            // このコードのように、ロビーごとに作るのではなく、
            // ロビー全体でひとつだけ登録し、選別後個別のロビーへ渡すような仕組みの方が望ましい
            {
                EOS_Lobby_AddNotifyLobbyMemberStatusReceivedOptions options;
                options.ApiVersion = EOS_LOBBY_ADDNOTIFYLOBBYMEMBERSTATUSRECEIVED_API_LATEST;

                auto h = EOS_Lobby_AddNotifyLobbyMemberStatusReceived(
                    lobby,
                    &options,
                    this,
                    [](const EOS_Lobby_LobbyMemberStatusReceivedCallbackInfo* data)
                    {
                        Lobby* lobby = (Lobby*)data->ClientData;
                        if (0 != strcmp(lobby->GetId(), data->LobbyId))
                        {
                            // 自分のロビーではなかったので処理しない
                            return;
                        }
                        lobby->OnLobbyMemberStatusReceivedCallbackInfo(*data);
                    });
                m_lobby_member_status_received = h;
            }

            {
                auto GetDetails = [](EOS_HLobby lobby, EOS_LobbyId id, EOS_ProductUserId local_id)
                {
                    EOS_Lobby_CopyLobbyDetailsHandleOptions options;
                    options.ApiVersion  = EOS_LOBBY_COPYLOBBYDETAILSHANDLE_API_LATEST;
                    options.LobbyId     = id;
                    options.LocalUserId = local_id;

                    EOS_HLobbyDetails details;

                    eos::Error r = EOS_Lobby_CopyLobbyDetailsHandle(lobby, &options, &details);
                    assert(r.IsSuccess());

                    return eos::Handle<EOS_LobbyDetailsHandle*>(details, EOS_LobbyDetails_Release);
                };

                auto details = GetDetails(lobby, id, m_eos.m_local_user_id);

                auto GetMemberCount = [](eos::Handle<EOS_LobbyDetailsHandle*> details)
                {
                    EOS_LobbyDetails_GetMemberCountOptions count_options;
                    count_options.ApiVersion = EOS_LOBBYDETAILS_GETMEMBERCOUNT_API_LATEST;

                    return EOS_LobbyDetails_GetMemberCount(details, &count_options);
                };
                auto GetMemberId = [](eos::Handle<EOS_LobbyDetailsHandle*> details, uint32_t index)
                {
                    EOS_LobbyDetails_GetMemberByIndexOptions options;
                    options.ApiVersion  = EOS_LOBBYDETAILS_GETMEMBERBYINDEX_API_LATEST;
                    options.MemberIndex = index;

                    return eos::EpicAccount<EOS_ProductUserId>(EOS_LobbyDetails_GetMemberByIndex(details, &options));
                };

                // 初期からいるものを参加済みに登録する
                for (uint32_t i = 0; i < GetMemberCount(details); i++)
                {
                    auto member_id = GetMemberId(details, i);

                    // 自分はリンクには登録しないようにする
                    if (member_id == m_eos.m_local_user_id)
                    {
                        continue;
                    }

                    m_p2p.OnJoined(member_id);
                }
            }
        }
        ~Lobby()
        {
            auto lobby = EOS_Platform_GetLobbyInterface(m_eos.GetPlatform());
            EOS_Lobby_RemoveNotifyLobbyMemberStatusReceived(lobby, m_lobby_member_status_received);
        }

        EOS_LobbyId GetId() const { return m_id.c_str(); };

        void Update() { m_p2p.Update(); }
    };

    class Search
    {
        EOS& m_eos;

    public:
        eos::Handle<EOS_HLobbySearch> m_search_handle;

    public:
        Search(EOS& eos, EOS_HLobbySearch search_handle)
            : m_eos(eos), m_search_handle(eos::Handle<EOS_HLobbySearch>(search_handle, EOS_LobbySearch_Release))
        {
        }

        void AddParameter(const EOS_Lobby_AttributeData& attr, EOS_EComparisonOp ope)
        {
            EOS_LobbySearch_SetParameterOptions options = {};

            options.ApiVersion   = EOS_LOBBYSEARCH_SETPARAMETER_API_LATEST;
            options.ComparisonOp = ope;
            options.Parameter    = &attr;

            const eos::Error r = EOS_LobbySearch_SetParameter(*m_search_handle, &options);
            assert(r.IsSuccess());
        }

        //// 以下結果のメソッド

        /// @brief 結果の個数を取得する
        uint32_t GetSearchResultCount()
        {
            EOS_LobbySearch_GetSearchResultCountOptions options;
            options.ApiVersion = EOS_LOBBYSEARCH_GETSEARCHRESULTCOUNT_API_LATEST;

            return EOS_LobbySearch_GetSearchResultCount(*m_search_handle, &options);
        };

        /// @brief 指定したインデクスのディティールハンドルを取得する
        /// @param index ディティールハンドルを取得したいインデクス
        /// @param details_handle ここに戻される
        eos::Error GetDetail(uint32_t index, eos::Handle<EOS_HLobbyDetails>& details_handle)
        {
            EOS_LobbySearch_CopySearchResultByIndexOptions options;
            options.ApiVersion = EOS_LOBBYSEARCH_COPYSEARCHRESULTBYINDEX_API_LATEST;
            options.LobbyIndex = index;
            EOS_HLobbyDetails _details_handle;
            const eos::Error  e = EOS_LobbySearch_CopySearchResultByIndex(m_search_handle, &options, &_details_handle);
            assert(e.IsSuccess());

            details_handle.Initialize(_details_handle, EOS_LobbyDetails_Release);

            return e;
        };

        /// @brief 検索結果を取り出しコンソールに出力する
        void ResultDump()
        {
            auto Details_GetAttributeCount = [](eos::Handle<EOS_HLobbyDetails> details)
            {
                EOS_LobbyDetails_GetAttributeCountOptions count_options = {};

                count_options.ApiVersion = EOS_LOBBYDETAILS_GETATTRIBUTECOUNT_API_LATEST;
                return EOS_LobbyDetails_GetAttributeCount(details, &count_options);
            };

            auto Details_GetAttribute = [](eos::Handle<EOS_HLobbyDetails> details, uint32_t index, LobbyAttribute& attr)
            {
                EOS_Lobby_Attribute* _attr;

                EOS_LobbyDetails_CopyAttributeByIndexOptions options;
                options.ApiVersion = EOS_LOBBYDETAILS_COPYATTRIBUTEBYINDEX_API_LATEST;
                options.AttrIndex  = index;
                const eos::Error e = EOS_LobbyDetails_CopyAttributeByIndex(details, &options, &_attr);
                assert(e.IsSuccess());

                attr.Initialize(_attr);
            };

            auto count = GetSearchResultCount();
            for (uint32_t i = 0; i < count; i++)
            {
                eos::Handle<EOS_HLobbyDetails> details;

                GetDetail(i, details);

                puts(std::format("index:{}[", i).c_str());
                LobbyAttribute attr;

                for (uint32_t attr_index = 0; attr_index < Details_GetAttributeCount(details); attr_index++)
                {
                    Details_GetAttribute(details, attr_index, attr);

                    auto s = std::format(" {} {}", attr.GetName(), attr.Dump());
                    puts(s.c_str());
                }
                puts("]");
            }
        }
    };

    /// @brief ロビーの属性を管理する
    class LobbyAttribute
    {
    private:
        eos::Handle<EOS_Lobby_Attribute*> m_attr;

    public:
        LobbyAttribute(EOS_Lobby_Attribute* attr) { Initialize(attr); }
        LobbyAttribute() {}

        ~LobbyAttribute() { Release(); }

    public:
        void Initialize(EOS_Lobby_Attribute* attr) { m_attr.Initialize(attr, EOS_Lobby_Attribute_Release); }
        void Release() { m_attr.Release(); }

        EOS_ELobbyAttributeType GetType() const { return m_attr->Data->ValueType; }

        const char* GetName() const { return m_attr->Data->Key; }

        int64_t AsInt64() const
        {
            assert(GetType() == EOS_ELobbyAttributeType::EOS_AT_INT64);
            return m_attr->Data->Value.AsInt64;
        }
        double AsDouble() const
        {
            assert(GetType() == EOS_ELobbyAttributeType::EOS_AT_DOUBLE);
            return m_attr->Data->Value.AsDouble;
        }
        bool AsBool() const
        {
            assert(GetType() == EOS_ELobbyAttributeType::EOS_AT_BOOLEAN);
            return m_attr->Data->Value.AsBool == EOS_TRUE;
        }
        const char* AsUtf8() const
        {
            assert(GetType() == EOS_ELobbyAttributeType::EOS_AT_STRING);
            return m_attr->Data->Value.AsUtf8;
        }

        std::string Dump() const
        {
            std::string r;

            switch (GetType())
            {
                case EOS_ELobbyAttributeType::EOS_AT_BOOLEAN:
                    r = AsBool() ? "true" : "false";
                    break;
                case EOS_ELobbyAttributeType::EOS_AT_DOUBLE:
                    r = std::to_string(AsDouble());
                    break;
                case EOS_ELobbyAttributeType::EOS_AT_INT64:
                    r = std::to_string(AsInt64());
                    break;
                case EOS_ELobbyAttributeType::EOS_AT_STRING:
                    r = AsUtf8();
                    break;
            }

            return r;
        }
    };

private:
    ::eos::Handle<EOS_HPlatform> m_platform;
    bool                         m_is_initialized = false;

    eos::EpicAccount<EOS_ProductUserId> m_local_user_id;

    std::list<std::weak_ptr<Lobby>> m_lobbies;

private:
#pragma region utility

    static std::string input(const std::string_view& ask)
    {
        std::cout << ask << std::endl;

        std::string r;
        std::cin >> r;
        return r;
    }

    /// @brief 認証タイプを取得する
    EOS_ELoginCredentialType GetCredentialType()
    {
        // ログインのための情報を受け取る
        puts("ログインタイプを選択");
        do
        {
            auto _login_type = input("0:EOS_LCT_Developer , 1:EOS_LCT_AccountPortal");

            char* endptr = nullptr;

            auto v = strtol(_login_type.c_str(), &endptr, 10);
            if (strlen(endptr) == 0)
            {
                switch (v)
                {
                    case 0:
                        return EOS_ELoginCredentialType::EOS_LCT_Developer;
                    case 1:
                        return EOS_ELoginCredentialType::EOS_LCT_AccountPortal;
                    default:
                        break;
                }
            }
        } while (true);
        assert(false);
        return EOS_ELoginCredentialType::EOS_LCT_AccountPortal;
    }

    static eos::Handle<EOS_Auth_Token*> CopyUserAuthToken(EOS_HAuth auth, EOS_EpicAccountId id)
    {
        EOS_Auth_CopyUserAuthTokenOptions options = {0};

        options.ApiVersion = EOS_AUTH_COPYUSERAUTHTOKEN_API_LATEST;

        EOS_Auth_Token* UserAuthToken = nullptr;
        EOS_Auth_CopyUserAuthToken(auth, &options, id, &UserAuthToken);

        return eos::Handle<EOS_Auth_Token*>(UserAuthToken, EOS_Auth_Token_Release);
    }

    class StaticUtility
    {
    public:
        static void* AllocateFunc(size_t SizeInBytes, size_t Alignment)
        {
#if defined(_WIN32)
            return _aligned_malloc(SizeInBytes, Alignment);
#else
            return aligned_alloc(Alignment, SizeInBytes);
#endif
        }
        static void* ReallocateFunc(void* p, size_t SizeInBytes, size_t Alignment)
        {
#if defined(_WIN32)
            return _aligned_realloc(p, SizeInBytes, Alignment);
#else
            return reallocalign(p, SizeInBytes, Alignment);
#endif
        }
        static void ReleaseFunc(void* p)
        {
#if defined(_WIN32)
            _aligned_free(p);
#else
            free(p);
#endif
        }
        static const char* GetTempDirectory()
        {
#ifdef _WIN32
            static char Buffer[1024] = {0};
            if (Buffer[0] == 0)
            {
                GetTempPathA(sizeof(Buffer), Buffer);
            }

            return Buffer;

#elif defined(__APPLE__)
            return "/private/var/tmp";
#else
            return "/var/tmp";
#endif
        }
    };

#pragma endregion utility

public:
    EOS() {}
    ~EOS() { Finalize(); }

    /// @brief 初期化
    void Initialize()
    {
        puts(__func__);
        {
            EOS_InitializeOptions options = {};

            options.ApiVersion               = EOS_INITIALIZE_API_LATEST;
            options.AllocateMemoryFunction   = StaticUtility::AllocateFunc;
            options.ReallocateMemoryFunction = StaticUtility::ReallocateFunc;
            options.ReleaseMemoryFunction    = StaticUtility::ReleaseFunc;
            options.ProductName              = SampleConstants::GameName;
            options.ProductVersion           = "1.0";
            options.Reserved                 = nullptr;
            options.SystemInitializeOptions  = nullptr;
            options.OverrideThreadAffinity   = nullptr;

            const eos::Error r = EOS_Initialize(&options);
            assert(r.IsSuccess());
        }

        {
            EOS_Platform_Options options = {};

            options.ApiVersion          = EOS_PLATFORM_OPTIONS_API_LATEST;
            options.bIsServer           = EOS_FALSE;
            options.EncryptionKey       = SampleConstants::EncryptionKey;
            options.OverrideCountryCode = nullptr;
            options.OverrideLocaleCode  = nullptr;
            options.Flags               = 0;
            options.CacheDirectory      = StaticUtility::GetTempDirectory();

            options.ProductId    = SampleConstants::ProductId;
            options.SandboxId    = SampleConstants::SandboxId;
            options.DeploymentId = SampleConstants::DeploymentId;

            options.ClientCredentials.ClientId     = SampleConstants::ClientCredentialsId;
            options.ClientCredentials.ClientSecret = SampleConstants::ClientCredentialsSecret;

            m_platform.Initialize(EOS_Platform_Create(&options), EOS_Platform_Release);
            assert((bool)m_platform);
        }
        m_is_initialized = (bool)m_platform;
    }

    /// @brief 認証する
    /// @return 認証情報を返す
    eos::Handle<EOS_Auth_Token*> Authorize()
    {
        puts(__func__);
        EOS_Auth_Credentials auth_credentials = {};

        std::string _auth_id, _auth_token;

        auth_credentials.ApiVersion = EOS_AUTH_CREDENTIALS_API_LATEST;

        if (IsDebuggerPresent())
        {
            auth_credentials.Type = EOS_ELoginCredentialType::EOS_LCT_Developer;

            auth_credentials.Id    = "localhost:8080";
            auth_credentials.Token = AUTH_CREDENTIALS_TOKEN;
        }
        else
        {
            // 利用したい認証を取得する
            auth_credentials.Type = GetCredentialType();

            switch (auth_credentials.Type)
            {
#if 0 // 二段階認証が必須になったので、開発アカウントではEOS_LCT_Passwordは事実上動作しない気がします
            case EOS_ELoginCredentialType::EOS_LCT_Password:
                assert(false);

                _auth_id    = input("ユーザー名を入力してください");
                _auth_token = input("パスワードを入力してください");

                auth_credentials.Id    = _auth_id.c_str();
                auth_credentials.Token = _auth_token.c_str();
                break;
#endif
                case EOS_ELoginCredentialType::EOS_LCT_Developer:
                    _auth_id = input("DevAuthToolに表示されているIPアドレスとポートを入力してください");
                    _auth_token = input("DevNameを入力してください");

                    auth_credentials.Id    = _auth_id.c_str();
                    auth_credentials.Token = _auth_token.c_str();
                    break;
                case EOS_ELoginCredentialType::EOS_LCT_AccountPortal:
                    break;
                default:
                    return eos::Handle<EOS_Auth_Token*>();
            }
        }
        // ログインを行い認証情報を得る

        {
            EOS_HAuth auth = EOS_Platform_GetAuthInterface(m_platform);

            EOS_Auth_LoginOptions options = {};

            options.ApiVersion  = EOS_AUTH_LOGIN_API_LATEST;
            options.ScopeFlags  = EOS_EAuthScopeFlags::EOS_AS_BasicProfile;
            options.Credentials = &auth_credentials;

            Async<eos::Handle<EOS_Auth_Token*>> async(*this);

            EOS_Auth_Login(auth,
                           &options,
                           &async,
                           [](const EOS_Auth_LoginCallbackInfo* data)
                           {
                               if (!EOS_EResult_IsOperationComplete(data->ResultCode))
                                   return;
                               auto a = (Async<eos::Handle<EOS_Auth_Token*>>*)data->ClientData;

                               EOS_HAuth auth = EOS_Platform_GetAuthInterface(a->GetEOS().GetPlatform());

                               // 認証用のトークンを保存する
                               auto h = CopyUserAuthToken(auth, data->LocalUserId);
                               a->SetStorage(eos::Error(data->ResultCode), h);
                           });

            async.Wait();

            assert(async.GetError().IsSuccess());

            return async.GetStorage();
        }
    }

    /// @brief トークンを利用してユーザーを作成する
    /// @return 作成したIDを返す
    eos::EpicAccount<EOS_ProductUserId> CreateUser(const EOS_ContinuanceToken& token)
    {
        puts(__func__);

        auto connect = EOS_Platform_GetConnectInterface(m_platform);

        EOS_Connect_CreateUserOptions options = {};

        options.ApiVersion       = EOS_CONNECT_CREATEUSER_API_LATEST;
        options.ContinuanceToken = token;

        Async<eos::EpicAccount<EOS_ProductUserId>> async(*this);

        EOS_Connect_CreateUser(connect,
                               &options,
                               &async,
                               [](const EOS_Connect_CreateUserCallbackInfo* data)
                               {
                                   if (!EOS_EResult_IsOperationComplete(data->ResultCode))
                                       return;

                                   auto a = (Async<eos::EpicAccount<EOS_ProductUserId>>*)data->ClientData;
                                   a->SetStorage(eos::Error(data->ResultCode), data->LocalUserId);
                               });

        async.Wait();
        assert(async.GetError().IsSuccess());

        return async.GetStorage();
    }

    /// @brief 認証情報を使って接続する
    /// @param auth_token 認証情報
    void Connect(eos::Handle<EOS_Auth_Token*> auth_token)
    {
        puts(__func__);
        // 認証情報を利用して接続を行う
        auto connect = EOS_Platform_GetConnectInterface(m_platform);

        EOS_Connect_Credentials  credentials = {};
        EOS_Connect_LoginOptions options     = {};

        credentials.ApiVersion = EOS_CONNECT_CREDENTIALS_API_LATEST;
        credentials.Token      = auth_token->AccessToken;
        credentials.Type       = EOS_EExternalCredentialType::EOS_ECT_EPIC;

        options.ApiVersion    = EOS_CONNECT_LOGIN_API_LATEST;
        options.Credentials   = &credentials;
        options.UserLoginInfo = nullptr;

        Async<EOS_Connect_LoginCallbackInfo> async(*this);
        EOS_Connect_Login(connect,
                          &options,
                          &async,
                          [](const EOS_Connect_LoginCallbackInfo* data)
                          {
                              if (!EOS_EResult_IsOperationComplete(data->ResultCode))
                                  return;
                              auto a = (Async<EOS_Connect_LoginCallbackInfo>*)data->ClientData;
                              a->SetStorage(eos::Error(data->ResultCode), *data);
                          });
        async.Wait();

        if (async.GetStorage().ResultCode == EOS_EResult::EOS_InvalidUser)
        {
            // EOS_InvalidUser の場合はユーザーがまだ存在していないのでContinuanceTokenを利用してユーザーを作成する
            m_local_user_id = CreateUser(async.GetStorage().ContinuanceToken);
        }
        else
        {
            m_local_user_id = async.GetStorage().LocalUserId;

            assert(async.GetError().IsSuccess());
        }
    }

    /// @brief ロビーを作成する
    std::shared_ptr<Lobby> LobbyCreate()
    {
        puts(__func__);

        auto lobby = EOS_Platform_GetLobbyInterface(m_platform);

        EOS_Lobby_CreateLobbyOptions options = {};

        options.ApiVersion            = EOS_LOBBY_CREATELOBBY_API_LATEST;
        options.bPresenceEnabled      = EOS_FALSE;
        options.LocalUserId           = m_local_user_id;
        options.MaxLobbyMembers       = 10;
        options.PermissionLevel       = EOS_ELobbyPermissionLevel::EOS_LPL_PUBLICADVERTISED;
        options.bAllowInvites         = false;
        options.bDisableHostMigration = EOS_FALSE;
        options.BucketId              = "BucketId::BucketId";

        Async<std::string> async(*this);

        EOS_Lobby_CreateLobby(lobby,
                              &options,
                              &async,
                              [](const EOS_Lobby_CreateLobbyCallbackInfo* data)
                              {
                                  if (!EOS_EResult_IsOperationComplete(data->ResultCode))
                                      return;
                                  auto a = (Async<std::string>*)data->ClientData;

                                  if (data->ResultCode == EOS_EResult::EOS_Success)
                                      a->SetStorage(eos::Error(data->ResultCode), data->LobbyId);
                                  else
                                      a->SetStorage(eos::Error(data->ResultCode), "");
                              });

        async.Wait();
        assert(async.GetError().IsSuccess());

        auto p = std::make_shared<Lobby>(*this, async.GetStorage().c_str());
        m_lobbies.push_back(p);
        return p;
    }

    /// @brief ロビーへ参加する
    std::shared_ptr<Lobby> LobbyJoin(eos::Handle<EOS_HLobbyDetails> to_join)
    {
        puts(__func__);

        auto lobby = EOS_Platform_GetLobbyInterface(m_platform);

        EOS_Lobby_JoinLobbyOptions options = {};

        options.ApiVersion         = EOS_LOBBY_JOINLOBBY_API_LATEST;
        options.bPresenceEnabled   = EOS_FALSE;
        options.LocalUserId        = m_local_user_id;
        options.LobbyDetailsHandle = to_join;

        Async<std::string> async(*this);

        EOS_Lobby_JoinLobby(lobby,
                            &options,
                            &async,
                            [](const EOS_Lobby_JoinLobbyCallbackInfo* data)
                            {
                                if (!EOS_EResult_IsOperationComplete(data->ResultCode))
                                    return;
                                auto a = (Async<std::string>*)data->ClientData;

                                if (data->ResultCode == EOS_EResult::EOS_Success)
                                    a->SetStorage(eos::Error(data->ResultCode), data->LobbyId);
                                else
                                    a->SetStorage(eos::Error(data->ResultCode), "");
                            });

        async.Wait();
        assert(async.GetError().IsSuccess());

        auto p = std::make_shared<Lobby>(*this, async.GetStorage().c_str());
        m_lobbies.push_back(p);
        return p;
    }

    /// @brief ロビーから抜ける
    void LobbyLeave(std::shared_ptr<Lobby> p)
    {
        puts(__func__);

        auto lobby = EOS_Platform_GetLobbyInterface(m_platform);

        Async<None> async(*this);

        EOS_Lobby_LeaveLobbyOptions options;
        options.ApiVersion  = EOS_LOBBY_LEAVELOBBY_API_LATEST;
        options.LobbyId     = p->GetId();
        options.LocalUserId = m_local_user_id;
        EOS_Lobby_LeaveLobby(lobby,
                             &options,
                             &async,
                             [](const EOS_Lobby_LeaveLobbyCallbackInfo* data)
                             {
                                 if (!EOS_EResult_IsOperationComplete(data->ResultCode))
                                     return;
                                 auto a = (Async<None>*)data->ClientData;
                                 a->SetStorage(eos::Error(data->ResultCode), None());
                             });
        async.Wait();
        assert(async.GetError().IsSuccess());
    }
    /// @brief ロビーから抜ける
    void LobbyDestroy(std::shared_ptr<Lobby> p)
    {
        puts(__func__);

        auto lobby = EOS_Platform_GetLobbyInterface(m_platform);

        Async<None> async(*this);

        EOS_Lobby_DestroyLobbyOptions options;
        options.ApiVersion  = EOS_LOBBY_DESTROYLOBBY_API_LATEST;
        options.LobbyId     = p->GetId();
        options.LocalUserId = m_local_user_id;
        EOS_Lobby_DestroyLobby(lobby,
                               &options,
                               &async,
                               [](const EOS_Lobby_DestroyLobbyCallbackInfo* data)
                               {
                                   if (!EOS_EResult_IsOperationComplete(data->ResultCode))
                                       return;
                                   auto a = (Async<None>*)data->ClientData;
                                   a->SetStorage(eos::Error(data->ResultCode), None());
                               });
        async.Wait();
        assert(async.GetError().IsSuccess());
    }

    /// @brief テスト用の属性を設定する
    void LobbySetAttributes(std::shared_ptr<Lobby> p, int number, int test_value)
    {
        puts(__func__);

        // 更新用インターフェイス作成
        eos::Handle<EOS_HLobbyModification> modification;
        {
            auto lobby = EOS_Platform_GetLobbyInterface(m_platform);

            EOS_HLobbyModification _modification;

            EOS_Lobby_UpdateLobbyModificationOptions options = {};

            options.ApiVersion  = EOS_LOBBY_UPDATELOBBYMODIFICATION_API_LATEST;
            options.LobbyId     = p->GetId();
            options.LocalUserId = m_local_user_id;

            const eos::Error e = EOS_Lobby_UpdateLobbyModification(lobby, &options, &_modification);
            assert(e.IsSuccess());

            modification.Initialize(_modification, EOS_LobbyModification_Release);
        }

        auto AddAttribute = [](eos::Handle<EOS_HLobbyModification> modification, const EOS_Lobby_AttributeData& attr)
        {
            EOS_LobbyModification_AddAttributeOptions options = {};
            options.ApiVersion                                = EOS_LOBBYMODIFICATION_ADDATTRIBUTE_API_LATEST;

            options.Attribute  = &attr;
            options.Visibility = EOS_ELobbyAttributeVisibility::EOS_LAT_PUBLIC;

            const eos::Error e = EOS_LobbyModification_AddAttribute(modification, &options);
            assert(e.IsSuccess());
        };

        EOS_Lobby_AttributeData attr;

        AddAttribute(modification, MakeAttribute(attr, "test", (int64_t)1));

        // 属性を適当に設定
        auto tm      = std::chrono::system_clock::now();
        auto tp_msec = std::chrono::duration_cast<std::chrono::milliseconds>(tm.time_since_epoch());

        AddAttribute(modification, MakeAttribute(attr, "now", (int64_t)tp_msec.count()));

        // サーバー側へ登録を行う
        {
            auto lobby = EOS_Platform_GetLobbyInterface(m_platform);

            Async<None> async(*this);

            EOS_Lobby_UpdateLobbyOptions options = {};

            options.ApiVersion              = EOS_LOBBY_UPDATELOBBY_API_LATEST;
            options.LobbyModificationHandle = modification;
            EOS_Lobby_UpdateLobby(lobby,
                                  &options,
                                  &async,
                                  [](const EOS_Lobby_UpdateLobbyCallbackInfo* data)
                                  {
                                      if (!EOS_EResult_IsOperationComplete(data->ResultCode))
                                          return;
                                      auto a = (Async<None>*)data->ClientData;
                                      a->SetStorage(eos::Error(data->ResultCode), None());
                                  });
            async.Wait();
            assert(async.GetError().IsSuccess());
        }
    }

    /// @brief ロビー検索インターフェイスを作成する
    std::shared_ptr<Search> LobbySearchCreate(int max_result)
    {
        auto lobby = EOS_Platform_GetLobbyInterface(m_platform);

        EOS_HLobbySearch search_handle = nullptr;

        EOS_Lobby_CreateLobbySearchOptions options = {};

        options.ApiVersion = EOS_LOBBY_CREATELOBBYSEARCH_API_LATEST;
        options.MaxResults = max_result;

        const eos::Error r = EOS_Lobby_CreateLobbySearch(lobby, &options, &search_handle);
        assert(r.IsSuccess());

        return std::make_shared<Search>(*this, search_handle);
    }

    /// @brief ロビー検索を実行する
    void LobbySearchExecute(std::shared_ptr<Search> p)
    {
        // EOS_LobbySearch_SetParameter
        // EOS_LobbySearch_SetTargetUserId
        // EOS_LobbySearch_SetLobbyId
        // が設定されていない状態で呼び出すと失敗する

        EOS_LobbySearch_FindOptions options;
        options.ApiVersion  = EOS_LOBBYSEARCH_FIND_API_LATEST;
        options.LocalUserId = m_local_user_id;

        Async<None> async(*this);

        EOS_LobbySearch_Find(*p->m_search_handle,
                             &options,
                             &async,
                             [](const EOS_LobbySearch_FindCallbackInfo* data)
                             {
                                 if (!EOS_EResult_IsOperationComplete(data->ResultCode))
                                 {
                                     return;
                                 }

                                 auto a = (Async<None>*)data->ClientData;
                                 a->SetStorage(eos::Error(data->ResultCode), None());
                             });

        async.Wait();
        assert(async.GetError().IsSuccess());
    }

    static EOS_Lobby_AttributeData& MakeAttribute(EOS_Lobby_AttributeData& a, const char* key, int64_t v)
    {
        internal_MakeAttribute(a, key, EOS_ELobbyAttributeType::EOS_AT_INT64).Value.AsInt64 = v;
        return a;
    }
    static EOS_Lobby_AttributeData& MakeAttribute(EOS_Lobby_AttributeData& a, const char* key, double v)
    {
        internal_MakeAttribute(a, key, EOS_ELobbyAttributeType::EOS_AT_DOUBLE).Value.AsDouble = v;
        return a;
    }
    static EOS_Lobby_AttributeData& MakeAttribute(EOS_Lobby_AttributeData& a, const char* key, bool v)
    {
        internal_MakeAttribute(a, key, EOS_ELobbyAttributeType::EOS_AT_BOOLEAN).Value.AsBool = v;
        return a;
    }
    static EOS_Lobby_AttributeData& MakeAttribute(EOS_Lobby_AttributeData& a, const char* key, const char* v)
    {
        internal_MakeAttribute(a, key, EOS_ELobbyAttributeType::EOS_AT_STRING).Value.AsUtf8 = v;
        return a;
    }

    static EOS_Lobby_AttributeData&
    internal_MakeAttribute(EOS_Lobby_AttributeData& a, const char* key, EOS_ELobbyAttributeType t)
    {
        a.ApiVersion = EOS_LOBBY_ATTRIBUTEDATA_API_LATEST;
        a.Key        = key;
        a.ValueType  = t;
        return a;
    }

    bool IsInitialized() const { return m_is_initialized; }

    // eosの内部進行を進めるために呼び出す必要がある、本来は１フレームに一度程度呼び出すだけでよい
    void Update()
    {
        EOS_Platform_Tick(m_platform);
        for (auto& l : m_lobbies)
        {
            if (auto p = l.lock())
            {
                p->Update();
            }
        }
    }

private:
    /// @brief 持っているハンドル類をすべて解放する
    void internal_Release() { m_platform.Release(); }

public:
    /// @brief 終了
    void Finalize()
    {
        internal_Release();

        if (m_is_initialized)
        {
            m_is_initialized = false;

            const eos::Error r = EOS_Shutdown();
            assert(r.IsSuccess());
        }
    }

    /// @brief プラットフォームハンドルを取得する
    ::eos::Handle<EOS_HPlatform> GetPlatform() { return m_platform; }
};
