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

#include <unordered_map>
#include "timeout.h"

// TODO
// ユーザーグループの概念を導入、ProductIDのリストでいいかな
// P2Pのファイル分離作業
// もうちょっとマシなパケット送信をつける

class P2P
{

public:
    class Link
    {
        enum class STATE
        {
            INIT,       // 初期
            WAKEUP,     // 相手から受信できるまでダミーパケット送信
            WAKEUP_ACK, // 相手から受信できるまでACKダミーパケット送信
            KEEPALIVE,  // 安定状態、状態の監視や定点動作があれば
        };

        P2P&                                m_p2p;
        eos::EpicAccount<EOS_ProductUserId> m_product_id;
        STATE                               m_state = STATE::INIT;

        const static int32_t WAKE_INTERVAL = 100; // Wakeを呼び出す頻度

        const static int32_t KEEPALIVE         = 5 * 1000;  // 最終的にタイムアウトする時間
        const static int32_t PREWAKE_KEEPALIVE = 20 * 1000; // Wake前のタイムアウト時間

        std::chrono::system_clock::time_point m_interval_old;
        std::chrono::system_clock::time_point m_keepalive_old;

    public:
        Link(P2P& p2p, EOS_ProductUserId id) : m_p2p(p2p), m_product_id(id) {}

        const eos::EpicAccount<EOS_ProductUserId>& GetProductUserId() const { return m_product_id; }

        /// @brief パケットなどが届いて通信状態が成立しているのを確認できたら常時呼び出す
        void Keepalive()
        {
            puts(__func__);
            m_keepalive_old = std::chrono::system_clock::now();
        }

        void Update()
        {
            auto IntervalZero = []()
            { return std::chrono::system_clock::time_point(std::chrono::system_clock::time_point::duration::zero()); };

            switch (m_state)
            {
                case STATE::INIT:
                    m_state = STATE::WAKEUP;
                    // 初回はすぐに動作してほしいのですぐにタイムアウトとなる時間を指定する
                    m_interval_old  = IntervalZero();
                    m_keepalive_old = std::chrono::system_clock::now();
                    break;
                case STATE::WAKEUP:
                    // 相手から反応があるまでブートコード送信を繰り返す
                    // 中断する場合は、ここでタイムアウトしてロビーから切断する
                    if (IsTimeout(m_interval_old, WAKE_INTERVAL))
                    {
                        m_interval_old = std::chrono::system_clock::now();
                        puts("STATE::WAKEUP");

                        m_p2p.Wake(m_product_id);

                        if (m_p2p.GetEstablished(m_product_id) >= ESTABLISHED_LEVEL::WAKEUP)
                        {
                            // 接続が確立したので、通信監視へ
                            m_state = STATE::WAKEUP_ACK;

                            m_interval_old = IntervalZero();
                        }
                    }

                    // タイムアウト判定、ここはちょっと長めの判定をしたほうがいいのかもしれません
                    assert(!IsTimeout(m_keepalive_old, PREWAKE_KEEPALIVE));
                    break;
                case STATE::WAKEUP_ACK:
                    // 相手から反応があるまで２度目のブートコード送信を繰り返す
                    // 中断する場合は、ここでタイムアウトしてロビーから切断する
                    if (IsTimeout(m_interval_old, WAKE_INTERVAL))
                    {
                        m_interval_old = std::chrono::system_clock::now();
                        puts("STATE::WAKEUP_ACK");

                        m_p2p.Wake(m_product_id, true);

                        if (m_p2p.GetEstablished(m_product_id) >= ESTABLISHED_LEVEL::ALREADY_WAKEUP)
                        {
                            // 接続が確立したので、通信監視へ
                            m_state = STATE::KEEPALIVE;

                            m_interval_old = IntervalZero();
                        }
                    }

                    // タイムアウト判定
                    assert(!IsTimeout(m_keepalive_old, KEEPALIVE));
                    break;
                case STATE::KEEPALIVE:
                    // この状態はやることがとくにないので、必要に応じて監視などに利用する

                    // タイムアウト判定
                    assert(!IsTimeout(m_keepalive_old, KEEPALIVE));
                    break;
            }
        }
    };

private:
    enum ESTABLISHED_LEVEL
    {
        NONE = -1,

        WAKEUP         = 1,
        ALREADY_WAKEUP = 2,
    };

