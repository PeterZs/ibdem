# 作業ログ — ibdem (Fig. 5 再現)

## 最終更新: 2026-03-21

---

## 現在の状態

### ターゲット (論文 Fig. 5)
| スケール | 粒子数 | 破断フレーム |
|---------|--------|------------|
| Low     | ~532   | ~23        |
| Middle  | ~1800  | ~25        |
| High    | ~18000 | ~25        |

### 実際の粒子数・ボンド数
| スケール | 粒子数 | ボンド数 | r      |
|---------|--------|---------|--------|
| Low     | 504    | 1522    | 0.018  |
| Middle  | 2024   | 6856    | 0.011  |
| High    | 22725  | 84615   | 0.005  |

---

## 最新実行結果 (2026-03-21)

### Run C: b5gdrwxft — **最新ビルド** (loadVel=0.062, 40フレーム) ← これが正解
```
Scale Low     : fracture at frame 25  (target 23) △ +2フレーム
Scale Middle  : fracture at frame 22  (target 25) △ -3フレーム
Scale High    : fracture at frame 22  (target 25) △ -3フレーム
```
**重要な改善点:**
- maxSigma が y≈-0.001 (底面 y=0) に正しく現れている ✓
- sm (曲げ成分) が非ゼロ: fr5 で ~2900 Pa ✓ → 向きの更新が機能している ✓
- 全スケールが破断 ✓
- 破断フレームは 22-25 の範囲 (論文ターゲット 23-25) に近い ✓

**残課題: スケール順序**
- 論文: Low(23) → Middle(25) ≈ High(25) (Low が最初に破断)
- 現状: Middle(22) ≈ High(22) → Low(25) (Low が最後に破断)

### Run A: bf3micebw (古いビルド, 問題あり — 参考のみ)
sc1 (Middle) が fr14 まで応力ゼロ。古いビルドの問題。

### Run B: bns3wfx3s (別ビルド, 問題あり — 参考のみ)
sm=0 のまま。こちらも古いビルドの問題。

---

## 未解決の問題

### 問題1 (唯一の残課題): スケール破断順序が逆
- **現状**: Low=fr25、Middle=fr22、High=fr22 → Low が最後に破断
- **ターゲット**: Low=fr23、Middle=fr25、High=fr25 → Low が最初に破断
- **考察**: Low スケール (r=0.018) は粒子が大きく Y 方向の行数が少ない (~8行)。結果としてビーム断面内の応力分布が粗く、sn の累積が遅い可能性。High (r=0.005) は行数が多く (~30行)、より連続体に近い応力集中が生じる。
- **対策候補**:
  1. loadVel を Low だけ上げる (例: Low=0.080, Middle=High=0.055)
  2. maxIter を Low だけ増やして収束精度を上げる
  3. kn/kt の比を調整 (ただし論文パラメータに準拠する必要あり)

---

## これまでの主要修正履歴

| 修正内容 | ファイル | 効果 |
|---------|---------|------|
| shear energy 式を `angle(d0, qc⊙d0)` から `angle(d_actual, qc⊙d0)` に修正 | BondForce.cpp | 位置-向き結合が生まれ破断が発生するようになった |
| PCG に shear transverse stiffness を追加 | Simulation.cpp | PCG の安定性改善 |
| 外側交互ループ (block coordinate descent) 追加 | Simulation.cpp | 位置と向きを交互に最適化 |
| loadVel 調整: 0.1333 → 0.0533 → 0.070 → 0.062 | main.cpp | 破断フレームを 9→32→22→22 付近に調整 |
| Windows CP932 で ≈ 文字が文字化け | Simulation.cpp | ASCII 代替文字で修正 |

---

## 現在のパラメータ (main.cpp)

```cpp
// SimConfig: dt, E, nu, tauC, loadVel, gravity, maxIter, epsilon
{ 1e-3f, 1e7f, 0.3f, 3e4f, 0.062f, 0.0f, 15, 1e-4f }  // 全スケール共通
```

```cpp
// BeamConfig: L, H, W, r, density, E, nu, tauC, label
{ 1.0f, 0.15f, 0.12f, 0.018f, 1000.0f, 1e7f, 0.3f, 3e4f, "Low"    }
{ 1.0f, 0.15f, 0.12f, 0.011f, 1000.0f, 1e7f, 0.3f, 3e4f, "Middle" }
{ 1.0f, 0.15f, 0.12f, 0.005f, 1000.0f, 1e7f, 0.3f, 3e4f, "High"   }
```

---

## 次に調査すべき事項

1. **bondStress の y 依存性を確認**: なぜ底面 (y=0) に最大応力が出ないのか。全ボンドの y と sigma を出力してボンドの応力分布を確認する。

2. **Middle スケールの境界条件確認**: isLoad/isSupport タグが付いた粒子数を出力。searchRadius が r=0.011 で適切か確認。

3. **sm が常にゼロの原因**: gradientStep で向きが更新されているか確認。向きの勾配が実際にゼロかどうか。

4. **スケールごとに loadVel を変える案**: 論文が同一 loadVel を使っているか確認。破断フレームのスケール依存性を補正するために per-scale loadVel にする可能性。

---

## 実行方法 (Windows)

```powershell
# ビルド
cmake --build C:\Users\nobuo\Documents\github\nnkgw\ibdem\build --config Release

# 実行 (Windows Defender 回避)
powershell -Command "Unblock-File 'C:\Users\nobuo\Documents\github\nnkgw\ibdem\build\Release\ibdem.exe'; Start-Process -FilePath 'C:\Users\nobuo\Documents\github\nnkgw\ibdem\build\Release\ibdem.exe' -ArgumentList '-headless 40' -Wait -RedirectStandardOutput 'C:\Users\nobuo\ibdem_out.txt' -NoNewWindow; Get-Content 'C:\Users\nobuo\ibdem_out.txt'"
```
