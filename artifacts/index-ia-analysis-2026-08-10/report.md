# JungleRuins GPU IA/index-fetch 분석

## 결론

현재 기본 카메라에서 가장 효과가 큰 것은 **R32를 R16으로 바꾸는 것 자체가 아니라, 하위 LOD의 sparse vertex ID를 작은 연속 범위로 compact-remap하는 것**이다.

- 기준 Forward GPU 시간: 24.0620 ms
- LOD5+6 compact R32: 21.0152 ms, **-3.0468 ms / -12.66%**
- LOD5+6 compact R16: 20.9721 ms, **-3.0899 ms / -12.84%**
- compact 상태의 R32 -> R16 순수 추가 효과: **-0.0431 ms**, 측정 변동에 가까움
- QueenForest 02/05 LOD6만 compact R32: 22.3502 ms, **-1.7118 ms / -7.11%**

따라서 이 캡처에서 `PE -> L2` 병목을 단순히 index-buffer byte bandwidth로 해석하면 맞지 않는다. hot index range는 작고 같은 모델이 수천 번 인스턴싱되어 캐시에 남는다. 반면 sparse index가 넓은 vertex 범위를 찌르면서 post-transform cache와 vertex fetch locality를 훼손한다. R32 compact만으로 성능이 개선되고, 같은 compact 데이터에서 R16으로 절반을 줄여도 거의 변하지 않은 것이 그 증거다.

## 측정 조건

- 커밋: `ef8c0c16febf633bcb02bb75811424761f496ba1`
- 씬: `assets/cooked/JungleRuins.fjscene`
- 해상도: 1280 x 720
- 카메라: 씬의 기본 카메라, 입력 없이 고정
- 측정 범위: GPU timestamp로 `ForwardPass`만 측정
- 각 실험의 첫 200 프레임은 warm-up으로 제외
- D3D12 pipeline statistics와 GPU culling의 `bin_counts`를 readback

절대 시간은 사용자가 측정한 7.5 ms와 카메라/실행 조건이 다르므로 직접 비교하지 않는다. 동일 머신, 동일 실행 조건에서 한 A/B의 상대 차이를 판단 근거로 쓴다.

## 실제 프레임의 IA 부하

| 항목 | 값 |
|---|---:|
| visible instances | 722,037 |
| active mesh LODs | 136 |
| logical index invocations / `IAVertices` | 420,994,533 |
| `IAPrimitives` | 140,331,511 |
| baseline `VSInvocations` | 234,436,786 |
| baseline `PSInvocations` | 43,322,614 |
| R32 list 논리 index bytes | 1,683,978,132 B / 1,606.0 MiB |
| R16 list 논리 index bytes | 841,989,066 B / 803.0 MiB |

`IAVertices`가 `logical_indices`와 정확히 일치하므로 per-frame workload 계산이 실제 IA 통계와 검증됐다. 다만 이 수치는 논리적 요청량이며 실제 DRAM read bytes와 같지는 않다.

### 모델 x 인스턴스 가중치

| 모델 | logical indices | 비율 |
|---|---:|---:|
| QueenForest_02 | 135,222,174 | 32.12% |
| QueenForest_05 | 134,163,675 | 31.87% |
| RiverForest_05 | 33,257,199 | 7.90% |
| QueenForest_06 | 27,851,157 | 6.62% |
| RiverForest_01 | 27,133,392 | 6.45% |
| RiverForest_03 | 25,494,684 | 6.06% |

QueenForest 02와 05 두 모델만 전체 IA index invocation의 63.99%다. 반대로 정적 index 데이터의 31.60%를 차지하는 Banyan은 이 카메라에서 주요 부하가 아니다. 저장 용량 기준 우선순위와 프레임 성능 기준 우선순위가 완전히 다르다.

### 선택된 local LOD 가중치

| local LOD | logical indices | 비율 |
|---:|---:|---:|
| 0 | 3,761,742 | 0.89% |
| 1 | 4,441,971 | 1.06% |
| 2 | 2,977,728 | 0.71% |
| 3 | 19,984,635 | 4.75% |
| 4 | 34,464,018 | 8.19% |
| 5 | 77,359,509 | 18.38% |
| 6 | 278,004,930 | 66.04% |

