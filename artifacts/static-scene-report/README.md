# Cooked StaticScene quantitative report

분석 대상: `C:/Users/mkkim/myfolder/mydev1/fast-jungle-renderer/assets/cooked/JungleRuins.fjscene`  
분석 경계: cooked `StaticScene` 데이터와 companion texture payload 크기/metadata만 사용. `SceneResources`와 renderer 파생 데이터는 포함하지 않음.

## 핵심 관찰

- Point instance는 **8,674,676개**, PointBatch는 **778개**다. 배치당 instance 중앙값은 **9,194개**, p90은 **22,336개**, 최대는 **78,914개**다.
- instance의 50%를 상위 **140개 batch**, 90%를 상위 **437개 batch**가 차지한다.
- 가장 큰 component는 **River Forest**로 **2,407,967개 instance / 195개 batch**다.
- 기존 batch의 XZ 대각선 중앙값은 **1,412.6m**, p90은 **4,284.3m**다. source batch 자체는 공간 culling 단위로 보기에는 매우 넓은 경우가 많다.
- 전체 배치 instance anchor 범위는 X **-3,488.0..4,512.0m**, Y **-88.2..411.2m**, Z **-3,488.0..4,512.0m**다.
- 고유 mesh의 총 triangle은 LOD0 **15,555,338**, LOD1 **6,200,083 (39.9%)**, LOD2 **2,632,831 (16.9%)**, LOD3 **669,131 (4.3%)**다.
- cooker indexing은 triangle corner **46,666,014개**를 unique vertex **32,264,161개**로 줄였다(**30.9% 감소**).
- texture는 **243개**, payload는 **2.41 GiB**다. 가장 흔한 해상도는 **2048×2048 (177개)**다.

## 렌더러 설계에 바로 볼 그림

- `02`, `03`, `04`: batch 크기와 부하 집중도 — draw/dispatch 묶음 크기 후보를 판단.
- `05`, `06`, `07`: instance 공간 분포와 현재 batch의 실제 공간 폭 — source batch와 culling cluster를 분리할 필요를 판단.
- `08`: 8~1024m regular grid의 cell 수, cell occupancy, source-batch fragmentation — spatial bin 크기 탐색.
- `11`, `12`, `13`, `14`: mesh/LOD 복잡도와 잠재 triangle work — LOD 선택 및 instance compaction 우선순위 판단.
- `15`, `16`, `17`: texture/material/auxiliary payload — streaming, pass 분리, 정적 메모리 예산 판단.

CSV와 `summary.json`에는 그림에 축약된 원시 집계값을 남겼다. 모든 공간 plot은 PointInstance position에 PointBatch `local_to_world`를 적용한 instance anchor 기준이다. prototype local transform과 renderer가 계산하는 bounds는 의도적으로 사용하지 않았다.
