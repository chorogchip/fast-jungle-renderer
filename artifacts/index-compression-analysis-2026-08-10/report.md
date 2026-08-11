# JungleRuins 인덱스 압축 분석

## 결론

인덱스 압축은 두 계층으로 나누는 것이 가장 안전하다.

1. **디스크/스트리밍은 즉시 `meshoptimizer codec + Zstandard`를 적용한다.**
   현재 R32 인덱스 349.69 MiB가 `meshopt_encodeIndexBuffer + zstd level 3`에서
   26.98 MiB로 줄었다(92.29% 절감). 로딩 후 R32로 복원하면 렌더러 변경 없이 적용할 수 있다.
2. **GPU 인덱스 VRAM은 LOD 공유 청크 방식의 all-R16을 별도 실험한다.**
   LOD0 청크 목표를 57,344 정점, 상한을 65,536 정점으로 잡은 시뮬레이션에서
   R16 인덱스와 추가 정점을 합쳐 184.58 MiB였다. 현재 R32 인덱스 대비
   165.11 MiB(47.22%) 순절감이다. 단, 전체 draw range가 1,460개에서 약
   4,639개로 늘어날 수 있으므로 GPU culling/indirect draw 비용을 반드시 실측해야 한다.

단순 R16/R32 혼합은 6.10 MiB(1.75%)밖에 줄지 않아 구현 복잡도 대비 가치가 낮다.

## 분석 기준

- 최신 원격 코드: `ef8c0c16febf633bcb02bb75811424761f496ba1`
- 분석 씬: `assets/cooked/JungleRuins.fjscene`
- FJSCENE 버전: 12
- 파일 크기: 2,075,237,131 B (1.933 GiB)
- SHA-256: `0D86801FA0580654347808627C51FDD647A7E1961EDE4B75CD001439A6216D36`
- 인덱스는 서브메시 로컬 값이며 최종 정점은 `vertex_offset + index`이다.
- 압축 후보는 91,668,861개 인덱스 전체를 순회해 계산했다. 샘플링 결과가 아니다.
- meshoptimizer 코덱은 모든 서브메시를 encode/decode해 검증했다.

원본 작업 트리는 원격보다 7커밋 뒤였고 원격 변경과 로컬 수정 10개 파일이 겹쳤다.
사용자 변경을 보존하기 위해 원격 최신 커밋은 격리된 worktree에서 분석했다.

## 데이터 규모

| 항목 | 값 |
|---|---:|
| 정점 | 32,264,289 |
| 인덱스 | 91,668,861 |
| 삼각형 | 30,556,287 |
| 서브메시 | 1,460 |
| mesh LOD | 991 |
| mesh | 169 |
| 현재 R32 인덱스 | 366,675,444 B / 349.69 MiB |
| 씬 파일에서 인덱스 비율 | 17.67% |

인덱스 가중 비트폭은 18비트가 44.77%, 19비트가 17.60%, 22비트가
10.15%, 23비트가 21.03%를 차지한다. 16비트 이하인 인덱스는 3.49%뿐이다.
따라서 현재 서브메시 경계를 유지한 채 형식만 R16으로 바꾸는 접근은 효과가 작다.

인접 인덱스 절대 델타의 75.52%는 7 이하이고, 연속 삼각형의 55.91%는 최소
한 정점을 공유한다. 이 강한 국소성 때문에 블록/토폴로지 코덱의 압축률이 높다.

## 후보별 전체 크기

| 후보 | 크기 | R32 대비 절감 | 사용 조건 |
|---|---:|---:|---|
| 현재 R32 | 349.69 MiB | 0% | 현재 fixed-function IA |
| 서브메시 rebase R16/R32 혼합 | 343.58 MiB | 1.75% | 두 index format/pass 필요 |
| 서브메시 remap R16/R32 혼합 | 317.02 MiB | 9.34% | 정점 재작성 필요 |
| R24 | 262.27 MiB | 25.00% | IA에서 직접 사용 불가 |
| 서브메시 최소 비트패킹 | 212.79 MiB | 39.15% | custom decode 필요 |
| all-R16 순수 인덱스 | 174.84 MiB | 50.00% | 분할/정점 remap 필요 |
| LOD 공유 57,344 청크 all-R16 + 추가 정점 | 184.58 MiB | 47.22% | fixed-function IA 가능, draw 증가 |
| 96-index 블록 range bitpack | 110.62 MiB | 68.37% | custom decode 또는 load-time decode |
| ZigZag delta LEB128 | 103.90 MiB | 70.29% | 직렬 load-time decode |
| meshoptimizer index codec | 81.62 MiB | 76.66% | load-time decode, 삼각형 cyclic rotation 허용 |
| meshoptimizer sequence codec | 98.97 MiB | 71.70% | load-time decode, byte-exact index sequence |