LOD5+6이 84.41%를 차지하므로 여기만 compact해도 높은 효과가 난다.

## 16비트 판정에서 중요한 점

R16 가능 여부는 `index_count < 65536`이 아니라 **한 draw가 참조하는 최대 local vertex ID 또는 compact 후 unique vertex 수가 65,536 이하인지**로 결정된다. 인스턴스 수는 R16 가능 여부를 바꾸지 않지만, 그 변환의 성능 가치를 크게 바꾼다.

예를 들어 QueenForest_05 LOD6은 draw 전체 index count가 12,321개지만 leaves submesh가 ID 416,841까지 참조한다. 현재 값 그대로는 R16이 아니다. 그러나 해당 leaves가 실제로 쓰는 unique vertex는 1,796개뿐이므로 `0..1795`로 remap하면 쉽게 R16이 된다.

| 판정 | 정적 전체 index 비율 | 현재 프레임의 인스턴스 가중 비율 | 논리 byte 절감 |
|---|---:|---:|---:|
| 현재 ID 그대로 R16 | 3.49% | 16.09% | 8.04% |
| submesh compact-remap 후 R16 | 18.68% | 98.23% | 49.11% |
| 모든 draw를 partition하여 R16 | 100% | 100% | 50.00% |

정적 비율만 보면 R16의 가치가 작아 보이지만, 모델 x 인스턴스 가중으로 보면 compact-remap 대상이 거의 전체 프레임을 덮는다.

## GPU A/B 결과

| 변형 | 평균 | p50 | p95 | VS invocations | 기준 대비 |
|---|---:|---:|---:|---:|---:|
| baseline R32 sparse | 24.0620 ms | 24.0499 | 24.3337 | 234,436,786 | - |
| split R32, original IDs | 23.9247 ms | 23.8801 | 24.1696 | 234,436,786 | -0.1373 ms |
| Queen02/05 LOD6 compact R32 | 22.3502 ms | 22.2832 | 22.6447 | 218,801,672 | -1.7118 ms |
| LOD5+6 compact R32 | 21.0152 ms | 20.9812 | 21.3111 | 206,732,558 | -3.0468 ms |
| LOD5+6 compact R16 | 20.9721 ms | 20.9269 | 21.2486 | 206,732,558 | -3.0899 ms |

`split R32, original IDs`는 compact 실험과 같은 command 분리/순서를 쓰되 원래 sparse ID를 유지한다. baseline 대비 차이가 0.14 ms뿐이고 VS 호출 수도 그대로다. 따라서 LOD5+6 compact R32의 약 3 ms 개선은 draw 분리 때문이 아니라 compact-remap에서 나온다.

compact R32와 compact R16은 topology, vertex data, draw 순서가 같고 index element width만 다르다. 둘의 차이가 0.043 ms이므로 이 캡처에서 R16 width 자체는 우선순위가 낮다.

## 왜 compact-remap이 빨라졌는가

- 삼각형과 IA index invocation 수는 모든 실험에서 동일했다.
- `VSInvocations`는 234.44M -> 206.73M으로 27.70M, 11.82% 감소했다.
- 관측 post-transform cache hit 추정치는 `1 - VS/IA` 기준 44.31% -> 50.89%로 6.58%p 개선됐다.
- ACMR은 1.6706 -> 1.4732로 개선됐다.
- active index sequence의 이상적인 cache-16 시뮬레이션 ACMR은 1.3858이다. compact 결과가 sparse 결과보다 이 값에 훨씬 가까워졌다.

index equality/order는 바꾸지 않았고 ID와 vertex 배치만 조밀하게 만들었다. 따라서 현재 하드웨어 경로는 넓게 흩어진 vertex ID/주소에서 post-transform cache 충돌 또는 vertex fetch locality 손실을 크게 겪는 것으로 해석할 수 있다. 이것은 IA/PE에서 시작되는 문제지만, 해결 레버는 index 폭보다 index remap과 vertex locality다.

## 권장 구현

### 1. 가장 먼저: hot lower LOD를 compact R32로 cook

R16과 별도 pass를 도입하기 전에 cooker에서 선택한 하위 LOD submesh를 다음과 같이 바꾼다.

