この `patches/0001-request-2m-phy-and-data-len.patch` は、ZMK リポジトリのアプリケーション（通常は `app/`）に適用するための git-style パッチです。

使い方（ZMK リポジトリのルートで実行）:

1) パッチを適用:

   git apply ../zmk-config-ZaruBall/patches/0001-request-2m-phy-and-data-len.patch

   （または `git am` を使ってコミットとして適用）

2) 必要に応じて `app/CMakeLists.txt` に `src/phy_update.c` を追加してビルドに含めます。

注意:
- Zephyr の API 名はバージョンによって異なる可能性があります。ビルドエラーが出た場合は、`bluetooth/conn.h` や `bluetooth/hci.h` を確認し、関数名を読み替えてください（例: `bt_conn_le_phy_update` / `bt_conn_set_phy`、`bt_conn_set_data_len` / `bt_conn_le_set_data_len`）。
- このパッチは「実装の例」を提供するものであり、ビルド・実動作は ZMK/Zephyr のバージョンに依存します。

サポートが必要なら、あなたの ZMK/Zephyr のバージョンを教えてください。対応する API 名に合わせてパッチを調整します。