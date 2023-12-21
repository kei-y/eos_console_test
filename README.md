[eos_console_test](https://github.com/y-horiuchi-snd/eos_console_test)のコード元に、[Epic Online Serivces](https://dev.epicgames.com/ja/services)の[P2P機能](https://dev.epicgames.com/docs/ja/api-ref/interfaces/p-2-p)を使って簡単なP2P通信をしてみよう、といった趣旨の記事です

- [Epic Online Servicesって何？](#epic-online-servicesって何)
- [はじめに](#はじめに)
- [ビルド環境](#ビルド環境)
- [ビルド方法](#ビルド方法)
- [オリジナルからの変更点](#オリジナルからの変更点)
- [DevAuthToolの使い方](#devauthtoolの使い方)
  - [なぜ使うのか](#なぜ使うのか)
  - [マニュアル](#マニュアル)
  - [概要](#概要)
  - [できること](#できること)
  - [今回の使い方](#今回の使い方)
- [実行前準備](#実行前準備)
- [動作内容](#動作内容)
  - [マッチング主催者側（eos\_console\_test）](#マッチング主催者側eos_console_test)
  - [マッチング参加者側（eos\_console\_test\_join）](#マッチング参加者側eos_console_test_join)
- [最後に](#最後に)

# Epic Online Servicesって何？

  [Epic Games](https://ja.wikipedia.org/wiki/Epic_Games)が提供しているオンラインサービスです。ゲームやアプリケーションに組み込むことでオンラインゲームの開発が出来るミドルウエアで、現時点ではコンソール機を含み完全無償で提供されています。

[公式ドキュメント](https://dev.epicgames.com/docs/)
[【CEDEC2020】無料でオンラインゲーム開発 ～EOS を利用したゲーム開発～](https://www.youtube.com/watch?v=zm1y6azGmSk)
[Epic Online Services でできること](https://www.youtube.com/watch?v=ASfE0T_-cmI)
  

# はじめに
  オリジナルのeos_console_testは、とある勉強会のスライド内容補強のために用意したもので、

  なるべく構造を簡単になるよう、面倒そうな処理はなるべく省き、

  [Epic Online Serivces](https://dev.epicgames.com/ja/services)の[ロビー機能](https://dev.epicgames.com/docs/ja/api-ref/interfaces/lobby)が最低限確認できる、という状態を目的に作成したものです。

  提供コードとしてはスライドの内容に沿ったものであったので、これで良かった、と思うのですが、

  ロビーだけあってもしょうがないよなぁ、とも思うのでP2Pの部分も追加してみました。

  （~~許可をとるのがちょっと面倒だったため~~完全に業務外となっており、forkする形になっております）
  
  DevAuthToolを前提で複数のアプリを立ち上げて自動的にログイン状態とし、

  ロビーへの参加、

  簡単なP2P管理部分の実装

  といった内容を追加したものです

  動作させるにはマッチングホスト側と参加側で二名分、Epicアカウントが必要になりますので、事前に作成しておいてください、同じアカウントでマッチングを行うと正しく動作しません。

# ビルド環境

  動作確認を行ったツール、SDKはこちらです

  VisualStudio2022 17.8.2
  
  EOS-SDK-27379709-v1.16.1

# ビルド方法

  ビルド方法や事前準備は、fork以前のプロジェクトと同様となります
  
  [eos_console_test/README.md](https://github.com/y-horiuchi-snd/eos_console_test?tab=readme-ov-file#eos_console_test) こちらの項目をこなすとビルド出来るようになっております。

# オリジナルからの変更点

- マッチングホスティング側と参加側でプロジェクトを分離

  異なる部分のみをif文で分岐させて、共通処理で動作させてもいいのですが、

  動作順序が追いかけづらくなってしまうので分離しました

  それぞれのプロジェクトのmain()が上から順に実行されるだけになっています。

  相変わらず、EOS_Platform_Tickなどの定期呼び出し処理もその場で適当に呼び出していますが、

  こちらはわかりやすく完全待機にしたいという目的で現状実装にしているだけで、

  本来は1フレームに一度呼び出すような処理が望ましいです

- デバッグ時はDevAuthTool前提としてデバッグ中は認証を自動化

  ログイン大変なので、DevAuthToolで事前にログインしたものを固定の名前で使います。

  DevAuthToolのポートは8080で、マッチングホスト側はHOSTING、参加側はJOINという名前で事前に登録しておいてください

  （DevAuthToolはそろそろログイン状況を保存する機能を実装しても怒られないのではないか、と思います！）

- P2P処理の実装

  ロビーからのEOS_LMS_JOINEDなどを利用して

  P2Pのマッチング状態を作成します

  初期化、通信許可、開通のためのパケット送受信と相互確認、接続後は定期的に通信を行う、
  
  といった流れが簡易に実装されています

# DevAuthToolの使い方

## なぜ使うのか

  ログイン状態を保留できるようになるため、デバッグ実行時のログイン認証を大幅に簡略化できるようになります

## マニュアル

  [DevAuthTool](https://dev.epicgames.com/docs/ja/epic-account-services/developer-authentication-tool)

## 概要

  DevAuthToolはSDK内に入っています、

  SDK\Tools\EOS_DevAuthTool-win32-x64-1.1.0.zip

  が該当アプリケーションです

  DevAuthToolはEpicアカウントのログイン処理を代行（というか維持）してくれるツールです

  ゲームを起動するたびにログインするのは非常に大変なので、

  DevAuthToolでログインしておいて、維持されているトークンをかすめ取り、

  そのトークンを使ってログインし自分のアプリケーションのデバッグを行う、といったツールになっていると思われます

## できること

  DevAuthToolを起動し、デバッグで使用するユーザーでログインし、そのユーザーに名前を付けることで、

  EOS_LCT_DeveloperとDevAuthToolが起動しているIPとポート、ユーザーに付けた名前、を使って二段階認証などをすっとばしてデバッグ出来るようになります

  ビルドやデバッグ機材固有の情報をキーにすれば、入力をすべて飛ばしてデバッグを行うことも出来るようになります

## 今回の使い方

  DevAuthToolを起動し、ポート番号を8080に設定します

  登録を二つ作っていますが、すべて異なったEpic アカウントを用意し、それぞれログインする必要があります。

  ログインを選び、Epicアカウントにログインし、「HOSTING」と名前を付けます

![0.png](https://qiita-image-store.s3.ap-northeast-1.amazonaws.com/0/208256/fc1841ef-dd19-f4c6-c31b-72371041a10c.png)

  再びログインを選び、Epicアカウントにログインし、「JOIN」と名前を付けます

![1.png](https://qiita-image-store.s3.ap-northeast-1.amazonaws.com/0/208256/58178483-5a61-bdcb-0f82-60f7ce8275c3.png)

  両方ログインが完了すると、ツールの左側が↓のようになります

![2.png](https://qiita-image-store.s3.ap-northeast-1.amazonaws.com/0/208256/90735f52-97da-c275-5e49-b9a02bf6359f.png)

  これでDevAuthToolの準備は完了となります

# 実行前準備

  VisualStudio2022で実行するのですが、複数同時デバッグ実行の設定を行うと楽に実行できます

  ソリューションエクスプローラーのソリューションを右クリックし、

  ![10.png](https://qiita-image-store.s3.ap-northeast-1.amazonaws.com/0/208256/9eb56cab-6aea-3ef1-2e8e-f71e00f73bb7.png)

  「スタートアッププロジェクトの構成」を選択します

![11.png](https://qiita-image-store.s3.ap-northeast-1.amazonaws.com/0/208256/492f9cd2-058c-2b73-e1d5-7284c589479b.png)

  「スタートアッププロジェクト」の編集が出来るプロパティダイアログへ飛ばされるので、

  「マルチスタートアッププロジェクト」を動作するように選択し、

  ２つあるプロジェクトのアクションを両方とも「開始」に設定し、「OK」を押します

  これでデバッグ実行時に２つのアプリケーションが同時にデバッガにアタッチされた状態で立ち上がります。

  （両方ともデバッグした状態での実行できるようになります）

![12.png](https://qiita-image-store.s3.ap-northeast-1.amazonaws.com/0/208256/411fce5e-c0ab-be95-970f-9f94831ca721.png)

  コードに小細工をしデバッガに接続されている場合は、

  DevAuthToolを使い固有名で自動的に認証を動作するようにしてあるため、

  不要な場合は、毎度ログインするようにする等、良い感じに対処してください。


# 動作内容

## マッチング主催者側（eos_console_test）

  EOSを初期化、DevAuthTool経由で"HOSTING"という名前で認証し、ロビーを作成し待機する

  ロビーの属性には、"now"という日時情報を付与し、一番新しいロビーがわかるようにしています、
<details><summary>コード（折りたたまれています）</summary><div>

```c++
// ロビーを作成
auto lobby = eos.LobbyCreate();
// ロビーに属性を設定
eos.LobbySetAttributes(lobby, i, 1);
// 待機する
EOS::WaitSignal(eos);

void LobbySetAttributes(std::shared_ptr<Lobby> p, int number, int test_value)
{
    // nowという属性に現在の時間を設定する
    auto tm = std::chrono::system_clock::now();
    auto tp_msec = std::chrono::duration_cast<std::chrono::milliseconds>(tm.time_since_epoch());
    AddAttribute(modification, MakeAttribute(attr, "now", (int64_t)tp_msec.count()));
}
```
</div></details>

  これは、デバッガで強制的に終了するなどでロビーをつぶした場合、サーバに残り続けてしまうという問題対策も兼ねています

  （一番最新のものが識別できるので、デバッグ中に間違って参加しないようにできます）

  ロビーでは誰かが参加してくると、EOS_ELobbyMemberStatus::EOS_LMS_JOINEDが発火し、

  P2P::OnJoined() へ参加者情報が届きます

<details><summary>コード（折りたたまれています）</summary><div>

```c++
void Lobby::OnLobbyMemberStatusReceivedCallbackInfo(const EOS_Lobby_LobbyMemberStatusReceivedCallbackInfo& data)
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

```
</div></details>

  P2P::OnJoinedではその情報を元に、「P2P::Link」を作成し、EOS_P2P_AcceptConnectionを行います。

<details><summary>コード（折りたたまれています）</summary><div>

```c++
void P2P::OnJoined(EOS_ProductUserId id)
{
    const auto _id = eos::Account<EOS_ProductUserId>::ToString(id);
    m_links[_id] = std::make_shared<Link>(*this, id);
}

/// @brief 接続許可を設定します
void P2P::Link::AcceptConnection()
{
    EOS_P2P_AcceptConnectionOptions options;
    options.ApiVersion   = EOS_P2P_ACCEPTCONNECTION_API_LATEST;
    options.LocalUserId  = m_p2p.m_local_user_id;
    options.RemoteUserId = m_product_id;
    options.SocketId = &m_p2p.m_socket_id;
    eos::Error r = EOS_P2P_AcceptConnection(m_p2p.m_p2p, &options);
}
```
</div></details>

  P2P::Update では有効なP2P::Linkの定点動作を行っています

<details><summary>コード（折りたたまれています）</summary><div>

```c++
void P2P::Update()
{
    for (auto& l : m_links)
    {
        l.second->Update();
    }
}

void P2P::Link::Update()
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
```
</div></details>

  P2P::Linkの定点動作の役割は主に２つあり、

  - 初動時にESTABLISHED_LEVEL::WAKEUP -> ESTABLISHED_LEVEL::ALREADY_WAKEUP という通信を相互に行う、

  - 通信確認が終わったら待機状態に入る
  
  のふたつを行います（Link::Update()の部分になります）

  最初に相互通信の確立確認を行っているのは、

  相手側通信を受け入れるまで通信が行われず、
  
  到達したか不明瞭な状態となってしまいます、これを防ぐ目的です

  このタイミングはRUDPでも届かないようなので、儀式だと思ってやっておきましょう。

<details><summary>コード（折りたたまれています）</summary><div>

```c++
/// @brief 初回接続確立までのダミーパケットを送信する
/// @param user_id 送信先
/// @param is_ack 最初はfalse、trueには相手からの応答があった後に切り替える
void P2P::Wake(EOS_ProductUserId user_id, bool is_ack = false)
{
    puts(__func__);

    Head head = {};

    head.m_no = (++m_packet_no);

    head.established_level = is_ack ? ESTABLISHED_LEVEL::ALREADY_WAKEUP : ESTABLISHED_LEVEL::WAKEUP;
    Send(user_id, &head, sizeof(head), EOS_EPacketReliability::EOS_PR_ReliableOrdered);
}
/// @brief user_id に対してパケットを送信する
/// @param user_id 送信先ID
void P2P::Send(EOS_ProductUserId      user_id,
          const void*            mem,
          uint32_t               len,
          EOS_EPacketReliability reliability = EOS_EPacketReliability::EOS_PR_UnreliableUnordered)
{
    if (!m_local_user_id.IsValid())
    {
        return;
    }

    EOS_P2P_SendPacketOptions options = {};

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
```
</div></details>  接続が確立した後は、定期的に通信を行い接続が継続していることを確認しておきます

<details><summary>ホスティング側の動作ログ（折りたたまれています）</summary><div>

```text
Initialize
Authorize
Connect
LobbyCreate
LobbySetAttributes
wait(break ctrl+c)
EOS_LMS_JOINED
STATE::WAKEUP
Wake
STATE::WAKEUP
Wake
STATE::WAKEUP
Wake
STATE::WAKEUP
Wake
STATE::WAKEUP
Wake
STATE::WAKEUP
Wake
STATE::WAKEUP
Wake
STATE::WAKEUP
Wake
STATE::WAKEUP
Wake
STATE::WAKEUP
Wake
STATE::WAKEUP
Wake
STATE::WAKEUP
Wake
STATE::WAKEUP
Wake
STATE::WAKEUP
Wake
Keepalive
received 00022d5ff:12:12
Keepalive
received 00022d5ff:13:12
Keepalive
received 00022d5ff:14:12
STATE::WAKEUP
Wake
Keepalive
received 00022d5ff:15:12
STATE::WAKEUP_ACK
Wake
Keepalive
received 00022d5ff:16:12
STATE::WAKEUP_ACK
Wake
Keepalive
received 00022d5ff:17:12
KEEPALIVE(POST)
Keepalive
received 00022d5ff:18:12
KEEPALIVE(POST)
Keepalive
received 00022d5ff:19:12
KEEPALIVE(POST)
Keepalive
received 00022d5ff:20:12
KEEPALIVE(POST)
Keepalive
received 00022d5ff:21:12
KEEPALIVE(POST)
Keepalive
received 00022d5ff:22:12
KEEPALIVE(POST)
Keepalive
received 00022d5ff:23:12
KEEPALIVE(POST)
Keepalive
received 00022d5ff:24:12
KEEPALIVE(POST)
Keepalive
received 00022d5ff:25:12
EOS_LMS_LEFT
```

</div></details>


## マッチング参加者側（eos_console_test_join）

  EOSを初期化、DevAuthTool経由で"JOIN"という名前で認証します

  次に参加するロビーを探すのですが、EOS_EComparisonOp::EOS_CO_DISTANCEを使い、
  
  一番新しいもの順にソートして、0番目のロビーへ参加するようにしてあります、

  参加時にすでに参加済みのユーザーに対して P2P::OnJoined() を行い、「P2P::Link」を作成します

<details><summary>コード（折りたたまれています）</summary><div>

```c++
Lobby::Lobby() {
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
```

</div></details>

  この後は、ホストでの動作とほぼ同じ流れでの処理が行われます

  双方で、EOS_P2P_AcceptConnectionを行い、相互に通信を開始することで相互通信が確立できます。

<details><summary>参加側の動作ログ（折りたたまれています）</summary><div>

```text
  Initialize
Authorize
Connect
wait(10000ms)(break ctrl+c)
search 1
index:0[
 NOW 1703125307976
 TEST 1
]
LobbyJoin
wait
wait(45000ms)(break ctrl+c)
STATE::WAKEUP
Wake
STATE::WAKEUP
Wake
STATE::WAKEUP
Wake
STATE::WAKEUP
Wake
STATE::WAKEUP
Wake
STATE::WAKEUP
Wake
STATE::WAKEUP
Wake
STATE::WAKEUP
Wake
STATE::WAKEUP
Wake
STATE::WAKEUP
Wake
STATE::WAKEUP
Wake
STATE::WAKEUP
Wake
STATE::WAKEUP
Wake
STATE::WAKEUP
Wake
Keepalive
received 00024b70:17:12
STATE::WAKEUP
Wake
Keepalive
received 00024b70:18:12
STATE::WAKEUP_ACK
Wake
Keepalive
received 00024b70:19:12
KEEPALIVE(POST)
Keepalive
received 00024b70:20:12
KEEPALIVE(POST)
Keepalive
received 00024b70:21:12
KEEPALIVE(POST)
Keepalive
received 00024b70:22:12
KEEPALIVE(POST)
Keepalive
received 00024b70:23:12
KEEPALIVE(POST)
Keepalive
received 00024b70:24:12
KEEPALIVE(POST)
Keepalive
received 00024b70:25:12
KEEPALIVE(POST)
Keepalive
received 00024b70:26:12
KEEPALIVE(POST)
Keepalive
received 00024b70:27:12
LobbyLeave
KEEPALIVE(POST)
Hello World!
```

</div></details>

# 最後に

  Epic Online Servicesはかなり便利なオンラインサービスなのですが、

  資料や採用例の情報が少なく、少しだけでも貢献できればと考え記事と動作するサンプルコードを書いてみました。

  読んでいただいた方のなんらかの助力になっていれば幸いです。

  