1. index sequence의 최초 등장 순서로 source vertex ID를 `0..N-1`에 매핑한다.
2. 참조되는 packed position/normal/UV만 연속 vertex block에 복사한다.
3. 기존 R32 index 값은 새 local ID로 교체한다.
4. `base_vertex`를 compact vertex block 시작점으로 바꾼다.
5. index count, triangle order, submesh/draw 수는 유지한다.

R32 index buffer 안에서 in-place로 값만 교체할 수 있으므로 production 구현은 실험용 두 번째 buffer/command class가 필요 없다. draw 순서도 그대로 유지할 수 있다.

### 2. 적용 범위

| 범위 | compact vertex 추가 | R16 적용 시 index 절감 | R16 포함 순증 | 현재 workload coverage |
|---|---:|---:|---:|---:|
| Queen02/05 LOD6 | 161,200 B | 49,368 B | 111,832 B | 58.40% |
| 모든 LOD6 | 4,010,320 B | 945,636 B | 3,064,684 B | 66.04% |
| 모든 LOD5+6 | 10,838,304 B | 2,880,756 B | 7,957,548 B | 84.41% |
| 모든 LOD4+ | 24,478,176 B | 6,726,312 B | 17,751,864 B | 92.60% |

가장 작은 첫 패치는 Queen02/05 LOD6이다. 약 157 KiB의 compact vertex 복사만으로 이 캡처에서 1.71 ms가 줄었다. 그 다음 LOD5+6 전체로 확대한다.

### 3. R16은 2단계

compact 작업을 하면 R16 변환은 자연스럽게 가능하지만, 현재 GPU 시간만을 위해 format/pass 복잡도를 먼저 넣을 이유는 약하다. LOD5+6 R16은 논리 index bytes를 42.21% 줄였지만 compact R32 대비 실측 이득은 0.043 ms였다.

다른 카메라/GPU에서 R16 이득이 확인되면 R16/R32 command class를 분리하거나 R16 전용 index buffer를 추가한다. 이때 submesh draw 개수 자체는 늘릴 필요가 없다.

### 4. triangle strip은 후순위

전체 submesh를 실제 `meshopt_stripify`한 결과:

- 정적 전체: 91.67M -> 82.01M indices, 10.54% 감소
- 현재 프레임 인스턴스 가중: 420.99M -> 343.94M, 18.30% 감소
- degenerate stitching 방식은 정적 전체에서 오히려 7.84% 증가

restart strip + LOD5+6 R16의 이론적 논리 byte 절감은 52.62%지만, compact R32가 먼저이고 strip은 topology/PSO 변경 대비 실측이 필요하다.

## 다음 측정에서 볼 것

1. 사용자가 7.5 ms를 측정한 정확한 카메라에서 같은 `bin_counts` 계측을 수집한다.
2. PIX에서 IA/PE-to-L2와 함께 `VSInvocations`, vertex-cache hit 관련 지표, L2 read sectors를 비교한다.
3. baseline / compact R32 / compact R16 세 변형을 같은 draw order로 비교한다.
4. compact R32가 계속 이기면 R16보다 LOD별 dense vertex layout을 정식 포맷으로 채택한다.
5. compact 후에도 primitive setup이 병목이면 triangle 수, LOD threshold, impostor 전환, meshlet culling을 조정한다.

## 산출물

- `benchmark_summary.csv`: 5개 GPU A/B의 timing과 pipeline statistics
- `baseline_ia_profile.csv`, `r32_split_original_ids_ia_profile.csv`, `r32_compact_queen02_05_lod6_ia_profile.csv`, `r32_compact_lod5plus_ia_profile.csv`, `r16_lod5plus_ia_profile.csv`: raw frame samples
- 각 profile에 대응하는 `*_ia_bins.csv`: mesh-LOD별 visible instance raw samples
- `active_mesh_lods.csv`: 기준 프레임의 136개 active mesh-LOD 상세
- `active_models.csv`: 모델 x 인스턴스 집계
- `selective_compaction.csv`: 선택 범위별 정적 비용과 동적 coverage
- `strategy_projection.csv`: R16/strip 조합의 논리 index byte projection
- `strip_submeshes.csv`, `strip_meshes.csv`, `strip_lods.csv`: 실제 stripify 결과
- `mesh_lods.csv`: global mesh-LOD와 submesh/R16/strip 통계 매핑