`meshopt_optimizeVertexCacheStrip` 후 index codec은 81.44 MiB로 0.18 MiB만 더
줄었고, cache-16 ACMR은 1.9503에서 1.9529로 악화됐다. 별도 재정렬은 권하지 않는다.

## 디스크/스트리밍 압축

일반 압축만 적용한 R32와 meshoptimizer 전처리 후 결과를 비교했다.

| 입력 | 후처리 | 최종 크기 | R32 대비 절감 |
|---|---|---:|---:|
| R32 | LZ4 기본 | 247.47 MiB | 29.23% |
| R32 | zstd level 3 | 130.58 MiB | 62.66% |
| R32 | zstd level 9 | 106.57 MiB | 69.52% |
| meshopt index codec | LZ4 기본 | 36.09 MiB | 89.68% |
| meshopt index codec | zstd level 3 | 26.98 MiB | 92.29% |
| meshopt index codec | zstd level 9 | 25.15 MiB | 92.81% |
| meshopt sequence codec | zstd level 3 | 32.71 MiB | 90.65% |
| meshopt sequence codec | zstd level 9 | 30.57 MiB | 91.26% |

이 머신의 단일 실행 기준 index codec 디코드는 약 141 ms, 출력 기준 약
2.48 GiB/s였다. sequence codec 디코드는 약 152 ms, 약 2.29 GiB/s였다.
zstd level 3 후처리의 압축 해제는 각각의 meshopt 스트림에서 약 1 GiB/s였다.
수치는 warm file cache와 현재 CPU의 단일 실행 결과이며 실제 로딩 I/O는 포함하지 않는다.

### index codec과 sequence codec 선택

`meshopt_encodeIndexBuffer`는 일부 삼각형의 세 인덱스를 cyclic rotation한다.
방향, topology, primitive 순서는 유지돼 현재 vertex-only 렌더링에는 동일하지만,
인덱스와 병렬인 per-corner 데이터가 있으면 corner 대응이 달라질 수 있다.

- 현재 저장된 corner stream과 byte-exact 대응을 유지하려면 sequence codec을 쓴다.
- index codec을 쓰려면 cooker에서 회전된 corner 데이터도 함께 canonicalize하거나,
  해당 stream이 런타임에서 사용되지 않는다는 계약을 명시하고 검증한다.
- 두 선택의 zstd level 3 최종 차이는 5.73 MiB이므로 안전성이 중요하면 sequence codec이 합리적이다.

## GPU VRAM용 all-R16 시뮬레이션

LOD별 독립 remap은 같은 정점을 LOD마다 복제해 추가 packed vertex가
398,602,256 B 발생하고, R16 절감분보다 커져 약 205 MiB 순손해였다.

이를 피하려고 같은 `(vertex_offset, vertex_count)`를 쓰는 LOD들을 한 그룹으로 묶고:

1. LOD0을 목표 청크 크기로 분할한다.
2. 각 청크에 local R16 인덱스를 부여한다.
3. 하위 LOD 삼각형은 세 정점이 이미 들어 있는 청크를 우선 사용한다.
4. 경계를 넘는 하위 LOD 연결만 청크에 정점을 복제하며, 절대 상한은 65,536이다.

목표 청크 크기 스윕 결과는 다음과 같다.

| LOD0 목표 정점 | 정점 청크 | 추가 정점 | draw range | R16+추가 정점 | R32 대비 절감 |
|---:|---:|---:|---:|---:|---:|
| 32,768 | 1,033 | 699,197 | 7,071 | 185.51 MiB | 46.95% |
| 40,960 | 843 | 695,005 | 5,776 | 185.45 MiB | 46.97% |
| 49,152 | 702 | 672,642 | 4,823 | 185.11 MiB | 47.06% |
| **57,344** | **577** | **638,129** | **3,963** | **184.58 MiB** | **47.22%** |
| 61,440 | 1,630 | 642,588 | 5,693 | 184.65 MiB | 47.20% |
| 64,512 | 71,540 | 1,171,093 | 166,947 | 192.71 MiB | 44.89% |
| 65,536 | 95,451 | 1,393,273 | 233,618 | 196.10 MiB | 43.92% |

