# プロジェクトの描画構造

描画処理は、次のように上から下へ一方向に流れます。`framework` はゲームの車やモデルの詳細を知らず、`RacingGameScene` がそれらを管理します。

```text
main.cpp
  -> framework       ウィンドウ、Direct3D、1フレームの進行
     -> Scene         ゲーム画面の共通インターフェース
        -> RacingGameScene
           -> Player / CameraController
           -> static_mesh / skinned_mesh
```

## 1フレームの流れ

```text
framework::run
  -> update
     -> ImGui::NewFrame
     -> RacingGameScene::update
        -> Player::update
        -> CameraController::update
  -> render
     -> 画面と深度をクリア
     -> 共通の描画ステートを設定
     -> RacingGameScene::render
        -> update_scene_constants
        -> draw_models
        -> draw_hud
     -> ImGui::Render
     -> Present
```

## 定数バッファの分担

| スロット | 内容 | 更新する場所 |
| --- | --- | --- |
| `b0` | モデルごとのワールド行列・色 | `static_mesh` / `skinned_mesh` |
| `b1` | カメラのビュー射影行列・ライト・カメラ位置 | `RacingGameScene::update_scene_constants` |

モデル追加時は `draw_models()` に `render()` を1行足します。カメラやライトを変更するときは `update_scene_constants()` だけを確認すればよい設計です。

## クラスの責務

- `framework`: Direct3D初期化、フレーム開始・終了、共通描画ステート、シーンの実行。
- `RacingGameScene`: レースに必要なオブジェクトの生成、更新、描画順。
- `Player`: 入力から車の位置・角度・速度を計算。
- `CameraController`: 車の状態からカメラ位置・視線・FOVを計算。
- `static_mesh`: OBJを読み込み、静的モデルとして描画。
- `skinned_mesh`: FBXを読み込み、ノード変換を含めて描画。
