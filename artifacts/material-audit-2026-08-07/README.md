# FastJungle material audit

입력: `assets/cooked/spatial-audit/JungleRuins-spatial-audit.fjscene` (`FJSCENE v8`) / `assets/cooked/spatial-audit/JungleRuins-spatial-audit.fjtex` (`FJTEX v3`)

## 결론

- Material 187개, cooked texture binding 644개, texture 242개를 전수 검사했습니다.
- material slot에서 참조되는 binding은 607개이고, cook 후 참조되지 않는 binding은 37개입니다.
- 참조되지 않는 binding 중 37개는 base-color alpha로 합쳐진 opacity A-channel binding입니다.
- binding → texture → mip → FJTEX payload 범위 검증: **PASS**.
- 하나도 사용되지 않는 texture slot: **emissive**.
- metallic constant가 0이 아닌 material: **0개**.
- metallic texture를 쓰는 material: **64개**.
- metallic slot 또는 nonzero constant가 있는 material: **64개**.
- 원본 metallic 채널에 실제 nonzero pixel이 있는 material: **0개**.
- metallic slot 64개는 모두 `MI_Terrain_X*_Y*`이며 같은 `Metallic-Roughness.png`의 B 채널을 사용합니다.
- 그 원본 B 채널은 min=max=0, nonzero pixel 0/1024이므로 실제 metallic 값은 전부 0입니다.

## Texture slot

| slot | assigned materials | missing materials | unique textures | payload valid | cooked channels |
|---|---:|---:|---:|---|---|
| base_color | 175 | 12 | 101 | True | RGB:138; RGBA:37 |
| normal | 174 | 13 | 99 | True | RGB:174 |
| roughness | 174 | 13 | 37 | True | G:64; R:110 |
| metallic | 64 | 123 | 1 | True | B:64 |
| opacity | 20 | 167 | 4 | True | R:20 |
| emissive | 0 | 187 | 0 | N/A |  |

Shader 의미: base color·opacity·emissive는 상수와 texture를 곱합니다. roughness·metallic은 texture가 있으면 material 상수를 완전히 덮어씁니다. normal은 texture-only입니다.

Material 상수 10개는 모두 CPU에서 GPU material buffer로 복사됩니다. 다만 현재 Forward shader는 IOR·specular·clearcoat·clearcoat roughness를 읽지 않습니다.

## Material constants

`default_count`는 compiler fallback/구조체 기본값과 같은 material 수입니다. texture 연결 시 base color와 emissive는 compiler가 곱셈용 neutral 값 1로 바꿀 수 있습니다.

| constant | Forward shader | default | default count | non-default | unique | min | max |
|---|---|---|---:|---:|---:|---|---|
| base_color | used (multiplied by base-color texture) | [0.18, 0.18, 0.18, 1.0] | 0 | 187 | 4 | [0.029557, 0.05127, 0.007499, 1.0] | [1.0, 1.0, 1.0, 1.0] |
| emissive | used (multiplied by emissive texture) | [0.0, 0.0, 0.0] | 187 | 0 | 1 | [0.0, 0.0, 0.0] | [0.0, 0.0, 0.0] |
| roughness | used unless roughness texture overrides it | 0.5 | 174 | 13 | 3 | 0.0 | 1.0 |
| metallic | used unless metallic texture overrides it | 0.0 | 187 | 0 | 1 | 0.0 | 0.0 |
| opacity | used (multiplies albedo alpha and opacity texture) | 1.0 | 187 | 0 | 1 | 1.0 | 1.0 |
| opacity_threshold | used by clip() | 0.0 | 130 | 57 | 2 | 0.0 | 0.5 |
| ior | uploaded but not read by Forward.ps.hlsl | 1.5 | 8 | 179 | 2 | 1.45 | 1.5 |
| specular | uploaded but not read by Forward.ps.hlsl | 0.5 | 93 | 94 | 4 | 0.0 | 1.333 |
| clearcoat | uploaded but not read by Forward.ps.hlsl | 0.0 | 187 | 0 | 1 | 0.0 | 0.0 |
| clearcoat_roughness | uploaded but not read by Forward.ps.hlsl | 0.01 | 0 | 187 | 1 | 0.03 | 0.03 |

## Metallic users