    /// @brief わかりやすく情報を付けるために、固定構造体を先頭に貼り付ける
    /// @note ※本来はもっと無駄を減らし効率よくやった方がよいです
    /// @note ビット単位でシリアライズをする、圧縮する、といった感じです
    struct Head
    {
        char data[4] = {0};

        ESTABLISHED_LEVEL established_level = ESTABLISHED_LEVEL::NONE;
    };

    const static int32_t KEEPALIVE_INTERVAL = 5000; // keepaliveを行う頻度

    eos::EpicAccount<EOS_ProductUserId>& m_local_user_id;

    std::chrono::system_clock::time_point m_keepalive_interval_old;

    std::unordered_map<std::string, std::shared_ptr<Link>> m_links;
    std::unordered_map<std::string, ESTABLISHED_LEVEL>     m_activates; // 接続状態のレベルを保持、

    std::unordered_map<std::string, std::vector<char>> m_received;

    EOS_HP2P         m_p2p;
    EOS_P2P_SocketId m_socket_id = {};

    /// @brief 受信処理
    void UpdateReceive()
    {
        // 受信の準備を行う
        EOS_P2P_ReceivePacketOptions options = {};

        options.ApiVersion       = EOS_P2P_RECEIVEPACKET_API_LATEST;
        options.LocalUserId      = m_local_user_id;
        options.MaxDataSizeBytes = 4096;
        options.RequestedChannel = nullptr;

        EOS_P2P_SocketId socket_id;
        socket_id.ApiVersion = EOS_P2P_SOCKETID_API_LATEST;

        uint8_t channel = 0;

        std::vector<char> data;
        data.resize(options.MaxDataSizeBytes);
        uint32_t byte_written = 0;

        while (true)
        {
            EOS_ProductUserId received_id;

            const auto r =
                EOS_P2P_ReceivePacket(m_p2p, &options, &received_id, &socket_id, &channel, data.data(), &byte_written);

            if (r == EOS_EResult::EOS_NotFound)
            {
                break;
            }
            assert(r == EOS_EResult::EOS_Success);

            const Head* head = (const Head*)data.data();

            const auto remote_user_id = eos::EpicAccount<EOS_ProductUserId>(received_id).ToString();

            if (auto p = GetLink(remote_user_id))
            {
                // 通信が届いたので延命させる
                p->Keepalive();
            }
            if (head->established_level != ESTABLISHED_LEVEL::NONE)
            {
                // wakeupのシグナルを毎回送るなどだいぶ手抜きだが、通信があったことをリンクの方へ伝達できればなんでもよい
                // 当然通信は前後する可能性もあるので、レベルが小さくならないようにしないといけない
                m_activates[remote_user_id] = head->established_level;
            }

            // このループ内で複雑な処理を動かしたくないので、
            // 別バッファへコピーし、通信結果の反映は別のルーチンで処理するようにコピーしてます
            auto& receiver = m_received[remote_user_id];
            receiver.resize(byte_written);
            memcpy(receiver.data(), data.data(), byte_written);

            puts(std::format("received {}:{}", remote_user_id, byte_written).c_str());
        }
    }

    /// @brief 指定IDとの接続が確立しているか
    int GetEstablished(const eos::EpicAccount<EOS_ProductUserId>& id)
    {
        auto iter = m_activates.find(id.ToString());
        if (m_activates.end() != iter)
        {
            return iter->second;
        }
        return -1;
    }

    /// @brief 指定したIDのリンクを取得する
    std::shared_ptr<Link> GetLink(EOS_ProductUserId id)
    {
        const auto _id = eos::Account<EOS_ProductUserId>::ToString(id);
        return GetLink(_id);
    }
    std::shared_ptr<Link> GetLink(const std::string& id)
    {
        auto iter = m_links.find(id);
        if (m_links.end() == iter)
        {
            return std::shared_ptr<Link>();
        }
        return iter->second;
    }

public:
    P2P(eos::EpicAccount<EOS_ProductUserId>& local_user_id, EOS_HP2P p2p, const char* sockname)
        : m_local_user_id(local_user_id), m_p2p(p2p)
    {
        m_socket_id.ApiVersion = EOS_P2P_SOCKETID_API_LATEST;
        strcpy_s(m_socket_id.SocketName, sockname);

        m_keepalive_interval_old = std::chrono::system_clock::now();
    }