청크를 처음부터 65,536까지 채우면 하위 LOD의 경계 연결을 받아들일 여유가 없어
작은 보조 청크가 폭증한다. 57,344는 8,192정점(12.5%)의 headroom을 남겨 이 현상을
피한 현재 데이터의 최적점이다. 추가 정점은 대상 원본 정점의 2.03%인 638,129개,
packed 16-byte vertex 기준 9.74 MiB다.

다만 대상 LOD 그룹의 draw range는 784개에서 3,963개로 늘어난다. 영향을 받지 않는
676개를 합치면 전체는 약 1,460개에서 4,639개로 3.18배가 된다. 현재 GPU indirect
pipeline에서 CPU submission이 반드시 같은 비율로 증가하는 것은 아니지만,
command capacity, binning, culling, ExecuteIndirect 비용은 별도 벤치마크가 필요하다.

## 용량 우선순위

가장 큰 한 모델인 `STT_BanyanForest_ROP_Reduced_001_Translucent`가
28,963,293개 인덱스, 115,853,172 B를 차지한다. 전체 인덱스의 31.60%다.
그 다음은 QueenForest 06/05/02이며 각각 약 3.04M/2.55M/2.53M 인덱스다.
전체 기능을 한 번에 바꾸기 전에 Banyan 단독 A/B가 가장 높은 신호를 준다.

## 권장 구현 순서

### 1. 낮은 위험: 파일 코덱

1. FJSCENE의 raw `indices` vector 대신 unique `(index_offset, index_count)` range별
   codec descriptor와 encoded byte stream을 추가한다.
2. corner 순서 계약이 불명확한 동안은 sequence codec을 기본으로 한다.
3. 전체 로딩이면 한 zstd frame, 부분 스트리밍이면 mesh 또는 mesh-LOD 그룹별 frame을 쓴다.
4. 로더에서 zstd 해제 후 meshopt decode하여 현재 R32 upload buffer를 만든다.
5. 기존 validation에 인덱스 범위, triangle count, exact/cyclic 정책 검증을 추가한다.

### 2. 중간 위험: Banyan all-R16 프로토타입

1. Banyan의 LOD 공유 vertex range를 57,344 목표/65,536 상한으로 분할한다.
2. 모든 LOD가 동일한 vertex chunk를 공유하고 경계 정점만 복제하도록 cooker를 바꾼다.
3. 각 LOD의 active chunk별 SubMesh를 만들고 R16 index buffer를 생성한다.
4. R32/R16 혼합보다 Banyan만 별도 R16 buffer/pass로 먼저 A/B한다.
5. VRAM, cook time, load time, frame GPU time, indirect command count, culling time을 기록한다.

### 3. 확장 판단

- Banyan에서 GPU 시간이 유지되면 나머지 111개 blocker vertex range로 확대한다.
- draw 증가가 병목이면 topology-aware graph partition 또는 meshlet 경로를 검토한다.
- custom block bitpack은 현재 IA에서 직접 쓸 수 없으므로 mesh shader/compute predecode
  전환 계획이 있을 때만 고려한다. load-time 압축만 필요하면 meshoptimizer가 더 작고 단순하다.

## 산출물

- `summary.csv`: 전체 카운트와 핵심 합계
- `candidates.csv`: 압축 후보별 크기
- `submeshes.csv`: 1,460개 서브메시 상세 통계
- `meshes.csv`: 모델별 집계
- `lod_totals.csv`: LOD별 집계
- `distributions.csv`: 델타, 삼각형 공유, 삼각형 span 분포
- `general_compression.csv`: LZ4/zstd/zlib 크기와 처리 속도
- `shared_lod_partitions.csv`: 57,344 목표의 LOD 공유 청크 상세
- `shared_lod_partition_sweep.csv`: 목표 청크 크기 스윕

최종 CSV와 보고서만 남겼으며, 계측 소스·실행 파일·임시 압축 스트림은 제거한다.