| id | material | runtime source | constant | texture | channel | source min..max | nonzero pixels | effective nonzero | LOD0 submeshes | meshes |
|---:|---|---|---:|---|---|---|---:|---|---:|---|
| 100 | MI_Terrain_X4_Y4 | texture_override | 0 | Metallic-Roughness.png | B | 0..0 | 0/1024 | False | 5 | E_X4_Y4_37<br>M_4x4_01<br>M_4x4_02<br>M_4x4_03<br>M_4x4_04 |
| 101 | MI_Terrain_X3_Y4 | texture_override | 0 | Metallic-Roughness.png | B | 0..0 | 0/1024 | False | 5 | E_X3_Y4_36<br>M_3x4_01<br>M_3x4_02<br>M_3x4_03<br>M_3x4_04 |
| 102 | MI_Terrain_X4_Y3 | texture_override | 0 | Metallic-Roughness.png | B | 0..0 | 0/1024 | False | 5 | E_X4_Y3_29<br>M_4x3_01<br>M_4x3_02<br>M_4x3_03<br>M_4x3_04 |
| 103 | MI_Terrain_X3_Y3 | texture_override | 0 | Metallic-Roughness.png | B | 0..0 | 0/1024 | False | 5 | E_X3_Y3_28<br>M_3x3_01<br>M_3x3_02<br>M_3x3_03<br>M_3x3_04 |
| 104 | MI_Terrain_X2_Y5 | texture_override | 0 | Metallic-Roughness.png | B | 0..0 | 0/1024 | False | 1 | E_X2_Y5_43 |
| 105 | MI_Terrain_X1_Y5 | texture_override | 0 | Metallic-Roughness.png | B | 0..0 | 0/1024 | False | 1 | E_X1_Y5_42 |
| 106 | MI_Terrain_X0_Y5 | texture_override | 0 | Metallic-Roughness.png | B | 0..0 | 0/1024 | False | 1 | E_X0_Y5_41 |
| 107 | MI_Terrain_X0_Y4 | texture_override | 0 | Metallic-Roughness.png | B | 0..0 | 0/1024 | False | 1 | E_X0_Y4_33 |
| 108 | MI_Terrain_X1_Y4 | texture_override | 0 | Metallic-Roughness.png | B | 0..0 | 0/1024 | False | 1 | E_X1_Y4_34 |
| 109 | MI_Terrain_X2_Y4 | texture_override | 0 | Metallic-Roughness.png | B | 0..0 | 0/1024 | False | 1 | E_X2_Y4_35 |
| 110 | MI_Terrain_X5_Y4 | texture_override | 0 | Metallic-Roughness.png | B | 0..0 | 0/1024 | False | 1 | E_X5_Y4_38 |
| 111 | MI_Terrain_X6_Y4 | texture_override | 0 | Metallic-Roughness.png | B | 0..0 | 0/1024 | False | 1 | E_X6_Y4_39 |
| 112 | MI_Terrain_X7_Y4 | texture_override | 0 | Metallic-Roughness.png | B | 0..0 | 0/1024 | False | 1 | E_X7_Y4_40 |
| 113 | MI_Terrain_X7_Y3 | texture_override | 0 | Metallic-Roughness.png | B | 0..0 | 0/1024 | False | 1 | E_X7_Y3_32 |
| 114 | MI_Terrain_X6_Y3 | texture_override | 0 | Metallic-Roughness.png | B | 0..0 | 0/1024 | False | 1 | E_X6_Y3_31 |
| 115 | MI_Terrain_X5_Y3 | texture_override | 0 | Metallic-Roughness.png | B | 0..0 | 0/1024 | False | 1 | E_X5_Y3_30 |
| 116 | MI_Terrain_X2_Y3 | texture_override | 0 | Metallic-Roughness.png | B | 0..0 | 0/1024 | False | 1 | E_X2_Y3_27 |
| 117 | MI_Terrain_X1_Y3 | texture_override | 0 | Metallic-Roughness.png | B | 0..0 | 0/1024 | False | 1 | E_X1_Y3_26 |
| 118 | MI_Terrain_X0_Y3 | texture_override | 0 | Metallic-Roughness.png | B | 0..0 | 0/1024 | False | 1 | E_X0_Y3_25 |
| 119 | MI_Terrain_X0_Y2 | texture_override | 0 | Metallic-Roughness.png | B | 0..0 | 0/1024 | False | 1 | E_X0_Y2_17 |
| 120 | MI_Terrain_X1_Y2 | texture_override | 0 | Metallic-Roughness.png | B | 0..0 | 0/1024 | False | 1 | E_X1_Y2_18 |
| 121 | MI_Terrain_X2_Y2 | texture_override | 0 | Metallic-Roughness.png | B | 0..0 | 0/1024 | False | 1 | E_X2_Y2_19 |
| 122 | MI_Terrain_X3_Y2 | texture_override | 0 | Metallic-Roughness.png | B | 0..0 | 0/1024 | False | 1 | E_X3_Y2_20 |
| 123 | MI_Terrain_X4_Y2 | texture_override | 0 | Metallic-Roughness.png | B | 0..0 | 0/1024 | False | 1 | E_X4_Y2_21 |
| 124 | MI_Terrain_X5_Y2 | texture_override | 0 | Metallic-Roughness.png | B | 0..0 | 0/1024 | False | 1 | E_X5_Y2_22 |
| 125 | MI_Terrain_X6_Y2 | texture_override | 0 | Metallic-Roughness.png | B | 0..0 | 0/1024 | False | 1 | E_X6_Y2_23 |
| 126 | MI_Terrain_X7_Y2 | texture_override | 0 | Metallic-Roughness.png | B | 0..0 | 0/1024 | False | 1 | E_X7_Y2_24 |
| 127 | MI_Terrain_X0_Y1 | texture_override | 0 | Metallic-Roughness.png | B | 0..0 | 0/1024 | False | 1 | E_X0_Y1_9 |
| 128 | MI_Terrain_X1_Y1 | texture_override | 0 | Metallic-Roughness.png | B | 0..0 | 0/1024 | False | 1 | E_X1_Y1_10 |
| 129 | MI_Terrain_X2_Y1 | texture_override | 0 | Metallic-Roughness.png | B | 0..0 | 0/1024 | False | 1 | E_X2_Y1_11 |
| 130 | MI_Terrain_X3_Y1 | texture_override | 0 | Metallic-Roughness.png | B | 0..0 | 0/1024 | False | 1 | E_X3_Y1_12 |
| 131 | MI_Terrain_X4_Y1 | texture_override | 0 | Metallic-Roughness.png | B | 0..0 | 0/1024 | False | 1 | E_X4_Y1_13 |
| 132 | MI_Terrain_X5_Y1 | texture_override | 0 | Metallic-Roughness.png | B | 0..0 | 0/1024 | False | 1 | E_X5_Y1_14 |
| 133 | MI_Terrain_X6_Y1 | texture_override | 0 | Metallic-Roughness.png | B | 0..0 | 0/1024 | False | 1 | E_X6_Y1_15 |
| 134 | MI_Terrain_X7_Y1 | texture_override | 0 | Metallic-Roughness.png | B | 0..0 | 0/1024 | False | 1 | E_X7_Y1_16 |
| 135 | MI_Terrain_X7_Y0 | texture_override | 0 | Metallic-Roughness.png | B | 0..0 | 0/1024 | False | 1 | E_X7_Y0_8 |
| 136 | MI_Terrain_X6_Y0 | texture_override | 0 | Metallic-Roughness.png | B | 0..0 | 0/1024 | False | 1 | E_X6_Y0_7 |
| 137 | MI_Terrain_X5_Y0 | texture_override | 0 | Metallic-Roughness.png | B | 0..0 | 0/1024 | False | 1 | E_X5_Y0_6 |
| 138 | MI_Terrain_X4_Y0 | texture_override | 0 | Metallic-Roughness.png | B | 0..0 | 0/1024 | False | 1 | E_X4_Y0_5 |
| 139 | MI_Terrain_X3_Y0 | texture_override | 0 | Metallic-Roughness.png | B | 0..0 | 0/1024 | False | 1 | E_X3_Y0_4 |
| 140 | MI_Terrain_X2_Y0 | texture_override | 0 | Metallic-Roughness.png | B | 0..0 | 0/1024 | False | 1 | E_X2_Y0_3 |
| 141 | MI_Terrain_X1_Y0 | texture_override | 0 | Metallic-Roughness.png | B | 0..0 | 0/1024 | False | 1 | E_X1_Y0_2 |
| 142 | MI_Terrain_X0_Y0 | texture_override | 0 | Metallic-Roughness.png | B | 0..0 | 0/1024 | False | 1 | E_X0_Y0_1 |
| 143 | MI_Terrain_X7_Y7 | texture_override | 0 | Metallic-Roughness.png | B | 0..0 | 0/1024 | False | 1 | E_X7_Y7_64 |
| 144 | MI_Terrain_X6_Y7 | texture_override | 0 | Metallic-Roughness.png | B | 0..0 | 0/1024 | False | 1 | E_X6_Y7_63 |
| 145 | MI_Terrain_X5_Y7 | texture_override | 0 | Metallic-Roughness.png | B | 0..0 | 0/1024 | False | 1 | E_X5_Y7_62 |
| 146 | MI_Terrain_X4_Y7 | texture_override | 0 | Metallic-Roughness.png | B | 0..0 | 0/1024 | False | 1 | E_X4_Y7_61 |
| 147 | MI_Terrain_X3_Y7 | texture_override | 0 | Metallic-Roughness.png | B | 0..0 | 0/1024 | False | 1 | E_X3_Y7_60 |
| 148 | MI_Terrain_X2_Y7 | texture_override | 0 | Metallic-Roughness.png | B | 0..0 | 0/1024 | False | 1 | E_X2_Y7_59 |
| 149 | MI_Terrain_X1_Y7 | texture_override | 0 | Metallic-Roughness.png | B | 0..0 | 0/1024 | False | 1 | E_X1_Y7_58 |
| 150 | MI_Terrain_X0_Y7 | texture_override | 0 | Metallic-Roughness.png | B | 0..0 | 0/1024 | False | 1 | E_X0_Y7_57 |
| 151 | MI_Terrain_X0_Y6 | texture_override | 0 | Metallic-Roughness.png | B | 0..0 | 0/1024 | False | 1 | E_X0_Y6_49 |
| 152 | MI_Terrain_X1_Y6 | texture_override | 0 | Metallic-Roughness.png | B | 0..0 | 0/1024 | False | 1 | E_X1_Y6_50 |
| 153 | MI_Terrain_X2_Y6 | texture_override | 0 | Metallic-Roughness.png | B | 0..0 | 0/1024 | False | 1 | E_X2_Y6_51 |
| 154 | MI_Terrain_X3_Y6 | texture_override | 0 | Metallic-Roughness.png | B | 0..0 | 0/1024 | False | 1 | E_X3_Y6_52 |
| 155 | MI_Terrain_X4_Y6 | texture_override | 0 | Metallic-Roughness.png | B | 0..0 | 0/1024 | False | 1 | E_X4_Y6_53 |
| 156 | MI_Terrain_X5_Y6 | texture_override | 0 | Metallic-Roughness.png | B | 0..0 | 0/1024 | False | 1 | E_X5_Y6_54 |
| 157 | MI_Terrain_X6_Y6 | texture_override | 0 | Metallic-Roughness.png | B | 0..0 | 0/1024 | False | 1 | E_X6_Y6_55 |
| 158 | MI_Terrain_X7_Y6 | texture_override | 0 | Metallic-Roughness.png | B | 0..0 | 0/1024 | False | 1 | E_X7_Y6_56 |
| 159 | MI_Terrain_X7_Y5 | texture_override | 0 | Metallic-Roughness.png | B | 0..0 | 0/1024 | False | 1 | E_X7_Y5_48 |
| 160 | MI_Terrain_X6_Y5 | texture_override | 0 | Metallic-Roughness.png | B | 0..0 | 0/1024 | False | 1 | E_X6_Y5_47 |
| 161 | MI_Terrain_X5_Y5 | texture_override | 0 | Metallic-Roughness.png | B | 0..0 | 0/1024 | False | 1 | E_X5_Y5_46 |
| 162 | MI_Terrain_X4_Y5 | texture_override | 0 | Metallic-Roughness.png | B | 0..0 | 0/1024 | False | 1 | E_X4_Y5_45 |
| 163 | MI_Terrain_X3_Y5 | texture_override | 0 | Metallic-Roughness.png | B | 0..0 | 0/1024 | False | 1 | E_X3_Y5_44 |

## Raw files

- `analyze_materials.py`: FJSCENE v8/FJTEX v3 분석 재현 스크립트
- `slot_stats.csv`: slot별 실제 binding/payload 통계
- `constant_stats.csv`: material 상수 분포
- `materials.csv`: 187개 material 전체 값과 모든 slot
- `metallic_users.csv`: metallic slot/constant 사용자와 원본 채널 실측값
- `texture_usage.csv`: 242개 cooked texture의 slot별 사용자와 payload
- `summary.json`: 검증 invariant와 집계

## Reproduce

```powershell
python analyze_materials.py --scene assets/cooked/spatial-audit/JungleRuins-spatial-audit.fjscene --output material-audit-2026-08-07
```