    /// @brief 初回接続確立までのダミーパケット
    /// @param user_id 送信先
    /// @param is_ack 最初はfalse、trueには相手からの応答があった後に切り替える
    void Wake(EOS_ProductUserId user_id, bool is_ack = false)
    {
        puts(__func__);

        // 接続許可を出しておく
        {
            EOS_P2P_AcceptConnectionOptions options;
            options.ApiVersion   = EOS_P2P_ACCEPTCONNECTION_API_LATEST;
            options.LocalUserId  = m_local_user_id;
            options.RemoteUserId = user_id;

            options.SocketId = &m_socket_id;

            eos::Error r = EOS_P2P_AcceptConnection(m_p2p, &options);

            assert(r.IsSuccess());
        }

        Head head = {};

        head.established_level = is_ack ? ESTABLISHED_LEVEL::ALREADY_WAKEUP : ESTABLISHED_LEVEL::WAKEUP;
        Send(user_id, &head, sizeof(head), EOS_EPacketReliability::EOS_PR_ReliableUnordered);
    }

    /// @brief user_id に対してパケットを送信する
    /// @param user_id 送信先ID
    void Send(EOS_ProductUserId      user_id,
              const void*            mem,
              uint32_t               len,
              EOS_EPacketReliability reliability = EOS_EPacketReliability::EOS_PR_UnreliableUnordered)
    {
        if (!m_local_user_id.IsValid())
        {
            return;
        }

        {
            EOS_P2P_SendPacketOptions options;

            options.ApiVersion            = EOS_P2P_SENDPACKET_API_LATEST;
            options.LocalUserId           = m_local_user_id;
            options.RemoteUserId          = user_id;
            options.SocketId              = &m_socket_id;
            options.bAllowDelayedDelivery = EOS_FALSE;
            options.Channel               = 0;
            options.Reliability           = reliability;

            options.DataLengthBytes = len;
            options.Data            = mem;

            eos::Error r = EOS_P2P_SendPacket(m_p2p, &options);
            assert(r.IsSuccess());
        }
    }

    /// @brief ロビーから参加通知があった場合に呼び出される
    void OnJoined(EOS_ProductUserId id)
    {
        const auto _id = eos::Account<EOS_ProductUserId>::ToString(id);

        m_links[_id] = std::make_shared<Link>(*this, id);
    }
    /// @brief ロビーからいなくなったときに呼び出される
    void OnLeft(EOS_ProductUserId id)
    {
        const auto _id = eos::Account<EOS_ProductUserId>::ToString(id);

        m_links.erase(_id);
    }

    /// @brief 定期更新処理
    void Update()
    {
        UpdateReceive();

        // 受け取り済みのパケット情報を良い感じに必要な人に届ける、届けたらクリアしておく
        m_received.clear();

        for (auto& l : m_links)
        {
            l.second->Update();
        }

        // 開通確認が終わったユーザーと一定時間ごとに通信をおこなっておく
        // 通信がこなくなって一定時間すると切断処理を行うようにする
        // レシーブさえしていれば接続中と見なせばよいと思いますので、、
        // 常時通信しているようなゲームであれば専用のkeepalive処理は不要です
        if (IsTimeout(m_keepalive_interval_old, KEEPALIVE_INTERVAL))
        {
            m_keepalive_interval_old = std::chrono::system_clock::now();

            // このプロジェクトではパケットに必ずHeadをつけておく必要があるのでHeadを送っています
            // 特に良い方法ではないので、本番ではなるべく入れなくて良い方法で実装したほうがよいです
            Head head;

            const auto remote_users = GetActiveRemoteUsers();
            if (!remote_users.empty())
            {
                puts("KEEPALIVE(POST)");
            }
            for (auto id : remote_users)
            {
                Send(id, &head, sizeof(head));
            }
        }
    }

    /// @brief P2P接続中の全ProductUserIdを取得する
    std::vector<eos::EpicAccount<EOS_ProductUserId>> GetActiveRemoteUsers()
    {
        std::vector<eos::EpicAccount<EOS_ProductUserId>> r;
        for (auto& l : m_links)
        {
            // 相互接続が確認できたものだけを返す
            if (GetEstablished(l.second->GetProductUserId()) >= ESTABLISHED_LEVEL::ALREADY_WAKEUP)
            {
                r.push_back(l.second->GetProductUserId());
            }
        }

        return r;
    }
};
